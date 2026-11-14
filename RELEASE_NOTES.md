**Spirited Drive** 0.9.1: *The Need for Speed* Special Edition (1996), rebuilt from scratch to run natively on today's Macs. This release brings sound in the rain and snow, records kept the way you race, fewer and clearer settings, and a round of fixes from your reports.

You need your own copy of the game: the PC Special Edition CD, a folder with its files, or an ISO image of your own disc. Spirited Drive contains no EA content.

## Install

1. Download `Spirited-Drive-0.9.1-macos.dmg` below. One app for Apple Silicon and Intel, macOS 11 or later.
2. Open it and drag **Spirited Drive** to Applications. **Updating from 0.9.0?** Replace the old app: your settings, records and the copied game files are kept.
3. **The first time only**, macOS will say it cannot check the app: the app is not notarized by Apple. Right-click the app, choose **Open**, then **Open** again. On recent macOS: try to open it once, then go to **System Settings → Privacy & Security** and click **Open Anyway**.
4. On first start, point it at your game. It copies what it needs, and you can put the CD away.

## New in 0.9.1

- **Rain and snow you can hear** (Restomod): rain on the car when you stop, outside or on the roof in the cockpit, and on the move the rain around the car and the hiss of wet tyres, rising with your speed. Every car throws up its own spray and splashes through puddles, yours, the rivals' and the traffic's, each heard from where it is, quieter the farther it is. The rain sits under the engine without distorting it, and it goes quiet at once when you pause or open the race menu, with the thunder and the snow. Snow on asphalt brings the wet spray of meltwater; the snowy rally stage has its own sound of tyres in snow. All new clips are CC0.
- **Records in Restomod**: a table per track and layout (normal, mirror, reverse, rally), per cheats and per lap count, with the best lap shared by every lap count. Every time notes its weather, time of day and difficulty, and every race mode counts. **Best Times** lets you pick the layout, cheats, laps and page (Top 10, Best Lap, Segments), and Track Select lets you set your own lap count. The first start brings in your existing records and leaves the old files as they were; Stock and Restored keep the original's records.
- **Fewer, clearer settings**: one switch per effect instead of pairs and steps that did the same thing. Your old settings files still load.
- **Customize Game**: a cleaner page 3 (Restore profile defaults set apart), a **Star us on GitHub** row, and the selection now follows the mouse or the keys, never both: a click acts only on what is under the pointer, and the page arrows go back and forward as they show.
- **Changing mode takes effect at once**: switching between Stock, Restored and Restomod, or changing one setting, reaches everything without a restart.
- **Restomod**: traffic headlight halos, rally stage lamps in the dry and the snow effects are on by default.
- **Cameras and cockpit**: the chase camera no longer shakes the car about on bumpy and banked roads; the wipers turn on either side of the steering wheel; the widened rear-view mirror's frame is whole.
- **Start-up**: the Spirited Drive screen comes first, and the press that skips EA's logo no longer skips the Road & Track title too.
- **HUD**: the green start lights no longer stay under the numbers when you change view as the race starts.
- **Mac icon**: the same size as your other apps.
- **Update check and anonymous stats** now say which system you play on (macOS, Windows or Linux); the API works out the country from the connection. Only daily totals are kept: no IP, no ID. Turn them off in **Customize Game**, page 3.

## Known issues

- The app is not notarized: macOS asks you to allow it the first time (see Install).
- Only the Mac app is packaged and tested. The code also targets Linux and Windows; you can build it from source ([docs/building.md](https://github.com/BytesNRust/spirited-drive/blob/main/docs/building.md)), but it is untested there.
- Network races have been tested between Macs only.
- On one Mac the menu music went quiet as Control Central opened. The log now records every stop and fade of the music: if it happens to you, please send us `spirited-drive.log`.

## Found a bug?

[Open an issue](https://github.com/BytesNRust/spirited-drive/issues) and attach `spirited-drive.log` (**Customize Game**, page 3, **Open the log folder**). Please don't attach files from the original game.

`SHA256SUMS.txt` lists the DMG's checksum.

Spirited Drive is by Bytes & Rust, under the GPL v3. Not affiliated with or endorsed by Electronic Arts.
