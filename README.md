# obs-bm-camera-control

An OBS Studio dock panel plugin for controlling Blackmagic cameras over the network using the official [Blackmagic Camera REST API](https://documents.blackmagicdesign.com/DeveloperManuals/RESTAPIforBlackmagicCameras.pdf).

---

## How It Works

Blackmagic cameras expose a REST HTTP API when the **Web Media Manager** is enabled. The plugin connects directly to that API over your local network — no Blackmagic software needs to be running during use.

Once connected, the plugin:
- **Polls the camera every 2 seconds** and updates all controls to reflect the current settings
- **Sends changes immediately** when you adjust a control
- **Skips updating** any control you are actively editing, so your input is never overwritten

The dock panel adapts to its width: single-column when docked narrow, two-column grid when floated wider.

### Supported Controls

| Section | Controls |
|---|---|
| **Exposure** | Shutter speed / angle, ISO, Gain (dB), ND filter stop, Auto exposure mode |
| **White Balance** | Kelvin slider (2000–8000 K), Tint, Auto WB, Quick presets (Sun, Tungsten, Fluorescent, Shade, Cloud) |
| **Color / Contrast** | Contrast amount, Pivot point, Hue (°), Saturation (%) |
| **Presets** | List and apply camera `.cset` preset files stored on the camera |

---

## Camera Setup

The following steps configure a **Blackmagic Micro Studio Camera 4K G2** (and similar Blackmagic cameras) to accept REST API connections. You only need to do this once per camera.

### 1. Download Blackmagic Camera Setup

Download and install **Blackmagic Camera Setup** from the Blackmagic Design support page:

> https://www.blackmagicdesign.com/support/family/studio-cameras

Install and launch the application.

### 2. Connect the Camera via USB

1. Power on the camera.
2. Connect a **USB-C cable** from the camera's USB-C expansion port to your Mac or PC.
3. Blackmagic Camera Setup will detect the camera automatically. If prompted to update firmware, do so before continuing — the REST API requires recent firmware.

### 3. Enable Web Media Manager (HTTP)

1. In Blackmagic Camera Setup, select your camera from the device list.
2. Navigate to the **Network** settings tab.
3. Check **"Web Media Manager (HTTP)"** to enable the REST API.
4. Under the IP address section, select **DHCP** (recommended) so the camera receives an address automatically from your router. Alternatively set a **Static IP** if you need a fixed, predictable address.
5. Click **Save** (or **Apply**) to write the settings to the camera.

> ⚠️ **Security note:** Once Web Media Manager is enabled, any device on the same network can control the camera settings and access or delete recorded files. Use a dedicated or isolated network for camera control in professional environments.

### 4. Generate a Self-Signed Certificate (optional — for HTTPS)

If you want encrypted connections:

1. In the **Network** settings tab, click **"Generate Certificate"**.
2. The camera creates a self-signed TLS certificate and stores it internally.
3. HTTPS access becomes available at `https://<camera-hostname>/control/api/v1/`.

> The plugin currently uses **HTTP** by default. HTTPS requires manually accepting the self-signed certificate warning in a browser first, then configuring the plugin to use `https://` in the hostname field.

### 5. Connect the Camera to Your Network via Ethernet

The Micro Studio Camera 4K G2 does not have a built-in RJ45 port — you need a **USB-C to Gigabit Ethernet adapter**:

1. Plug a **USB-C to Ethernet adapter** into the camera's USB-C expansion port.
2. Run a standard Ethernet cable from the adapter to your router or network switch.
3. The camera and the computer running OBS must be on **the same local network**.

> Recommended adapter: any USB-C to Gigabit Ethernet adapter that is plug-and-play compatible (e.g. the Belkin USB-C to Gigabit Ethernet Adapter). Check Blackmagic's compatibility notes for your specific camera model.

### 6. Find the Camera's Hostname

Blackmagic cameras advertise themselves on the local network via **mDNS** (Bonjour/Zeroconf). The hostname is derived from the camera's name:

- Spaces are replaced with **hyphens** (`-`)
- `.local` is appended

| Camera name in settings | mDNS hostname to enter in plugin |
|---|---|
| `Micro Studio Camera 4K G2` | `Micro-Studio-Camera-4K-G2.local` |
| `Studio Camera` | `Studio-Camera.local` |
| `Camera 1` | `Camera-1.local` |

You can also use the camera's **IP address directly** (e.g. `192.168.1.50`) if mDNS resolution isn't working on your network.

> On **Windows**, `.local` hostname resolution requires **Bonjour** — install it via [Apple's Bonjour Print Services](https://support.apple.com/kb/DL999) or it is included automatically with iTunes.

---

## Building the Plugin

### Prerequisites

| Tool | Version | Notes |
|---|---|---|
| **Xcode** | 14+ | macOS — full Xcode app required, not just Command Line Tools |
| **Visual Studio** | 2022 | Windows |
| **CMake** | 3.28+ | `brew install cmake` on macOS |
| **Git** | any | |

On macOS, make sure `xcode-select` points at the full Xcode app (not Command Line Tools):

```bash
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
```

### Clone

```bash
git clone https://github.com/blandreth94/obs-bm-camera-control
cd obs-bm-camera-control
```

The build system uses `buildspec.json` to automatically download the correct OBS Studio headers and pre-built Qt6 / obs-deps for your platform — no manual dependency installation required.

### macOS

```bash
# Configure — downloads OBS deps automatically
cmake --preset macos

# Build
cmake --build --preset macos
```

Output:
```
build_macos/RelWithDebInfo/obs-bm-camera-control.plugin
```

### Windows

Open a **Visual Studio 2022 Developer Command Prompt**, then:

```bat
cmake --preset windows-x64
cmake --build --preset windows-x64
```

Output:
```
build_x64\RelWithDebInfo\obs-bm-camera-control.dll
```

---

## Installing the Plugin

### macOS

```bash
# Copy the .plugin bundle into OBS's plugin directory
cp -r build_macos/RelWithDebInfo/obs-bm-camera-control.plugin \
  ~/Library/Application\ Support/obs-studio/plugins/obs-bm-camera-control.plugin

# Remove the quarantine flag — required for unsigned local builds
xattr -dr com.apple.quarantine \
  ~/Library/Application\ Support/obs-studio/plugins/obs-bm-camera-control.plugin
```

Restart OBS. The dock appears under **View → Docks → BM Camera Control**.

### Windows

Copy `obs-bm-camera-control.dll` to:

```
%APPDATA%\obs-studio\plugins\obs-bm-camera-control\bin\64bit\obs-bm-camera-control.dll
```

Restart OBS.

---

## Connecting to a Camera in OBS

1. Open the **BM Camera Control** dock — **View → Docks → BM Camera Control**.
2. Enter the camera's hostname (e.g. `Micro-Studio-Camera-4K-G2.local`) or IP address in the text field.
3. Click **Connect**.
4. The status indicator turns green and all controls populate with the camera's live settings.
5. Adjust any control — changes are sent to the camera immediately.

---

## Troubleshooting

**Connect fails immediately**
- Verify the camera and OBS machine are on the same network.
- Confirm **Web Media Manager (HTTP)** is enabled in Blackmagic Camera Setup.
- Try the camera's IP address instead of the `.local` hostname.
- On Windows, install Bonjour if `.local` names don't resolve.

**Controls show defaults and don't update**
- The poll runs every 2 seconds — wait a moment after connecting.
- Check the camera is on and not in standby/sleep mode.

**Plugin doesn't appear in OBS (macOS)**
- Confirm you ran the `xattr -dr com.apple.quarantine` command above.
- Check the OBS log for errors: `~/Library/Application Support/obs-studio/logs/`

---

## API Reference

This plugin uses the official Blackmagic REST API. All endpoints are of the form:

```
http://<hostname>/control/api/v1/<endpoint>
```

Full documentation:
> [REST API for Blackmagic Cameras — Developer Manual (PDF)](https://documents.blackmagicdesign.com/DeveloperManuals/RESTAPIforBlackmagicCameras.pdf)

---

## License

GPL-2.0 — see [LICENSE](LICENSE).
