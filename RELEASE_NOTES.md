<!-- DRAFT: the next version's notes from notes/textos-anuncio-2026-11-25.md (section 4), not approved as final. [X.Y.Z] is the owner's to set; [WIN] waits for the Windows package. Do not release with these marks in. -->
**Spirited Drive** [X.Y.Z]: *The Need for Speed* Special Edition (1996), rebuilt from scratch
to run natively on today's computers. Now on Windows as well as macOS.

You need your own copy of the game: the PC Special Edition CD, a folder with its files, or an
ISO image of your own disc. Spirited Drive contains no EA content.

## Three ways to play

- **Stock '96**: the 1996 game as it shipped. 4:3, original menus, only its bugs fixed.
- **Restored '96**: the same game on a modern screen. Widescreen, up to 144 fps.
- **Restomod '26**: the game it could have been with a few more years of development. Weather,
  four times of day, night lighting, a full 360° camera, cars that shine and change colour, a dashboard that lights
  up, mirror, reverse and rally versions of the tracks, the lost moon track, and dozens of
  smaller touches. All of it still 8-bit, palette-indexed art.

The first start asks which one you want. Not sure? Take Restored '96. You can change it at any
time on the Game Mode screen.

## Install

**macOS** (11 or later, Apple Silicon and Intel)
1. Download `Spirited-Drive-[X.Y.Z]-macos.dmg`, open it and drag **Spirited Drive** to Applications.
2. The first time, macOS will say it cannot check the app. Right-click the app, **Open**, then
   **Open** again; or System Settings → Privacy & Security → **Open Anyway**.

**Windows 10 or 11, 64-bit** [WIN: file name, steps]
1. Download `[WIN]`, unzip it anywhere and run **Spirited Drive**.
2. The first time, Windows will warn that the app is from an unknown publisher: **More info**,
   then **Run anyway**.

On first start, point the game at your CD, folder or ISO. It copies what it needs.

**Updating?** On macOS, replace the old app. On Windows, delete the old Spirited Drive folder and unzip the new one in its place; do not unzip over it, because files a version removed would stay. Your profile, settings, records, mods and copied game files are kept (on Windows they are in `%APPDATA%\Spirited Drive`, which is not touched).

## New in 0.9.2

- **Windows.** Spirited Drive now runs on Windows 10 and 11 (64-bit): unzip it and run it. It works from a user folder with any name, in any alphabet. [NOCHE: fix/windows-gui-exe] No console window opens beside the game. [NOCHE: fix/windows-fullscreen] Leaving full screen gives you a window that fits above the taskbar. [NOCHE: fix/windows-network-closed] There are no network races on Windows yet: Multiplayer is dimmed and says why.
- **First start**: the game asks which way you want to play, Stock '96, Restored '96 or Restomod '26, and says what each one changes. Restored '96 is the one it suggests. You can change it at any time on the Game Mode screen, and your answer is now kept on a machine that never had the game before.
- **Stock '96 and Restored '96 look like the original again**: Stock keeps the black band under the outside view, as in 1996. Restored's movies keep their 4:3 frame. The things we added (the SETUP and RECORDS tiles of the car showcase, the variant's name on the track screen, the stats overlay) now show in Restomod only.
- **Game Mode screen**: Single Player and Multiplayer on top, the mode below them, Exit at the foot, and the update notice on the version's line. The picture now fills the screen by default (Scaling: Fill); a choice you saved stays.
- **Full screen with a key** [NOCHE: fix/fullscreen-key]: Alt+Enter on Windows, Cmd+Ctrl+F on macOS, in a menu or in a race.
- **Shadows cast by the sun** (Restomod): on by default at dawn, by day and at dusk, with the sun where each track's sky has it. A circuit run in reverse is lit the way it runs: its tunnels, lamps and shadows are where the road is.
- **Tunnels** (Restomod): the light fades in and out at a tunnel's mouth instead of changing in one step, by day and at night. Your headlights light a tunnel that has no lamps of its own. The tunnel lamps' halos no longer blink, the car no longer flickers under them, and Lost Vegas' lamps stay on.
- **Lights at dawn and dusk** (Restomod): the reflectors catch your beams in low light, in tunnels and in fog, not only at night. The traffic's headlight halos show at dawn and dusk as the racers' do, and the dashboard follows the track's light at those hours as the bodywork does.
- **Brake lights and shine, painted car by car** (Restomod): a car with hand-painted masks lights only its real brake lamps, and its shine has soft edges and no bright seams. [NOCHE: owner/masks-paints-2026-11-25] New masks for eight more cars, traffic and police included, and a fifth colour for the Corvette ZR-1: Torch Red.
- **Paint** (Restomod): the 911 Carrera takes its whole colour instead of patches of it, and a few stray pixels no longer shift the paint of a whole body (the 512TR in white). [NOCHE: fix/f512tr-rear-strip-paint] The 512TR's rear strip takes the colour you pick.
- **Turn around: the world is all there** (Restomod): the 360 camera follows the car all the way round, with the road behind you drawn, and **Look back** (B, or LB on a pad) shows who is on your tail.
- **Lunar Springs is a track of its own** (Restomod): its own card in Control Central, its own track screen and Best Times with the moon's art, and its own records. A replay plays back with the moon and the gravity its race had.
- **Records per car** (Restomod): every race keeps its records by car too. The showcase's Video tile turns to **RECORDS**: that car's best times and top speed on every track. A circuit's best time now says what it is, one lap or the laps of a race. CAR SETUP is laid out in two columns, a click lands on the row you see, and ABS and traction control are offered only on the cars that have them.
- **Mouse and pads** [NOCHE: fix/mouse-tap-latch, fix/windows-pads, fix/cursor-hidden-in-movies]: a quick tap on a touchpad counts as a click, a gamepad plugged in or pulled out mid-game is picked up, and the pointer no longer sits over the intro movies.
- **Fixes**: a short circuit race no longer shows an absurd average speed on the results (884 mph); menu text blends into the picture behind it instead of showing hard edges; the network screen leaves cleanly when the network service does not start; a guest in a network race runs the host's Lunar Springs choice.

## Always in the workshop

Restomod's car shine and colours and its track lighting are being tuned track by track and car
by car. Expect some reflections in the wrong place, paint that does not take its colour, and
tunnels or shadows that look off. The engine is solid; this is art and tuning, and it gets
better with every update. Stock '96 and Restored '96 are not affected.

## Known issues

- Neither app is signed: macOS and Windows ask you to allow it the first time (see Install).
- Network races work in all three modes, on macOS only for now: the Windows version has none
  yet. They are very beta, little tested so far.
- Linux is planned, later.

## Tell us

This game is not finished and is not meant to be: what you find shapes what comes next.
Bugs: [Open an issue](https://github.com/BytesNRust/spirited-drive/issues), with `spirited-drive.log` (Customize Game, page 3, Open the log folder).
Ideas and questions: [Discussions](https://github.com/BytesNRust/spirited-drive/discussions). Please don't attach files from the original game.
