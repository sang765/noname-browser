// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wallpaper_api/navigator_wallpaper.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_wallpaper_colors.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "wallpaper_manager.mojom-blink.h"

namespace blink {

// Supplement key: one NavigatorWallpaper per Navigator.
const char NavigatorWallpaper::kSupplementName[] = "NavigatorWallpaper";

// ---------------------------------------------------------------------------
// Supplement plumbing
// ---------------------------------------------------------------------------

NavigatorWallpaper& NavigatorWallpaper::From(Navigator& navigator) {
  NavigatorWallpaper* supplement =
      Supplement<Navigator>::From<NavigatorWallpaper>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorWallpaper>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// ---------------------------------------------------------------------------
// Construction / lifecycle
// ---------------------------------------------------------------------------

NavigatorWallpaper::NavigatorWallpaper(Navigator& navigator)
    : EventTargetWithInlineData(navigator.GetExecutionContext()),
      Supplement<Navigator>(navigator),
      ExecutionContextLifecycleObserver(navigator.GetExecutionContext()) {
  EnsureConnection();
}

NavigatorWallpaper::~NavigatorWallpaper() = default;

void NavigatorWallpaper::ContextDestroyed() {
  wallpaper_manager_.reset();
  observer_receiver_.reset();
}

void NavigatorWallpaper::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

// ---------------------------------------------------------------------------
// EventTarget
// ---------------------------------------------------------------------------

const AtomicString& NavigatorWallpaper::InterfaceName() const {
  DEFINE_STATIC_LOCAL(const AtomicString, name, ("NavigatorWallpaper"));
  return name;
}

ExecutionContext* NavigatorWallpaper::GetExecutionContext() const {
  return GetSupplementable()->GetExecutionContext();
}

// ---------------------------------------------------------------------------
// Mojo connection
// ---------------------------------------------------------------------------

void NavigatorWallpaper::EnsureConnection() {
  if (wallpaper_manager_.is_bound())
    return;

  auto* frame = GetSupplementable()->GetFrame();
  if (!frame)
    return;

  frame->GetBrowserInterfaceBroker().GetInterface(
      wallpaper_manager_.BindNewPipeAndPassReceiver(
          GetSupplementable()->GetExecutionContext()->GetTaskRunner(
              TaskType::kMiscPlatformAPI)));

  // Register as a color-change observer so we can fire "change" events.
  wallpaper_manager_->AddColorChangeObserver(
      observer_receiver_.BindNewPipeAndPassRemote(
          GetSupplementable()->GetExecutionContext()->GetTaskRunner(
              TaskType::kMiscPlatformAPI)));
}

// ---------------------------------------------------------------------------
// WebIDL: getColor()
// ---------------------------------------------------------------------------

ScriptPromise NavigatorWallpaper::getColor(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  EnsureConnection();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!wallpaper_manager_.is_bound()) {
    resolver->Reject(exception_state);
    return promise;
  }

  wallpaper_manager_->GetWallpaperColors(
      base::BindOnce(&NavigatorWallpaper::OnGetWallpaperColors,
                     weak_factory_.GetWeakPtr(), resolver));
  return promise;
}

void NavigatorWallpaper::OnGetWallpaperColors(
    ScriptPromiseResolver* resolver,
    wallpaper::mojom::blink::WallpaperColorsPtr colors) {
  if (!resolver || !resolver->GetScriptState()->IsContextValid())
    return;

  if (!colors) {
    // Wallpaper colors unavailable: resolve with null.
    resolver->Resolve();
    return;
  }

  // Build the WallpaperColors dictionary for JS.
  auto* wallpaper_colors = MakeGarbageCollected<WallpaperColors>();
  wallpaper_colors->setPrimaryColor(colors->primary_color);
  if (colors->secondary_color) {
    wallpaper_colors->setSecondaryColor(*colors->secondary_color);
  }
  if (colors->tertiary_color) {
    wallpaper_colors->setTertiaryColor(*colors->tertiary_color);
  }
  wallpaper_colors->setColorHints(colors->color_hints);

  resolver->Resolve(wallpaper_colors);
}

// ---------------------------------------------------------------------------
// WallpaperColorObserver callback
// ---------------------------------------------------------------------------

void NavigatorWallpaper::OnWallpaperColorsChanged(
    wallpaper::mojom::blink::WallpaperColorsPtr colors) {
  DispatchChangeEvent(std::move(colors));
}

// ---------------------------------------------------------------------------
// Event dispatch
// ---------------------------------------------------------------------------

void NavigatorWallpaper::DispatchChangeEvent(
    wallpaper::mojom::blink::WallpaperColorsPtr colors) {
  if (!GetExecutionContext())
    return;

  // Standard 'change' event — listeners call navigator.wallpaper.getColor()
  // to retrieve the updated colors.
  auto* event = Event::Create(event_type_names::kChange);
  event->SetTarget(this);
  DispatchEvent(*event);
}

}  // namespace blink
