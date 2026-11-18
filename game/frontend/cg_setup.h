/* port (dep59 showcase): Options' three settings (transmission 0 Auto / 1 Manual, ABS and traction control 0 On / 1 Off / 2 not offered) as the
 * SETUP panels read and keep them (central_glue.c). */
#ifndef TNFS_CG_SETUP_H
#define TNFS_CG_SETUP_H

void cg_setup_get(int *transmission, int *abs, int *traction);
void cg_setup_put(int transmission, int abs, int traction);

#endif
