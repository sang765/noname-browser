// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WALLPAPER_API_NAVIGATOR_WALLPAPER_H_
#define WALLPAPER_API_NAVIGATOR_WALLPAPER_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/supplement.h"
#include "wallpaper_manager.mojom-blink-forward.h"

namespace blink {

class Navigator;
class ScriptPromiseResolver;

// Blink-side implementation of navigator.wallpaper.
//
// Exposed to all web pages.  Connects to the browser-process
// WallpaperManager via the execution-context interface broker and
// exposes:
//   navigator.wallpaper.getColor() -> Promise<WallpaperColors?>
//   'change' event when the device wallpaper palette updates.
//
// The class also implements WallpaperColorObserver to receive change
// notifications from the browser process.
class CORE_EXPORT NavigatorWallpaper final
    : public EventTargetWithInlineData,
      public Supplement<Navigator>,
      public ExecutionContextLifecycleObserver,
      public wallpaper::mojom::blink::WallpaperColorObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  static NavigatorWallpaper& From(Navigator&);

  // WebIDL: navigator.wallpaper.getColor()
  ScriptPromise getColor(ScriptState*, ExceptionState&);

  // ExecutionContextLifecycleObserver.
  void ContextDestroyed() override;

  // EventTarget — returns the DOM interface name.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // wallpaper::mojom::blink::WallpaperColorObserver.
  void OnWallpaperColorsChanged(
      wallpaper::mojom::blink::WallpaperColorsPtr colors) override;

  void Trace(Visitor*) const override;

 private:
  explicit NavigatorWallpaper(Navigator&);
  ~NavigatorWallpaper() override;

  // Lazily connects to the WallpaperManager Mojo service.
  void EnsureConnection();

  // Mojo callback for GetWallpaperColors().
  void OnGetWallpaperColors(
      ScriptPromiseResolver* resolver,
      wallpaper::mojom::blink::WallpaperColorsPtr colors);

  // Dispatches a 'change' event to registered listeners.
  void DispatchChangeEvent(
      wallpaper::mojom::blink::WallpaperColorsPtr colors);

  // Mojo connection to browser-process WallpaperManager.
  mojo::Remote<wallpaper::mojom::blink::WallpaperManager> wallpaper_manager_;

  // Receives OnWallpaperColorsChanged from the browser process.
  mojo::Receiver<wallpaper::mojom::blink::WallpaperColorObserver>
      observer_receiver_{this};

  base::WeakPtrFactory<NavigatorWallpaper> weak_factory_{this};
};

}  // namespace blink

#endif  // WALLPAPER_API_NAVIGATOR_WALLPAPER_H_
