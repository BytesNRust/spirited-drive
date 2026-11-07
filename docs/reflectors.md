# Reflectors

At night, or in a dark tunnel, what is reflective (road signs, delineator posts, the cat's eyes painted on some roads) shines when the **player's** headlights reach it, far beyond the pool of light the road gets. Behind `reflectors=1` / `TNFS_REFLECTORS=1` (off in every profile; needs `psx_headlights`). With no mask for a texture, or the switch off, nothing changes: the frame is the one without the feature.

## What is reflective

A mask per texture (`game/race/track_mask.h`, `data/track_masks/<TRACK>/slot_NN.png` and `scenery_NN.png`; red >= 128 and not transparent). `track_mask_for` is the lookup. A mask with no texel set is ignored.

## The shine (`game/race/reflector.h`)

`reflector_gain(forward, lateral, height, dark)`, 0..256, from the player's lamps (2.2 m ahead of the car's centre, heading = the car's nose):

- 0 by day, behind the lamps, beyond `REFLECTOR_REACH_M` (200 m) or outside the cone;
- full to `REFLECTOR_NEAR_M` (6 m), then falling smoothly (smoothstep) to nothing at the reach;
- across, falling to nothing at `REFLECTOR_HALF_WIDTH_PERCENT` (35 %) of the distance (+ 1 m, the car's width): brightest on the axis;
- above the lamps: full to 6 % of the distance over their level, none from 16 % (a sign over the road).

The gain of a polygon is taken at its place (an object at its own place, a road polygon at the mean of its corners) and quantised to `REFLECTOR_LEVELS - 1` = 5 steps (`reflector_level`): no dither, so nothing crawls.

## How it is drawn (`game/viewer/reflect_pass.h`)

A second pass. A polygon whose texture has a mask is drawn once more right after its own draw (`draw_uv`, `draw_camera`, `draw_full`, `draw_full_alt` in `ported_view.c`: the road and the objects share them) from a copy of the texture where the texels the mask leaves out are the see-through index (0xFF), through a table that points each masked texel at an entry the night already has lit. No palette entry of its own is spent (at night the free ones are 0, `drive_palette.c`), and every colour of the texture is kept.

A texel takes a family by its own colour (`reflector_family`) and the polygon's level takes a step of that family's ramp (`reflector_step`), dimmest first:

| family | texels | entries |
|---|---|---|
| red | red | the position light, the lit brake light |
| warm | any other saturated colour (yellow, orange, green, blue) | the street lamp's rim, core, lens (the windows' entry when the track has no street lamp) |
| white | grey, white | the headlight, the reversing light |

A family with none of those entries borrows another's. A green sign therefore shines warm, not green. The table follows the palette (remade when it changes), and the copy is made again after a palette change (the bitmaps are remapped in place to the screen palette).

The masks of a road slot's variants (the same picture smaller) are the base mask resampled any-of (`reflector_mask_scale`), so a one-texel stud survives the smaller bitmaps.

## Cost

Only polygons with a mask, in the dark, with the lamps lit and the gain above 0, are drawn twice. No difference was measurable at the noise of the machine (CL1 and CY1 at night, 240 frames).

## Not done

- 3D models (scenery drawn through `draw_model`, the cars) are not reflective: the masks cover the boards (billboards) and the road.
- The rivals' lamps do nothing (the light a reflector returns goes back to its own source).
