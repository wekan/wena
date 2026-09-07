#define _POSIX_C_SOURCE 200809L
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../imports/preferences/collapse.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/resource.h>
#include <signal.h>
static char baseline[24576];
static size_t baseline_length;
static const char *workspace;
static char path[4096];
static WenaBoardCollapseState original;
static void write_data(const char *name, const char *data, size_t length)
{
    FILE *file;
    file = fopen(name, "wb"); assert(file);
    assert(fwrite(data, 1, length, file) == length); assert(!fclose(file));
}
static size_t read_data(const char *name, char *data)
{
    FILE *file;
    size_t length;
    file = fopen(name, "rb"); assert(file); length = fread(data, 1, 24575, file);
    assert(!ferror(file)); assert(!fclose(file)); data[length] = 0; return length;
}
static void bad_file(const char *data, size_t length)
{
    WenaBoardCollapseState state;
    char after[24576];
    write_data(path, data, length); state = original;
    assert(!wena_collapse_preferences_load(path, workspace, "actor", "board", &state));
    assert(!memcmp(&state, &original, sizeof(state)));
    assert(!wena_collapse_preferences_save(path, workspace, "actor", &state));
    assert(!wena_collapse_preferences_reset(path, workspace, "actor", "board", &state));
    assert(!memcmp(&state, &original, sizeof(state)));
    assert(read_data(path, after) == length && !memcmp(data, after, length));
}
int main(int argc, char **argv)
{
    WenaBoardCollapseState state, empty;
    WenaBoard board;
    WenaSwimlane lane;
    WenaList list;
    WenaBoardLayout layout;
    char other[4096], temp[4100], data[24576], sentinel[4096];
    char *p;
    size_t i, length;
    struct stat info;
    struct rlimit old_limit, restricted;
    void (*old_signal)(int);
    assert(argc == 2); workspace = argv[1];
    memset(&original, 0, sizeof(original)); strcpy(original.board_id, "board");
    strcpy(original.swimlane_ids[0], "lane"); original.swimlane_count = 1;
    strcpy(original.list_ids[0], "list"); original.list_count = 1;
    assert(wena_collapse_preferences_path(workspace, "actor", "board", path, sizeof(path)));
    assert(wena_collapse_preferences_path(workspace, "actor2", "board", other, sizeof(other)) && strcmp(path, other));
    assert(wena_collapse_preferences_path(workspace, "actor", "board2", other, sizeof(other)) && strcmp(path, other));
    strcpy(sentinel, "unchanged");
    assert(!wena_collapse_preferences_path(workspace, "bad/actor", "board", sentinel, sizeof(sentinel)));
    assert(!wena_collapse_preferences_path(workspace, "actor", "board", sentinel, 5));
    assert(!strcmp(sentinel, "unchanged"));
    assert(!wena_collapse_preferences_path("bad\nworkspace", "actor", "board", sentinel, sizeof(sentinel)));
    state = original;
    assert(wena_collapse_preferences_load(path, workspace, "actor", "board", &state) == 2);
    assert(!memcmp(&state, &original, sizeof(state)));
    assert(wena_collapse_preferences_save(path, workspace, "actor", &original));
    assert(!stat(path, &info) && (info.st_mode & 0777) == 0600);
    baseline_length = read_data(path, baseline);
    /* Force a real write/flush failure after exclusive temporary creation. */
    assert(!getrlimit(RLIMIT_FSIZE, &old_limit)); restricted = old_limit;
    restricted.rlim_cur = 16; old_signal = signal(SIGXFSZ, SIG_IGN);
    assert(old_signal != SIG_ERR); assert(!setrlimit(RLIMIT_FSIZE, &restricted));
    assert(!wena_collapse_preferences_save(path, workspace, "actor", &original));
    assert(!setrlimit(RLIMIT_FSIZE, &old_limit)); signal(SIGXFSZ, old_signal);
    assert(read_data(path, data) == baseline_length && !memcmp(data, baseline, baseline_length));
    strcpy(temp, path); strcat(temp, ".tmp"); assert(lstat(temp, &info) != 0);

    memset(&state, 0, sizeof(state));
    assert(wena_collapse_preferences_load(path, workspace, "actor", "board", &state));
    assert(!memcmp(&state, &original, sizeof(state)));
    assert(!wena_collapse_preferences_load(path, workspace, "wrong", "board", &state));
    assert(!wena_collapse_preferences_load(path, workspace, "actor", "wrong", &state));
    assert(!wena_collapse_preferences_load(path, "other-workspace", "actor", "board", &state));
    assert(!memcmp(&state, &original, sizeof(state)));
    assert(!wena_collapse_preferences_save(path, workspace, "wrong", &state));
    strcpy(temp, path); strcat(temp, ".tmp"); write_data(temp, "stale", 5);
    assert(!wena_collapse_preferences_reset(path, workspace, "actor", "board", &state));
    assert(!memcmp(&state, &original, sizeof(state)));
    assert(read_data(path, data) == baseline_length && !memcmp(data, baseline, baseline_length));
    assert(read_data(temp, data) == 5 && !strcmp(data, "stale")); assert(!remove(temp));
    assert(!symlink(path, temp));
    assert(!wena_collapse_preferences_save(path, workspace, "actor", &state));
    assert(!lstat(temp, &info) && S_ISLNK(info.st_mode)); assert(!remove(temp));
    assert(!rename(path, other)); assert(!symlink(other, path));
    assert(!wena_collapse_preferences_load(path, workspace, "actor", "board", &state));
    assert(!wena_collapse_preferences_save(path, workspace, "actor", &state));
    assert(read_data(other, data) == baseline_length && !memcmp(data, baseline, baseline_length));
    assert(!remove(path)); assert(!rename(other, path));
    bad_file("", 0); bad_file(baseline, baseline_length - 1);
    strcpy(data, baseline); data[14] = '2'; bad_file(data, baseline_length);
    strcpy(data, baseline); p = strstr(data, "list list\n"); assert(p); p[5] = '\t'; bad_file(data, baseline_length);
    strcpy(data, baseline); p = strstr(data, "end\n"); assert(p); strcpy(p, "list list\nend\n"); bad_file(data, strlen(data));
    strcpy(data, baseline); p = strstr(data, "end\n"); strcpy(p, "unknown value\nend\n"); bad_file(data, strlen(data));
    strcpy(data, baseline); p = strstr(data, "end\n"); strcpy(p, "swimlane late\nend\n"); bad_file(data, strlen(data));
    memcpy(data, baseline, baseline_length); data[20] = 0; bad_file(data, baseline_length);
    strcpy(data, baseline); strcat(data, "extra\n"); bad_file(data, strlen(data));
    strcpy(data, baseline); p = strstr(data, "list list\n"); *p = 0;
    for (i = 0; i < 65; ++i) { length = strlen(data); sprintf(data + length, "list id%u\n", (unsigned)i); }
    strcat(data, "end\n"); bad_file(data, strlen(data));
    write_data(path, baseline, baseline_length);
    state = original; state.list_count = 65;
    assert(!wena_collapse_preferences_save(path, workspace, "actor", &state));
    state = original; state.list_count = 2; strcpy(state.list_ids[1], "list");
    assert(!wena_collapse_preferences_save(path, workspace, "actor", &state));
    assert(wena_collapse_preferences_reset(path, workspace, "actor", "board", &state));
    memset(&empty, 0, sizeof(empty)); strcpy(empty.board_id, "board");
    assert(!memcmp(&state, &empty, sizeof(state)));
    state = original; assert(wena_collapse_preferences_load(path, workspace, "actor", "board", &state));
    assert(!memcmp(&state, &empty, sizeof(state)));
    assert(wena_collapse_preferences_save(path, workspace, "actor", &original));
    memset(&state, 0, sizeof(state)); strcpy(state.board_id, "board");
    state.list_count = state.swimlane_count = WENA_BOARD_COLLAPSE_CAPACITY;
    for (i = 0; i < WENA_BOARD_COLLAPSE_CAPACITY; ++i) {
        memset(state.list_ids[i], 'a', WENA_ID_CAPACITY-1);
        state.list_ids[i][WENA_ID_CAPACITY-1] = 0;
        state.list_ids[i][0] = (char)('A' + i / 26);
        state.list_ids[i][1] = (char)('A' + i % 26);
        strcpy(state.swimlane_ids[i], state.list_ids[i]);
    }
    assert(wena_collapse_preferences_save(path, workspace, "actor", &state));
    assert(wena_collapse_preferences_load(path, workspace, "actor", "board", &empty));
    assert(!memcmp(&state, &empty, sizeof(state)));
    assert(wena_collapse_preferences_save(path, workspace, "actor", &original));
    assert(wena_board_init(&board, "board", "Board", 0));
    assert(wena_swimlane_init(&lane, "lane", "board", "Lane", 0, 0));
    assert(wena_list_init(&list, "list", "board", "lane", "List", 0, 1));
    memset(&layout, 0, sizeof(layout)); layout.board = &board;
    layout.swimlanes = &lane; layout.swimlane_count = 1;
    layout.lists = &list; layout.list_count = 1;
    assert(wena_collapse_preferences_load(path, workspace, "actor", "board", &state));
    assert(wena_board_collapse_sync(&state, &layout));
    assert(state.swimlane_count == 1 && state.list_count == 0);
    /* Pruning is in-memory only until an explicit user save. */
    assert(read_data(path, data) == baseline_length && !memcmp(data, baseline, baseline_length));
    assert(!wena_collapse_preferences_reset(path, workspace, "actor", "board", NULL));
    assert(!wena_collapse_preferences_save(path, workspace, "actor", NULL));
    assert(!wena_collapse_preferences_load(path, workspace, "actor", "board", NULL));
    assert(!remove(path));
    puts("collapse preferences: scoped atomic save/load/reset, strict input and symlink/collision failures passed");
    return 0;
}
