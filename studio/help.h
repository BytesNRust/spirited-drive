/* tnfs-studio --help (or -h) after any subcommand: that subcommand's usage with every flag it takes (studio/help.c). */
#ifndef TNFS_STUDIO_HELP_H
#define TNFS_STUDIO_HELP_H

/* The usage text when argv asks for help (-h or --help after the command, or `track new --help`), NULL otherwise or for a
 * command this does not know. A subcommand with its own help is looked up by its command and its word (`car new`), else the command's. */
const char *studio_help_for(int argc, char **argv);

#endif
