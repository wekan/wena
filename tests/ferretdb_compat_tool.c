/* The Wena side of tests/test_ferretdb_roundtrip.py:
 *   write DIR    a new wekan.sqlite with a board, written by Wena
 *   update DIR   change the document FerretDB inserted, Wena's way */
#include "../server/ferretdb_sqlite.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    char path[1024], table[WENA_FERRETDB_TABLE_CAPACITY];
    sqlite3 *db;
    WenaFerretField fields[6];
    const char *gone[1] = {"description"};
    int ok;
    if (argc != 3 || strlen(argv[2]) > 900) return 2;
    sprintf(path, "%s/wekan.sqlite", argv[2]);
    if (sqlite3_open(path, &db) != SQLITE_OK) return 1;
    sqlite3_db_config(db, SQLITE_DBCONFIG_DQS_DDL, 0, NULL);
    sqlite3_db_config(db, SQLITE_DBCONFIG_DQS_DML, 0, NULL);
    sqlite3_exec(db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    if (!strcmp(argv[1], "write")) {
        fields[0].key = "title"; fields[0].element = WENA_FERRET_STRING; fields[0].value = "\"Wena board\"";
        fields[1].key = "archived"; fields[1].element = WENA_FERRET_BOOL; fields[1].value = "false";
        fields[2].key = "createdAt"; fields[2].element = WENA_FERRET_DATE; fields[2].value = "1700000000000";
        fields[3].key = "sort"; fields[3].element = WENA_FERRET_DOUBLE; fields[3].value = "1.5";
        fields[4].key = "members";
        fields[4].element = "{\"t\":\"array\",\"i\":[{\"t\":\"object\",\"$s\":{\"p\":{\"userId\":{\"t\":\"string\"},"
                            "\"isAdmin\":{\"t\":\"bool\"}},\"$k\":[\"userId\",\"isAdmin\"]}}]}";
        fields[4].value = "[{\"userId\":\"u1\",\"isAdmin\":true}]";
        fields[5].key = "stars"; fields[5].element = WENA_FERRET_INT; fields[5].value = "3";
        ok = wena_ferretdb_prepare(db, "main") && wena_ferretdb_collection(db, "main", "boards", 1, table, sizeof(table)) &&
             wena_ferretdb_insert(db, "main", table, "wena-b1", fields, 6);
    } else if (!strcmp(argv[1], "update")) {
        fields[0].key = "title"; fields[0].element = WENA_FERRET_STRING; fields[0].value = "\"Renamed by Wena\"";
        fields[1].key = "wenaNew"; fields[1].element = WENA_FERRET_BOOL; fields[1].value = "true";
        ok = wena_ferretdb_collection(db, "main", "boards", 0, table, sizeof(table)) &&
             wena_ferretdb_update(db, "main", table, "kanban1", fields, 2) &&
             wena_ferretdb_unset(db, "main", table, "kanban1", gone, 1);
    } else ok = 0;
    sqlite3_close(db);
    return ok ? 0 : 1;
}
