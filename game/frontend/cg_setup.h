/* port (dep59 showcase): Options' three settings (transmission 0 Auto / 1 Manual, ABS and traction control 0 On / 1 Off / 2 not offered) as the
 * SETUP panels read and keep them (central_glue.c). */
#ifndef TNFS_CG_SETUP_H
#define TNFS_CG_SETUP_H

void cg_setup_get(int *transmission, int *abs, int *traction);
void cg_setup_put(int transmission, int abs, int traction);
/* the same for the car whose page is shown: ABS and traction control "not offered" (2) by that car's own tables, not the player's */
void cg_setup_get_for(int car, int *transmission, int *abs, int *traction);

#endif
