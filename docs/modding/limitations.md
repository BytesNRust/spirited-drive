# Known Level 1 limitations

Level 1 replaces the files of existing slots; it does not patch the executable (that is Level 2).

## Car names

- A replaced car shows its new **full name on the selection screens**: those names are QFS bitmaps, changed by editing
  the image (`tnfs-studio unpack`, edit the PNG, `pack`; name the file with `slot replace --also`).
- The **short names on the results and records screens keep the original model name**. They are text in the EXE
  (table `004c5c84`, strings at `004b11e8..` and `004b1924..`, fixed slots of 4 or 8 bytes), drawn with `00463cd8`.
  Renaming them needs an EXE patch, which is Level 2 and out of scope for now. Details: `inventory.md` §8.1.
- `tnfs-studio slot replace car ...` prints this as a note.

## Slots

- Slots are fixed tables in the EXE: a mod replaces one of the 16 tracks or the cars, it cannot add one.
