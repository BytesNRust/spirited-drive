/* See app.h: New car (dep102). */
#include "app.h"

#include "car_new_kit.h"
#include "formats/car_piece.h"
#include "formats/mod_ini.h"

#include <stdio.h>
#include <string.h>

int app_new_car_named(App *a, const char *name, const char *id, const char *template_word)
{
    char made[MOD_INI_ID_MAX + 1], cars[APP_PATH], kit[APP_PATH], text[APP_MSG];
    text[0] = '\0';
    if (a->workspace[0] == '\0') snprintf(text, sizeof text, "no workspace: New car makes the car in the workspace's cars/ folder");
    else if (name == NULL || name[strspn(name, " ")] == '\0') snprintf(text, sizeof text, "give the car a name");
    else {
        if (id == NULL || id[0] == '\0') id = workspace_id_from_name(name, made, sizeof made, MOD_INI_ID_MAX) ? made : "";
        if (snprintf(cars, sizeof cars, "%s/cars", a->workspace) >= (int)sizeof cars) snprintf(text, sizeof text, "the workspace's path is too long");
        else if (!car_new_kit(a->export_src.cd, a->data_dir, cars, template_word, name, id, kit, sizeof kit, text, sizeof text)) {
            if (text[0] == '\0') snprintf(text, sizeof text, "the car was not made");
        }
    }
    if (text[0] != '\0') {
        snprintf(a->new_car_error, sizeof a->new_car_error, "%s", text);
        snprintf(a->status, sizeof a->status, "%s", text);
        return 0;
    }
    a->new_car_error[0] = '\0';
    app_workspace_refresh(a);
    return app_open_car(a, kit);
}
