// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WALLPAPER_API_WALLPAPER_EVENT_H_
#define WALLPAPER_API_WALLPAPER_EVENT_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "wallpaper_api/wallpaper_manager.mojom.h"

namespace wallpaper {

// Browser-process event dispatcher for wallpaper color changes.
//
// Sits between the native wallpaper change source (JNI callbacks via
// WallpaperManagerImpl) and consumers:
//
//   - Web API (navigator.wallpaper):  Blink already receives changes via
//     Mojo WallpaperColorObserver. This dispatcher provides a parallel
//     C++ observer path for browser-process code that doesn't go through
//     Mojo (e.g. extension event routing).
//
//   - Extension API (chrome.wallpaper.onChanged):  Extension event
//     functions register as observers here and forward changes to the
//     EventRouter for delivery to JS listeners.
//
// Lifetime: owned by the BrowserContext (e.g. as a keyed service or
// member of WallpaperManagerImpl). Observers must unregister themselves
// before destruction — use base::ScopedObservation for automatic cleanup.
class WallpaperEventDispatcher : public base::SupportsWeakPtr {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when wallpaper colors change. |colors| may be nullptr if
    // colors are temporarily unavailable.
    virtual void OnWallpaperColorsChanged(
        mojom::WallpaperColorsPtr colors) = 0;
  };

  WallpaperEventDispatcher();
  WallpaperEventDispatcher(const WallpaperEventDispatcher&) = delete;
  WallpaperEventDispatcher& operator=(const WallpaperEventDispatcher&) = delete;
  ~WallpaperEventDispatcher();

  // Adds an observer. Must call RemoveObserver() before destruction.
  void AddObserver(Observer* observer);

  // Removes an observer. Safe to call multiple times.
  void RemoveObserver(Observer* observer);

  // Dispatches wallpaper color change to all registered observers.
  // Called by WallpaperManagerImpl when JNI signals a change.
  void NotifyColorsChanged(mojom::WallpaperColorsPtr colors);

  // Returns the number of registered observers (for diagnostics).
  size_t GetObserverCount() const;

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace wallpaper

#endif  // WALLPAPER_API_WALLPAPER_EVENT_H_
