# Sounds of the port

Only CC0 or public domain samples go here, each with its source, author and licence.

## thunder.wav

- What: the storm's thunder (`weather=storm` with `rain_v2`), mono, 16-bit, 22050 Hz, 13.4 s.
- Made from: "rain long thunder" (`rain-thunder.ogg`), seconds 20.6 to 34.0 of the file, down-mixed to mono, resampled to 22050 Hz, with a 50 ms fade in and a 2.2 s fade out. Nothing else changed.
- Source: https://opengameart.org/content/rain-long-thunder
- Author: WuxiaScrub (https://opengameart.org/users/wuxiascrub)
- Licence: CC0 1.0 Universal (public domain dedication), https://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-06: the licence listed is CC0, the file is the one above.

## rain_stopped_out.wav

- What: the rain round a stopped car, heard outside (`rain_tyre_roll`, with `rain_v2`), a seamless loop, mono, 16-bit, 22050 Hz, 26.7 s. It gives way to the wet-road roll as the car moves off.
- Made from: "Rain (loopable)", `1.ogg` of the page's "Rain OGG.zip" (27.0 s, the loop the page's preview plays), down-mixed to mono, resampled to 22050 Hz, and looped: the last 0.3 s is cross-faded (equal power) into the first 0.3 s so the end meets the start; the level is set to -18 dBFS RMS, the few peaks above 0.3 of full scale rounded softly (tanh) under half scale. Nothing else changed.
- Source: https://opengameart.org/content/rain-loopable
- Author: Ylmir (https://opengameart.org/users/ylmir)
- Licence: Creative Commons 0 (CC0 1.0 Universal, public domain dedication), https://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-13: the licence listed is CC0, the file is the one above.

## rain_stopped_in.wav

- What: the rain on the roof of a stopped car, heard in the cockpit (`rain_tyre_roll`, with `rain_v2`), a seamless loop, mono, 16-bit, 22050 Hz, 22.5 s. It gives way to the wet-road roll as the car moves off.
- Made from: "Ambiance_Rain_Inside_Car_Close_Roof_Loop_Stereo.wav" (the page's `-hq.mp3` preview, https://cdn.freesound.org/previews/535/535870_9250976-hq.mp3, 23.0 s), down-mixed to mono, resampled to 22050 Hz, and looped: the last 0.5 s is cross-faded (equal power) into the first 0.5 s; the level is set to -22 dBFS RMS (the drops on the roof are sharp: -18 would flatten them), the few peaks above 0.3 of full scale rounded softly (tanh) under half scale. Nothing else changed.
- Source: https://freesound.org/people/Nox_Sound/sounds/535870/
- Author: Nox_Sound (https://freesound.org/people/Nox_Sound/), uploaded 2020-09-19
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-13: the licence listed is Creative Commons 0.

## rain_tyre_roll.wav

- What: the wet road under the tyres of a moving car in rain or falling snow (`rain_tyre_roll`, with `rain_v2`), a seamless loop, mono, 16-bit, 22050 Hz, 16.00 s. Its level and pitch rise with speed; it is 3 dB lower in the cockpit, lower the farther the chase camera, and lower still under a rally's gravel roll.
- Made from: "cars passing close by wet road.WAV" (the page's HQ mp3 preview, 113.2 s), the spray of real cars passing on a wet road. Decoded to mono at 44.1 kHz; a high-pass at 500 Hz and a low-pass at 12 kHz (Butterworth, order 8, zero-phase). Five stretches where the tyres' spray is louder than the engines (1-8 kHz at least 2 dB over 60-400 Hz) are kept: seconds 12.00-18.25, 18.75-19.75, 23.25-25.25, 36.75-37.50 and 94.75-96.00, each brought to the same level, with its few clicks above 2.8 times that level softened over 1 ms. From them, 23 grains of 0.6 to 1.6 s, drawn at random (seed 20261115), each within ±1.5 dB, follow each other round a 16 s ring with equal-power cross-fades of 40-50 % of the shorter grain, the last one fading into the first so the loop has no seam; the slow level (a 0.35 s Gaussian) is flattened; a high shelf of +3 dB at 4 kHz. Below 400 Hz the sound is kept steady (flattened on its own, its average level kept); above it, a puddle layer (seed 20261116) adds five swells of +2.2 to +3.8 dB (held 1.3 to 2.6 s) and eight short dips of 3 to 6 dB (0.15 to 0.4 s), never silent. The level set to -18 dBFS RMS, no limiter (peaks -3.5 dBFS). Then resampled to 22050 Hz as one period (FFT, so the loop stays seamless) and its RMS kept at -18 dBFS, the level of the clip it replaced. Nothing else changed.
- Source: https://freesound.org/people/goose278/sounds/174820/
- Author: goose278 (https://freesound.org/people/goose278/), uploaded 2013-01-19
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-13: the licence listed is Creative Commons 0.

## snow_roll.wav

- What: the roll of a car on the snowy rally's track (Alpine 3), a seamless loop, mono, 16-bit, 22050 Hz, 5.3 s. Silent when the car stands; its level and pitch rise steeply with speed, like a friction; 6 dB lower in the cockpit.
- Made from: "sled inuit drag across snow.wav" (the page's preview), down-mixed to mono at 22050 Hz: the four drag stretches, seconds 2.45 to 4.30, 5.20 to 6.80, 0.20 to 1.60 and 7.70 to 9.05, joined in that order with 0.2 s cross-fades; a high-pass at 180 Hz and a low-pass at 9 kHz; looped with a 0.3 s equal-power cross-fade of the end into the start; the level set to -18 dBFS RMS with the peak at half scale. Nothing else changed.
- Source: https://freesound.org/people/kyles/sounds/450847/
- Author: kyles (https://freesound.org/people/kyles/), uploaded 2018-12-06
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-13: the licence listed is Creative Commons 0.
