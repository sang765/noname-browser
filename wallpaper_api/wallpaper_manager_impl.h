// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WALLPAPER_API_WALLPAPER_MANAGER_IMPL_H_
#define WALLPAPER_API_WALLPAPER_MANAGER_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "wallpaper_api/wallpaper_manager.mojom.h"

namespace wallpaper {

class WallpaperManagerImpl : public mojom::WallpaperManager {
 public:
  WallpaperManagerImpl();
  ~WallpaperManagerImpl() override;

  WallpaperManagerImpl(const WallpaperManagerImpl&) = delete;
  WallpaperManagerImpl& operator=(const WallpaperManagerImpl&) = delete;

  // Creates a new instance bound to the given receiver.
  static void Create(mojo::PendingReceiver<mojom::WallpaperManager> receiver);

  // JNI entry point: called from Java when wallpaper colors change.
  // Not part of the Mojo API surface — accessible for JNI linkage only.
  void OnWallpaperColorsChanged(
      JNIEnv* env,
      int primary_color,
      int secondary_color,
      int tertiary_color,
      int color_hints);

 private:
  // mojom::WallpaperManager:
  void GetWallpaperColors(GetWallpaperColorsCallback callback) override;
  void AddColorChangeObserver(
      mojo::PendingRemote<mojom::WallpaperColorObserver> observer) override;

  // Mojo receiver for the WallpaperManager interface.
  mojo::Receiver<mojom::WallpaperManager> receiver_{this};

  // Java-side WallpaperHelper instance.
  base::android::ScopedJavaGlobalRef<jobject> java_wallpaper_helper_;

  // Remote observers for color change notifications.
  mojo::RemoteSet<mojom::WallpaperColorObserver> observers_;

  // Weak pointer factory for preventing use-after-free in callbacks.
  base::WeakPtrFactory<WallpaperManagerImpl> weak_factory_{this};
};

}  // namespace wallpaper

#endif  // WALLPAPER_API_WALLPAPER_MANAGER_IMPL_H_
