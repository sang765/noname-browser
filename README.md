# NoName Browser for Android

[![Stars](https://img.shields.io/github/stars/sang765/noname-browser?label=Stars&logo=GitHub)](https://github.com/sang765/noname-browser)
[![GitHub](https://img.shields.io/github/downloads/sang765/noname-browser/total?label=GitHub&logo=GitHub)](https://github.com/sang765/noname-browser/releases)
[![license](https://img.shields.io/badge/License-GPLv2-blue.svg)](https://github.com/sang765/noname-browser/blob/main/LICENSE)
[![build](https://img.shields.io/github/actions/workflow/status/sang765/noname-browser/build.yml)](https://github.com/sang765/noname-browser/actions/workflows/build.yml)
[![release](https://img.shields.io/github/v/release/sang765/noname-browser)](https://github.com/sang765/noname-browser/releases)

A secure and fully open-source, Chromium-based web browser with support for extensions, based on [Vanadium](https://github.com/GrapheneOS/Vanadium) by [GrapheneOS](https://github.com/GrapheneOS). To maintain a fast and native experience for everyone, advanced features are modularized into [**NoName Extension for Android**](https://github.com/sang765/noname-extension).

For the latest builds, see [**Releases**](https://github.com/sang765/noname-browser/releases/latest). You can also update between GitHub and Google Play releases seamlessly.

[<img height="48" alt="Get it on GitHub" src="https://jqssun.github.io/images/badges/github.svg">](https://github.com/sang765/noname-browser/releases/latest)

<img alt="NoName Browser for Android" src="fastlane/metadata/android/en-US/images/phoneScreenshots/1.png" />

## Usage

### Installing Extensions

For Chrome extensions, navigate to [Chrome Web Store](https://chromewebstore.google.com/), enable **Desktop site** using the menu button <kbd>⋮</kbd> in the top right corner, and proceed as normal.

For [Opera Add-ons](https://addons.opera.com/), [Microsoft Edge Add-ons](https://microsoftedge.microsoft.com/addons/), or other marketplaces, targeted User Agent modifications may be required. See [**NoName Extension for Android**](https://github.com/sang765/noname-extension) for instructions.

You can also load an unpacked extension manually by navigating to the **Manage extensions** page or [`chrome://extensions`](chrome://extensions). Enable **Developer mode**, select **Load unpacked**, and choose the folder containing the extension in the Storage Access Framework (SAF) picker. Manifest V2 (MV2) extensions are supported. It may take a moment for the extension to load.

### Using Extensions

To run an extension in Incognito (OTR) mode, go to **Manage extensions**, find the extension you want to use in Incognito mode, select **Details**, and turn on **Allow in Incognito**.

For advanced features including external download manager support, enhanced dark mode, and additional privacy options, you can use [**NoName Extension for Android**](https://github.com/sang765/noname-extension).

### Debug URLs

To view and access the debug URLs, use [`chrome://chrome-urls`](chrome://chrome-urls). For **Experiments**, use [`chrome://flags`](chrome://flags).

### WebRTC IP Policy

The option is available by using the menu button <kbd>⋮</kbd> in the top right corner, then selecting **Settings**, **Privacy and security**. If you experience issues with WebRTC due to IPs being shielded by default (e.g. [Discord Voice](https://discord.com/blog/how-discord-handles-two-and-half-million-concurrent-voice-users-using-webrtc)), try changing it to **Default public interface only**, or **Default**.

## Implementation

> [!WARNING]
> [NoName Browser for Android](#noname-browser-for-android) only attempts to improve security and privacy where possible. For better protection on Android, you should instead use [GrapheneOS](https://grapheneos.org) with [Vanadium](https://vanadium.app), which additionally integrates patches into Android System WebView and provides significant kernel and memory management hardening on the OS level.

```mermaid
---
config:
  layout: dagre
---
flowchart TD
 subgraph s1["Additional Patches"]
        n5["Feature Overrides"]
        n6["UI Overrides"]
        n7["Manifest V2 + Secure Off Store Install Support"]
        n8["Miscellaneous Fixes + Improvements"]
  end
 subgraph s2["Vanadium"]
        n9["Generic Patches<small><br>patches/*.patch</small>"]
        n10["Subprojects Patches<small><br>subprojects_patches/**/*.patch</small>"]
  end
 subgraph s3["NoName Browser for Android"]
        n11["GN Build Configuration<small><br>args.gn</small>"]
        n12["Signed Release"]
  end
    n1["Chromium"] --> s1 & s2
    n5 --> n6
    n6 --> n7
    n7 --> n8
    s1 --> s3
    s2 --> s3
    n11 --> n12
    n5@{ shape: subproc}
    n6@{ shape: subproc}
    n7@{ shape: subproc}
    n8@{ shape: subproc}
    n9@{ shape: subproc}
    n10@{ shape: subproc}
    n11@{ shape: subproc}
    n12@{ shape: subproc}
    n1@{ shape: rounded}
    classDef Aqua stroke-width:1px, stroke-dasharray:none, stroke:#46EDC8, fill:#DEFFF8, color:#378E7A
    style n5 stroke:#FF6D00
    style n7 stroke:#FF6D00
```

## Building

All releases are built using [Actions](https://github.com/sang765/noname-browser/actions). Current releases can also be attested using [GitHub CLI](https://github.com/cli/cli).

```shell
gh attestation verify *.apk -R sang765/noname-browser
```

This repository provides the build script to compile on the latest Ubuntu, and may also work with other Linux distributions.

To build these releases yourself via CI (e.g. GitHub Actions), fork this repository. Supply your `base64` encoded `keystore.jks` and `local.properties` (containing `keyAlias`, `keyPassword` and `storePassword`) to [**Repository secrets**](https://github.com/sang765/noname-browser/blob/main/.github/workflows/build.yml#L49-L50) under **Settings** > **Secrets and variables** > **Actions**. To generate a release, go to **Actions**, select **Build**, and select **Run workflow**. Under **Runner**, you can either use a GitHub-hosted runner by entering `ubuntu-latest`, or `self-hosted` for your own hardware.

## Credits

This project is a fork of [**Titanium Browser for Android**](https://github.com/jqssun/android-titanium-browser) by [jqssun](https://github.com/jqssun), which itself is based on [Vanadium](https://github.com/GrapheneOS/Vanadium) by [GrapheneOS](https://github.com/GrapheneOS). All credit goes to the original authors and contributors.

- **Vanadium** — the privacy-focused Chromium base by GrapheneOS
- **Titanium Browser** — the fork that added extension support and additional features
- **NoName Browser** — this project, continuing development with Material You color APIs and custom branding

This project is not affiliated with [Helium Browser for Linux](https://github.com/imputnet/helium-linux).

## License

GPL-2.0 — see [LICENSE](LICENSE) for details.