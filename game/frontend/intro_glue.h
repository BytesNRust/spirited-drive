/* The intro (00441abc, intro.c) and the menus' movies (004419c0) on the port's window and the TGV player. */
#ifndef TNFS_INTRO_GLUE_H
#define TNFS_INTRO_GLUE_H

/* 00441abc: the EA, attract and title movies */
void glue_intro_show(void);

/* 004419c0: one movie of frontend/movie/ (cop1.tgv, the win movies, the showcase's) */
void glue_intro_movie(const char *name);

#endif
