#ifndef WENA_DESKTOP_DEPENDENCIES_H
#define WENA_DESKTOP_DEPENDENCIES_H

#include <stdio.h>

/* Tests only documented upstream WAL-reset fixed version branches. A false
 * result does not identify distributor backports or prove a vulnerability. */
int wena_sqlite_wal_reset_fix_known(int version_number);

/* Emit stable key=value diagnostics without initializing video or opening a
 * database. Values escape controls and percent signs as %HH. Returns zero on
 * invalid output or write failure. Reports the libraries this process uses. */
int wena_desktop_dependency_report(FILE *output);

#endif
