The first public beta of **Spirited Drive**: *The Need for Speed* Special Edition (1996), rebuilt from scratch to run natively on today's Macs.

You need your own copy of the game: the PC Special Edition CD, a folder with its files, or an ISO image of your own disc. Spirited Drive contains no EA content.

## Install

1. Download `Spirited-Drive-0.9.0-beta.1-macos.dmg` below. One app for Apple Silicon and Intel, macOS 11 or later.
2. Open it and drag **Spirited Drive** to Applications.
3. **The first time only**, macOS will say it cannot check the app: this beta is not notarized by Apple. Right-click the app, choose **Open**, then **Open** again. On recent macOS: try to open it once, then go to **System Settings → Privacy & Security** and click **Open Anyway**.
4. On first start, point it at your game. It copies what it needs, and you can put the CD away.

## In this beta

- **Three ways to play**: **Stock '96** (the 1996 game, bugs fixed), **Restored '96** (the same look, widescreen, up to 144 Hz) and **Restomod '26** (the full set of improvements). Switch any time from Game Mode; each keeps its own settings.
- **Wide and sharp**: 16:9 and 21:9, 60 / 120 / 144 Hz, 2x and 3x render scale, field of view, view distance up to Ultra.
- **Night and weather**: lamp halos and light pools, headlight beams, lit dashboards, rain with spray and wipers, snow on Alpine, gravel and dust on the rally stages.
- **Pads**: every action bindable to keys, buttons, triggers or axes, with dead zone, sensitivity and force feedback.
- **More to race**: mirror and rally versions of every track, Lunar Springs, Lost Vegas, separate records for mirror, rally and cheats.
- **Network races** over your LAN or the internet.
- **Mods** *(work in progress)*: load from a folder, without touching your game files.

## Known issues

- The app is not notarized: macOS asks you to allow it the first time (see Install).
- Only the Mac app is packaged and tested. The code also targets Linux and Windows; you can build it from source ([docs/building.md](https://github.com/BytesNRust/spirited-drive/blob/main/docs/building.md)), but it is untested there.
- Network races have been tested between Macs only.

## Found a bug?

[Open an issue](https://github.com/BytesNRust/spirited-drive/issues) and attach `spirited-drive.log` (**Customize Game**, page 3, **Open the log folder**). Please don't attach files from the original game.

`SHA256SUMS.txt` lists the DMG's checksum.

Spirited Drive is by Bytes & Rust, under the GPL v3. Not affiliated with or endorsed by Electronic Arts.
