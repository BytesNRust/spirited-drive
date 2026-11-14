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

## rain_moving_out.wav

- What: the rain round a moving car, heard outside (`rain_tyre_roll`, with `rain_v2`), played once whatever cars there are; a seamless loop, mono, 16-bit, 22050 Hz, 12.0 s. The rain round a stopped car gives way to it as the followed car moves off.
- Made from: "Raining outside" (the page's HQ mp3 preview (128 kbps; the original needs a login), 84.8 s), decoded to mono at 22050 Hz; seconds 14.5 to 27.5 (the 13 s window whose level moves most like a real outside-rain reference), looped with a 1 s equal-power cross-fade of its end into its start (12 s). Equalised (a circular FFT filter) to the 1/3-octave curve of a reference recording of rain outside, the reference's 250 Hz hum peak capped, and below 100 Hz the curve continued down at the reference's own slope so the source's low rumble (an engine far off) is gone. Limited to -3.2 dBFS with a gain shared with rain_spray.wav. No sample of the reference is in the file. Nothing else changed.
- Source: https://freesound.org/s/518547/
- Author: TeTeNoise (https://freesound.org/people/TeTeNoise/)
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-14: the licence listed is Creative Commons 0.

## rain_moving_in.wav

- What: the rain on a moving car with the hiss of its own tyres, heard in the cockpit (`rain_tyre_roll`, with `rain_v2`); a seamless loop, mono, 16-bit, 22050 Hz, 12.0 s. The rain on the roof of a stopped car gives way to it as the car moves off; its level rises with speed.
- Made from: "Rain_insideCar" (the page's HQ mp3 preview (128 kbps; the original needs a login), 97.5 s), decoded to mono at 22050 Hz; seconds 55.5 to 68.5, looped with a 1 s equal-power cross-fade of its end into its start (12 s); equalised (a circular FFT filter) to the 1/3-octave curve of a reference recording inside a car driven in the rain; limited to -3.2 dBFS. No sample of the reference is in the file. Nothing else changed.
- Source: https://freesound.org/s/855678/
- Author: Brikaro (https://freesound.org/people/Brikaro/)
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-14: the licence listed is Creative Commons 0.

## rain_spray.wav

- What: the tyre spray of a car on a wet road (`rain_tyre_roll`, with `rain_v2`), every car's (the player's, the rivals', the traffic's) heard where the car is; a seamless loop, mono, 16-bit, 22050 Hz, 12.0 s. Its level and pitch rise with speed; lower the farther the car, lower in the cockpit, lower still under a rally's gravel roll.
- Made from: "cars passing close by wet road.WAV" (the page's HQ mp3 preview (128 kbps; the original needs a login), 113.2 s): only the stretches where the tyres' spray is louder than the engines, cut into grains and joined round a ring with equal-power cross-fades, so the loop has no seam (the steps of the clip it replaced, rain_tyre_roll.wav, the same recording); equalised to the 1/3-octave curve of a reference recording of rain outside, at 40 % of its power, and limited with the gain it shares with rain_moving_out.wav (peaks -6.9 dBFS). No sample of the reference is in the file. Nothing else changed.
- Source: https://freesound.org/people/goose278/sounds/174820/
- Author: goose278 (https://freesound.org/people/goose278/), uploaded 2013-01-19
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-14: the licence listed is Creative Commons 0.

## rain_puddle_1.wav

- What: a puddle splash thrown up by a car's tyres (`rain_tyre_roll`, with `rain_v2`), played once at random, more often the faster the car; mono, 16-bit, 22050 Hz. One of 7.
- Made from: "Water Puddle Splash" (the page's HQ mp3 preview (128 kbps; the original needs a login)), decoded to mono at 22050 Hz; the first splash; a 40 Hz high-pass, an 8 ms fade-in and a 60 ms cosine fade-out; the peak set to -4 dBFS. Nothing else changed.
- Source: https://freesound.org/s/563021/
- Author: cookies+policy (https://freesound.org/people/cookies+policy/)
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-14: the licence listed is Creative Commons 0.

## rain_puddle_2.wav

- What: a puddle splash thrown up by a car's tyres (`rain_tyre_roll`, with `rain_v2`), played once at random, more often the faster the car; mono, 16-bit, 22050 Hz. One of 7.
- Made from: "Water Puddle Splash" (the page's HQ mp3 preview (128 kbps; the original needs a login)), decoded to mono at 22050 Hz; the second splash; a 40 Hz high-pass, an 8 ms fade-in and a 60 ms cosine fade-out; the peak set to -4 dBFS. Nothing else changed.
- Source: https://freesound.org/s/563021/
- Author: cookies+policy (https://freesound.org/people/cookies+policy/)
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-14: the licence listed is Creative Commons 0.

## rain_puddle_3.wav

- What: a puddle splash thrown up by a car's tyres (`rain_tyre_roll`, with `rain_v2`), played once at random, more often the faster the car; mono, 16-bit, 22050 Hz. One of 7.
- Made from: "Small Puddle Splash" (the page's HQ mp3 preview (128 kbps; the original needs a login)), decoded to mono at 22050 Hz; the whole clip, 0.41 s; a 40 Hz high-pass, an 8 ms fade-in and a 60 ms cosine fade-out; the peak set to -4 dBFS. Nothing else changed.
- Source: https://freesound.org/s/841834/
- Author: Robo9418 (https://freesound.org/people/Robo9418/)
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-14: the licence listed is Creative Commons 0.

## rain_puddle_4.wav

- What: a puddle splash thrown up by a car's tyres (`rain_tyre_roll`, with `rain_v2`), played once at random, more often the faster the car; mono, 16-bit, 22050 Hz. One of 7.
- Made from: "water splash and flounder about in a puddle" (the page's HQ mp3 preview (128 kbps; the original needs a login)), decoded to mono at 22050 Hz; seconds 1.10 to 2.10 (the first splash); a 40 Hz high-pass, an 8 ms fade-in and a 60 ms cosine fade-out; the peak set to -4 dBFS. Nothing else changed.
- Source: https://freesound.org/s/249929/
- Author: launemax (https://freesound.org/people/launemax/)
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-14: the licence listed is Creative Commons 0.

## rain_puddle_5.wav

- What: a puddle splash thrown up by a car's tyres (`rain_tyre_roll`, with `rain_v2`), played once at random, more often the faster the car; mono, 16-bit, 22050 Hz. One of 7.
- Made from: "Water Splashes" (the page's HQ mp3 preview (128 kbps; the original needs a login)), decoded to mono at 22050 Hz; seconds 1.15 to 1.65; a 40 Hz high-pass, an 8 ms fade-in and a 60 ms cosine fade-out; the peak set to -4 dBFS. Nothing else changed.
- Source: https://freesound.org/s/207371/
- Author: Phil25 (https://freesound.org/people/Phil25/)
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-14: the licence listed is Creative Commons 0.

## rain_puddle_6.wav

- What: a puddle splash thrown up by a car's tyres (`rain_tyre_roll`, with `rain_v2`), played once at random, more often the faster the car; mono, 16-bit, 22050 Hz. One of 7.
- Made from: "PuddleSplash.wav" (the page's HQ mp3 preview (128 kbps; the original needs a login)), decoded to mono at 22050 Hz; seconds 1.10 to 1.85; a 40 Hz high-pass, an 8 ms fade-in and a 60 ms cosine fade-out; the peak set to -4 dBFS. Nothing else changed.
- Source: https://freesound.org/s/207139/
- Author: ahill86 (https://freesound.org/people/ahill86/)
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-14: the licence listed is Creative Commons 0.

## rain_puddle_7.wav

- What: a puddle splash thrown up by a car's tyres (`rain_tyre_roll`, with `rain_v2`), played once at random, more often the faster the car; mono, 16-bit, 22050 Hz. One of 7.
- Made from: "Walking through puddle" (the page's HQ mp3 preview (128 kbps; the original needs a login)), decoded to mono at 22050 Hz; seconds 2.40 to 3.10 (a step into a puddle); a 40 Hz high-pass, an 8 ms fade-in and a 60 ms cosine fade-out; the peak set to -4 dBFS. Nothing else changed.
- Source: https://freesound.org/s/553238/
- Author: NachtmahrTV (https://freesound.org/people/NachtmahrTV/)
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-14: the licence listed is Creative Commons 0.

## snow_roll.wav

- What: the roll of a car on the snowy rally's track (Alpine 3), a seamless loop, mono, 16-bit, 22050 Hz, 5.3 s. Silent when the car stands; its level and pitch rise steeply with speed, like a friction; 6 dB lower in the cockpit.
- Made from: "sled inuit drag across snow.wav" (the page's preview), down-mixed to mono at 22050 Hz: the four drag stretches, seconds 2.45 to 4.30, 5.20 to 6.80, 0.20 to 1.60 and 7.70 to 9.05, joined in that order with 0.2 s cross-fades; a high-pass at 180 Hz and a low-pass at 9 kHz; looped with a 0.3 s equal-power cross-fade of the end into the start; the level set to -18 dBFS RMS with the peak at half scale. Nothing else changed.
- Source: https://freesound.org/people/kyles/sounds/450847/
- Author: kyles (https://freesound.org/people/kyles/), uploaded 2018-12-06
- Licence: Creative Commons 0 (public domain dedication), http://creativecommons.org/publicdomain/zero/1.0/ . No attribution is required; it is given here anyway.
- Checked on the page on 2026-11-13: the licence listed is Creative Commons 0.
