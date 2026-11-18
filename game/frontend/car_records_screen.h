/* port (dep59 showcase, owner 2026-11-18): a car's RECORDS screen, a screen of its own as Best Times is: Best Times' art (the dark blue page)
 * with its headings taken out (added_best_art.h), the car's name in the title band, the table of car_records_view.h in Best Times' rows and
 * letters, the page selector and DONE where Best Times has its footer. It fades in from black as the screens of the game do and fades out
 * to black at DONE; the page it was opened from draws itself again after. Run by the added car's page and by the CD's pages. */
#ifndef TNFS_CAR_RECORDS_SCREEN_H
#define TNFS_CAR_RECORDS_SCREEN_H

/* the screen for the car `id` (the registry's, a stock car's included) named `name`, until DONE (a click on it, Enter, Escape or the back button);
 * Left and Right (or a click on the selector's halves) turn the table's pages. The display is left dark. */
void car_records_screen(const char *id, const char *name);

#endif
