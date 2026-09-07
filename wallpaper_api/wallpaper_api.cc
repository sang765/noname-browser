// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wallpaper_api/wallpaper_api.h"

#include <utility>

#include "base/values.h"
#include "wallpaper_api/wallpaper_manager.mojom.h"

namespace {

// Returns the WallpaperManager Mojo remote bound to the
// WallpaperManagerImpl for the given browser context.
//
// Implemented in the browser integration layer (e.g., a profile-keyed
// service or startup code that creates and owns the
// WallpaperManagerImpl instance).  Returns an unbound remote if
// the service has not been initialised for |context|.
mojo::Remote<mojom::WallpaperManager>& GetWallpaperManager(
    content::BrowserContext* context);

}  // namespace

WallpaperGetColorFunction::~WallpaperGetColorFunction() = default;

ExtensionFunction::ResponseAction WallpaperGetColorFunction::Run() {
  // chrome.wallpaper.getColor takes no arguments per the schema.

  mojo::Remote<mojom::WallpaperManager>& manager =
      GetWallpaperManager(browser_context());
  if (!manager.is_bound()) {
    return RespondNow(Error("Wallpaper service unavailable"));
  }

  // Bind the async callback with a weak pointer so the call is
  // safely cancelled if the extension unloads before the response
  // arrives.
  auto weak_self = AsWeakPtr();
  manager->GetWallpaperColors(base::BindOnce(
      [](base::WeakPtr<WallpaperGetColorFunction> self,
         mojom::WallpaperColorsPtr colors) {
        if (!self)
          return;

        base::Value::List result;
        if (colors) {
          base::Value::Dict colors_dict;
          colors_dict.Set("primaryColor",
                          static_cast<int>(colors->primary_color));
          if (colors->secondary_color) {
            colors_dict.Set("secondaryColor",
                            static_cast<int>(*colors->secondary_color));
          }
          if (colors->tertiary_color) {
            colors_dict.Set("tertiaryColor",
                            static_cast<int>(*colors->tertiary_color));
          }
          colors_dict.Set("colorHints", colors->color_hints);
          result.Append(std::move(colors_dict));
        } else {
          // Wallpaper colors unavailable — return null.
          result.Append(base::Value());
        }

        self->SendResponse(true, std::move(result));
      },
      std::move(weak_self)));
  return RespondLater();
}
