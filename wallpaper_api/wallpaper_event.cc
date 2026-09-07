// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wallpaper_api/wallpaper_event.h"

#include <utility>

#include "base/logging.h"

namespace wallpaper {

WallpaperEventDispatcher::WallpaperEventDispatcher() = default;

WallpaperEventDispatcher::~WallpaperEventDispatcher() = default;

void WallpaperEventDispatcher::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void WallpaperEventDispatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WallpaperEventDispatcher::NotifyColorsChanged(
    mojom::WallpaperColorsPtr colors) {
  for (auto& observer : observers_) {
    observer.OnWallpaperColorsChanged(colors.Clone());
  }
}

size_t WallpaperEventDispatcher::GetObserverCount() const {
  return observers_.size();
}

}  // namespace wallpaper
