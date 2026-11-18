/* port (dep59 step 2): the installed mods from the command line, no window and no game data needed (main.c):
 *   --install FILE.zip   installs or updates a mod; prints "installed <id> <version>", "updated <id> <old> -> <new>", or for the same version again "reinstalled <id> <version> (same version)" (and, when its files changed, that its version did not: raise version=)
 *   --uninstall ID       moves it to the backups; prints "uninstalled <id> (backup kept)"
 *   --mods-list          one line per installed mod: "<id>\t<version>\t<type>\t<on|off>\t<name>" (on: the edition in use names it)
 * The exit status: 0, or 1 with "tnfs: <reason>" on stderr. */
#ifndef TNFS_MOD_COMMANDS_H
#define TNFS_MOD_COMMANDS_H

int mod_commands_install(const char *zip_path);
int mod_commands_uninstall(const char *id);
int mod_commands_list(void);

#endif
