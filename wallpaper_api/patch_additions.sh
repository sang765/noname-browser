#!/bin/bash
# Wallpaper API — copy new files into Chromium source tree.
# Sourced from the Chromium source root (same context as patch.sh).

# Mojo interface: third_party/chromium_api/
mkdir -p third_party/chromium_api
cp $SCRIPT_DIR/wallpaper_api/wallpaper_manager.mojom third_party/chromium_api/

# Blink renderer: third_party/blink/renderer/modules/wallpaper/
mkdir -p third_party/blink/renderer/modules/wallpaper
cp $SCRIPT_DIR/wallpaper_api/navigator_wallpaper.idl third_party/blink/renderer/modules/wallpaper/
cp $SCRIPT_DIR/wallpaper_api/navigator_wallpaper.cc third_party/blink/renderer/modules/wallpaper/
cp $SCRIPT_DIR/wallpaper_api/navigator_wallpaper.h third_party/blink/renderer/modules/wallpaper/

# Browser extension API: chrome/browser/extensions/api/wallpaper/
mkdir -p chrome/browser/extensions/api/wallpaper
cp $SCRIPT_DIR/wallpaper_api/wallpaper_api.cc chrome/browser/extensions/api/wallpaper/
cp $SCRIPT_DIR/wallpaper_api/wallpaper_api.h chrome/browser/extensions/api/wallpaper/
cp $SCRIPT_DIR/wallpaper_api/wallpaper_event.cc chrome/browser/extensions/api/wallpaper/
cp $SCRIPT_DIR/wallpaper_api/wallpaper_event.h chrome/browser/extensions/api/wallpaper/
cp $SCRIPT_DIR/wallpaper_api/wallpaper_manager_impl.cc chrome/browser/extensions/api/wallpaper/
cp $SCRIPT_DIR/wallpaper_api/wallpaper_manager_impl.h chrome/browser/extensions/api/wallpaper/

# Material You color algorithms: base/material_color/
mkdir -p base/material_color
cp $SCRIPT_DIR/wallpaper_api/material_color/dynamic_scheme.cc base/material_color/
cp $SCRIPT_DIR/wallpaper_api/material_color/dynamic_scheme.h base/material_color/
cp $SCRIPT_DIR/wallpaper_api/material_color/hct.cc base/material_color/
cp $SCRIPT_DIR/wallpaper_api/material_color/hct.h base/material_color/

# Extension API schema: chrome/common/extensions/api/
mkdir -p chrome/common/extensions/api
cp $SCRIPT_DIR/wallpaper_api/chrome_wallpaper.json chrome/common/extensions/api/

# BUILD.gn patches — register new files with the build system.

# Mojo interface: third_party/chromium_api/BUILD.gn
# Adds wallpaper_manager.mojom to the mojom sources list.
sed -i 's|mojom("mojom") {|&\n  sources += [\n    "wallpaper_manager.mojom",\n  ],|' third_party/chromium_api/BUILD.gn

# Blink module: third_party/blink/renderer/modules/BUILD.gn
# Adds wallpaper module source files to the modules sources list.
sed -i 's|"dom/dom.cc",|&\n  "wallpaper/navigator_wallpaper.cc",\n  "wallpaper/navigator_wallpaper.h",\n  "wallpaper/navigator_wallpaper.idl",|' third_party/blink/renderer/modules/BUILD.gn

# Browser implementation: chrome/browser/extensions/api/BUILD.gn
# Adds wallpaper API source files to the extensions API sources list.
sed -i 's|"tabs/tabs_api.cc",|&\n  "wallpaper/wallpaper_api.cc",\n  "wallpaper/wallpaper_api.h",\n  "wallpaper/wallpaper_event.cc",\n  "wallpaper/wallpaper_event.h",\n  "wallpaper/wallpaper_manager_impl.cc",\n  "wallpaper/wallpaper_manager_impl.h",|' chrome/browser/extensions/api/BUILD.gn

# Extension schema: chrome/common/extensions/api/api_sources.gni
# Adds chrome_wallpaper.json to the uncompiled API sources list.
sed -i 's|uncompiled_sources_ = \[|&\n  "chrome_wallpaper.json",|' chrome/common/extensions/api/api_sources.gni
