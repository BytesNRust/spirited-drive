/* Export original car (dep59 car editor E1): a car of the game made into a project folder of the player's own, from the player's game files. Two kinds:
 *  - one of the eight cars you drive: a car project like `tnfs-studio car new` makes (mod.ini, SIMDATA/CARFAMS and CARSPECS, car_masks/<folder>/, dash_masks/),
 *    with the car's own stem and a card that says redistributable=0 and lists the catalogue colours;
 *  - a traffic car or the police car, which have no PBS and cannot be driven: a masks project (project.ini with kind=masks, original/<X>.CFM kept to look at,
 *    car_masks/<folder>/), made to adjust their masks.
 * The masks are copied from the game's data/car_masks/<folder>/ with the same names and the same bytes, so a project's folder is exactly what the game's holds. A car
 * the game has no masks for (probe) is listed but cannot be exported. Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_CAR_EXPORT_H
#define TNFS_TRACK_EDITOR_CAR_EXPORT_H

#include <stddef.h>
#include <stdint.h>

enum { EXPORT_PATH = 4096, EXPORT_CARS = 21 };

typedef enum { EXPORT_DRIVABLE = 0, EXPORT_TRAFFIC, EXPORT_POLICE } ExportKind;

typedef struct {
    ExportKind kind;
    const char *folder;   /* the folder in data/car_masks, spelt as the game's data has it */
    const char *file;     /* the car file's stem on the game's disc (SIMDATA/CARFAMS/<file>.CFM, in any case) */
    const char *label;    /* what the dialog calls it */
    int model;            /* a drivable car's race model 0..7; -1 for the others */
} ExportCar;

/* where the game's files are: `cd` holds SIMDATA/CARFAMS/ (the player's copy of the game's data), `data` holds car_masks/ (the game's own data folder) */
typedef struct {
    char cd[EXPORT_PATH];
    char data[EXPORT_PATH];
} ExportSource;

/* the `index`-th car of the dialog (drivable first, then the traffic cars, then the police car); NULL past EXPORT_CARS */
const ExportCar *car_export_car(int index);

/* The places to look, from the game's program (`game`, "<app>/Contents/MacOS/tnfs" or a build tree's ./tnfs), the player folder in use (`user`, "" for the
 * system's: then `pref`, ending in '/', NULL for none) and the two overrides (NULL for none). Each is the first candidate that holds what it should, else the
 * first candidate (the dialog then says what is missing and where it looked). */
/* the game's file `rel` ("FRONTEND/ART/CONTROL/CENTRAL.QFS", each part in any case) below `cd`, read whole: `data` malloc'd (free it), `size`. 1, or 0 with `why`. */
int car_export_game_file(const char *cd, const char *rel, uint8_t **data, size_t *size, char *why, size_t why_cap);
void car_export_source(const char *game, const char *user, const char *pref, const char *cd_override, const char *data_override, ExportSource *out);

/* 1 when the car can be exported from `src`; 0 with the reason (English, for the person: what is missing and where it looked) */
int car_export_available(const ExportSource *src, int index, char *why, size_t why_cap);

/* The name of the project folder for the car, such as "p911-original": the folder a dialog's chosen parent gets the project in */
void car_export_folder_name(int index, char *out, size_t cap);

/* Makes the project of car `index` in `dir`, a folder that is not there yet (its parent is made if needed). It is made beside as "<dir>.exporting" and renamed when
 * whole, so a failure leaves nothing. 1 with the folder made; 0 with `why`: the car cannot be exported, `dir` exists (a second export into the same folder is
 * refused), or a file could not be read or written. */
int car_export(const ExportSource *src, int index, const char *dir, char *why, size_t why_cap);

/* where a masks folder maps to in the game's source tree: "data/car_masks/<folder>/" */
void car_export_repo_path(const char *folder, char *out, size_t cap);

/* the file: URL of a folder (percent-encoded), for the system to open it in the Finder; 0 when it does not fit */
int car_export_file_url(const char *path, char *url, size_t cap);

#endif
