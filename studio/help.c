/* tnfs-studio --help: the usage of each subcommand, each flag it takes (the flags the parsers in the studio sources read). */
#include "studio/help.h"

#include <stdio.h>
#include <string.h>

static const char *const HELP_UNPACK =
    "usage: tnfs-studio unpack FILE DIR [--palette FILE] [--force]   (export = unpack)\n"
    "--palette FILE   a game file (FSH or QFS) whose palette colours bitmaps that carry none\n"
    "--force          overwrite existing files\n";

static const char *const HELP_PACK =
    "usage: tnfs-studio pack DIR OUT --from ORIGINAL [--palette FILE] [--force]   (import = pack)\n"
    "--from ORIGINAL  the file the tree was unpacked from; packing needs it (edits overlay it)\n"
    "--palette FILE   a game file (FSH or QFS) whose palette colours bitmaps that carry none\n"
    "--force          overwrite existing files\n";

static const char *const HELP_VALIDATE =
    "usage: tnfs-studio validate [--compat | --port-only] [--check-install] FILE_OR_DIR\n"
    "  FILE_OR_DIR: a game file, a mod's folder or a mod's zip (studio/mod_validate.h)\n"
    "--compat         refuse what the original executable would mishandle (default)\n"
    "--port-only      the same findings as warnings, exit 0\n"
    "--check-install  also warn when CARSPECS is writable (the original then zeroes every car's power)\n";

static const char *const HELP_SLOT =
    "usage: tnfs-studio slot list track|car\n"
    "       tnfs-studio slot replace track|car SLOT SOURCE_DIR --mods DIR [--data DIR] [--stem OLD] [--also REL]...\n"
    "       tnfs-studio slot undo --mods DIR [--all] [--force]\n"
    "--mods DIR       the mod folder to write (replace) or to undo in\n"
    "--data DIR       the game data the slot's files are read from (default original)\n"
    "--stem OLD       the stem the source's files carry, when it is not the slot's\n"
    "--also REL       one more file of the source, by its path below SOURCE_DIR (up to 8)\n"
    "--all            undo every transaction, not only the last\n"
    "--force          undo even where a file changed since it was written\n";

static const char *const HELP_SLOT_LIST = "usage: tnfs-studio slot list track|car\n";

static const char *const HELP_SLOT_REPLACE =
    "usage: tnfs-studio slot replace track|car SLOT SOURCE_DIR --mods DIR [--data DIR] [--stem OLD] [--also REL]...\n"
    "--mods DIR       the mod folder to write\n"
    "--data DIR       the game data the slot's files are read from (default original)\n"
    "--stem OLD       the stem the source's files carry, when it is not the slot's\n"
    "--also REL       one more file of the source, by its path below SOURCE_DIR (up to 8)\n";

static const char *const HELP_SLOT_UNDO =
    "usage: tnfs-studio slot undo --mods DIR [--all] [--force]\n"
    "--mods DIR       the mod folder whose last replacement is undone\n"
    "--all            undo every transaction, not only the last\n"
    "--force          undo even where a file changed since it was written\n";

static const char *const HELP_TRACK_NEW =
    "usage: tnfs-studio track new LAYOUT --template SLOT --out DIR [--stem NAME] [--data DIR] [--force]\n"
    "       tnfs-studio track new LAYOUT --template SLOT --piece --id ID --stem STEM --out DIR [--name TEXT]\n"
    "                             [--road asphalt|dirt] [--edge WORD] [--author TEXT] [--description TEXT] [--data DIR] [--force]\n"
    "       tnfs-studio track new LAYOUT --generic ART.FAM --piece --id ID --stem STEM --out DIR   (a track of our own template: no CD data)\n"
    "LAYOUT           the track as text (formats/track_gen.h)\n"
    "--template SLOT  the track slot whose files are the template (tnfs-studio slot list track)\n"
    "--generic ART.FAM  instead of --template: the track is made from our own template and this art file (tnfs-studio fam build); needs --piece\n"
    "--out DIR        where the files go, laid out like the CD (a piece: its mod.ini too)\n"
    "--stem NAME      the stem the files get (1..8 of letters and digits; a piece: STEM, its stage 1 is STEM1)\n"
    "--piece          make a track piece (a mod of type=track) instead of a slot's replacement\n"
    "--id ID          the piece's id (a-z 0-9 and '-', not first); with --piece\n"
    "--name TEXT      the name the game shows; with --piece (the id when not given)\n"
    "--road WORD      asphalt or dirt; with --piece (asphalt when not given)\n"
    "--edge WORD      grass, gravel, sand, snow, dirt or water; with --piece (grass when not given)\n"
    "--author TEXT    the author's name, shown in the piece's card (up to 48 characters); with --piece\n"
    "--description TEXT  what the piece is (up to 400 characters); with --piece\n"
    "--data DIR       the CD's data root the template is read from (default original, relative to the current folder)\n"
    "--force          overwrite existing files; a piece's mod.ini keeps the keys the generator does not write\n";

static const char *const HELP_CAR_NEW =
    "usage: tnfs-studio car new --template CAR --id ID --stem STEM --out DIR [--name NAME] [--author TEXT] [--description TEXT] [--data DIR] [--port-data DIR] [--force]\n"
    "--template CAR   one of the eight race cars: its word (viper) or its stem on the CD (DVIPER)\n"
    "--id ID          the mod's id, what --car takes (a-z 0-9 and '-', not first)\n"
    "--stem STEM      the name its three files go by (1..8 of A-Z and 0-9)\n"
    "--out DIR        the folder to write\n"
    "--name NAME      the name the game shows (the id when not given)\n"
    "--author TEXT    the author's name, up to 48 characters\n"
    "--description TEXT  what the car is, up to 400 characters\n"
    "--data DIR       the CD's data root the template's CFM, PDN and PBS are read from (default original)\n"
    "--port-data DIR  the port's data, for the template's car and dash masks (default data)\n"
    "--force          overwrite existing files\n";

static const char *const HELP_CAR_CARD =
    "usage: tnfs-studio car card PIECE_DIR --generic [--force]\n"
    "       tnfs-studio car card PIECE_DIR --tnfs BIN [--data DIR] [--track TR1] [--node N] [--time day|dawn|dusk|night] [--force]\n"
    "--generic        a plain card.png (a ground the game draws the car's name over)\n"
    "--tnfs BIN       the game program to take the picture from (a race's screenshot)\n"
    "--data DIR       the CD's data root for that race (default original)\n"
    "--track TR1      the track of that race (default TR1)\n"
    "--node N         the node of the track the picture is taken at\n"
    "--time WORD      day, dawn, dusk or night (default day)\n"
    "--force          overwrite an existing card.png\n";

static const char *const HELP_CAR_IMPORT =
    "usage: tnfs-studio car import PIECE_DIR --obj FILE.obj [--low FILE.obj] [--textures DIR] [--low-textures DIR] [--raw]\n"
    "                              [--tables DIR] [--low-tables DIR] [--scale S] [--steer DEGREES] [--donor CAR] [--keep-pdn]\n"
    "                              [--data DIR] [--force]\n"
    "--obj FILE.obj      the model (its high detail, when --low is given too)\n"
    "--low FILE.obj      the low detail model\n"
    "--textures DIR      the high detail model's textures\n"
    "--low-textures DIR  the low detail model's textures\n"
    "--raw               keep the model's own numbers; with --tables and --low-tables\n"
    "--tables DIR        the high detail tables, with --raw\n"
    "--low-tables DIR    the low detail tables, with --raw\n"
    "--scale S           the model's scale (a number above 0; 1 when not given)\n"
    "--steer DEGREES     the wheels' full lock, between 0 and 90 (the CD's cars turn 22 to 37)\n"
    "--donor CAR         the original car whose files the import starts from\n"
    "--keep-pdn          keep the donor's PDN instead of writing one\n"
    "--data DIR          the CD's data root (default original)\n"
    "--force             overwrite existing files\n";

static const char *const HELP_CAR_RESIZE =
    "usage: tnfs-studio car resize PIECE_DIR NAME WxH [--image FILE.png] [--low]\n"
    "       tnfs-studio car resize PIECE_DIR NAME --image FILE.png [--low]\n"
    "WxH               the new size of the bitmap NAME, in pixels\n"
    "--image FILE.png  the picture to scale into it\n"
    "--low             the low detail bank's bitmap, not the high detail one\n";

static const char *const HELP_CAR_TEXTURE =
    "usage: tnfs-studio car texture add PIECE_DIR NAME FILE.png [--low] [--label LABEL]\n"
    "--low             add to the low detail bank\n"
    "--label LABEL     the text the name is shown as (the name when not given)\n";

static const char *const HELP_CAR =
    "usage: tnfs-studio car new --template CAR --id ID --stem STEM --out DIR [--name NAME] [--author TEXT] [--description TEXT] [--data DIR] [--port-data DIR] [--force]\n"
    "       tnfs-studio car card PIECE_DIR --generic | --tnfs BIN ... (car card --help)\n"
    "       tnfs-studio car import PIECE_DIR --obj FILE.obj ... (car import --help)\n"
    "       tnfs-studio car resize PIECE_DIR NAME WxH ... (car resize --help)\n"
    "       tnfs-studio car texture add PIECE_DIR NAME FILE.png ... (car texture --help)\n"
    "       tnfs-studio car kit PIECE_DIR KIT_DIR     a car piece laid out as a kit: flat sources (README.txt inside says which)\n"
    "       tnfs-studio car build KIT_DIR             the kit's build/piece/ and build/<id>.zip from its sources alone\n"
    "       tnfs-studio car check KIT_DIR             what is wrong with the kit before a build: the model, its wheels, its masks\n"
    "Each form's flags: tnfs-studio car FORM --help\n";

static const char *const HELP_FAM =
    "usage: tnfs-studio fam build SPEC --out FILE.FAM [--force]\n"
    "SPEC             a text file, one picture per line in the order the file takes them (formats/fam_spec.h):\n"
    "                   terrain N [letters=ABC] FILE.png   scenery FILE.png   horizon FILE.png (exactly one)\n"
    "                 paths are relative to the spec's folder; '#' starts a comment\n"
    "--out FILE.FAM   the art file to write (the track's art: track new --generic takes it)\n"
    "--force          overwrite an existing FILE.FAM\n";

static const char *const HELP_MOD =
    "usage: tnfs-studio mod pack DIR OUT.zip [--force]\n"
    "--force           overwrite an existing OUT.zip\n";

static const struct {
    const char *key, *text;
} HELP[] = {
    { "unpack", HELP_UNPACK },      { "export", HELP_UNPACK },        { "pack", HELP_PACK },
    { "import", HELP_PACK },        { "validate", HELP_VALIDATE },    { "slot", HELP_SLOT },
    { "slot list", HELP_SLOT_LIST }, { "slot replace", HELP_SLOT_REPLACE }, { "slot undo", HELP_SLOT_UNDO },
    { "track", HELP_TRACK_NEW },    { "track new", HELP_TRACK_NEW },  { "car", HELP_CAR },
    { "car new", HELP_CAR_NEW },    { "car card", HELP_CAR_CARD },    { "car import", HELP_CAR_IMPORT },
    { "car resize", HELP_CAR_RESIZE }, { "car texture", HELP_CAR_TEXTURE }, { "mod", HELP_MOD },
    { "mod pack", HELP_MOD },
    { "fam", HELP_FAM },            { "fam build", HELP_FAM },
};

static int asks_for_help(int argc, char **argv)
{
    for (int i = 2; i < argc; ++i)
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) return 1;
    return 0;
}

static const char *lookup(const char *key)
{
    for (size_t i = 0; i < sizeof HELP / sizeof HELP[0]; ++i)
        if (strcmp(HELP[i].key, key) == 0) return HELP[i].text;
    return NULL;
}

const char *studio_help_for(int argc, char **argv)
{
    if (argc < 2 || !asks_for_help(argc, argv)) return NULL;
    const char *command = argv[1];
    if (argc > 2 && argv[2][0] != '-') {
        char key[32];
        snprintf(key, sizeof key, "%s %s", command, argv[2]);
        const char *text = lookup(key);
        if (text != NULL) return text;
    }
    return lookup(command);
}
