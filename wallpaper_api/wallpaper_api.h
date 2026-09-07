// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WALLPAPER_API_WALLPAPER_API_H_
#define WALLPAPER_API_WALLPAPER_API_H_

#include "extensions/browser/extension_function.h"

// Implements the chrome.wallpaper.getColor() extension API function.
// Fetches the current wallpaper color palette from the native
// WallpaperManager via Mojo IPC and returns it to the calling
// extension as a WallpaperColors dictionary.
class WallpaperGetColorFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaper.getColor", WALLPAPER_GET_COLOR)

 protected:
  ~WallpaperGetColorFunction() override;

  ResponseAction Run() override;
};

#endif  // WALLPAPER_API_WALLPAPER_API_H_
