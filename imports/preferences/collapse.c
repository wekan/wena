#define _POSIX_C_SOURCE 200809L
#include "collapse.h"
#include "../../server/sha256.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#define PREF_BYTES 24576u

static int scope_valid(const char *workspace, const char *actor, const char *board)
{
    return wena_model_title_string_valid(workspace, WENA_EXECUTABLE_PATH_CAPACITY) &&
        wena_model_identifier_valid(actor) && wena_model_identifier_valid(board);
}
static void hash_part(WenaSha256 *hash, const char *text)
{
    unsigned char length[4];
    size_t n;
    n = strlen(text);
    length[0] = (unsigned char)(n >> 24); length[1] = (unsigned char)(n >> 16);
    length[2] = (unsigned char)(n >> 8); length[3] = (unsigned char)n;
    wena_sha256_update(hash, length, 4);
    wena_sha256_update(hash, (const unsigned char *)text, n);
}
int wena_collapse_preferences_path(const char *workspace, const char *actor,
    const char *board, char *output, size_t capacity)
{
    WenaSha256 hash;
    char digest[65], result[WENA_EXECUTABLE_PATH_CAPACITY];
    size_t directory, i, needed;
    if (!output || !scope_valid(workspace, actor, board)) return 0;
    directory = 0;
    for (i = 0; workspace[i]; ++i) {
        if (workspace[i] == '/'
#if defined(_WIN32)
            || workspace[i] == '\\'
#endif
        ) directory = i + 1;
    }
    needed = directory + sizeof("wena-collapse-") - 1 + 64 + sizeof(".prefs");
    if (needed > sizeof(result) || needed > capacity) return 0;
    wena_sha256_init(&hash);
    hash_part(&hash, workspace); hash_part(&hash, actor); hash_part(&hash, board);
    wena_sha256_final_hex(&hash, digest);
    memcpy(result, workspace, directory); result[directory] = 0;
    strcat(result, "wena-collapse-"); strcat(result, digest); strcat(result, ".prefs");
    memcpy(output, result, needed);
    return 1;
}
static int state_valid(const WenaBoardCollapseState *state)
{
    size_t i, j;
    if (!state || !wena_model_identifier_valid(state->board_id) ||
        state->list_count > WENA_BOARD_COLLAPSE_CAPACITY ||
        state->swimlane_count > WENA_BOARD_COLLAPSE_CAPACITY) return 0;
    for (i = 0; i < state->list_count; ++i) {
        if (!wena_model_identifier_valid(state->list_ids[i])) return 0;
        for (j = 0; j < i; ++j) if (!strcmp(state->list_ids[i], state->list_ids[j])) return 0;
    }
    for (i = 0; i < state->swimlane_count; ++i) {
        if (!wena_model_identifier_valid(state->swimlane_ids[i])) return 0;
        for (j = 0; j < i; ++j) if (!strcmp(state->swimlane_ids[i], state->swimlane_ids[j])) return 0;
    }
    return 1;
}
static void hex_workspace(const char *workspace, char *hex)
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    unsigned char c;
    for (i = 0; workspace[i]; ++i) {
        c = (unsigned char)workspace[i]; hex[2*i] = digits[c >> 4]; hex[2*i+1] = digits[c & 15];
    }
    hex[2*i] = 0;
}
static char *line(char **cursor)
{
    char *start, *end;
    start = *cursor; end = strchr(start, '\n');
    if (!end) return NULL;
    *end = 0; *cursor = end + 1;
    return start;
}
static int parse(char *data, const char *workspace, const char *actor,
    const char *board, WenaBoardCollapseState *state)
{
    WenaBoardCollapseState candidate;
    char expected[2 * WENA_EXECUTABLE_PATH_CAPACITY], *cursor, *value;
    int lists;
    memset(&candidate, 0, sizeof(candidate)); strcpy(candidate.board_id, board);
    cursor = data;
    value = line(&cursor); if (!value || strcmp(value, "WENA-COLLAPSE 1")) return 0;
    hex_workspace(workspace, expected);
    value = line(&cursor); if (!value || strncmp(value, "workspace ", 10) || strcmp(value+10, expected)) return 0;
    value = line(&cursor); if (!value || strncmp(value, "actor ", 6) || strcmp(value+6, actor)) return 0;
    value = line(&cursor); if (!value || strncmp(value, "board ", 6) || strcmp(value+6, board)) return 0;
    lists = 0;
    while ((value = line(&cursor)) != NULL) {
        if (!strcmp(value, "end")) {
            if (*cursor || !state_valid(&candidate)) return 0;
            *state = candidate; return 1;
        }
        if (!strncmp(value, "swimlane ", 9) && !lists) {
            if (candidate.swimlane_count == WENA_BOARD_COLLAPSE_CAPACITY ||
                !wena_model_identifier_valid(value+9)) return 0;
            strcpy(candidate.swimlane_ids[candidate.swimlane_count++], value+9);
        } else if (!strncmp(value, "list ", 5)) {
            lists = 1;
            if (candidate.list_count == WENA_BOARD_COLLAPSE_CAPACITY ||
                !wena_model_identifier_valid(value+5)) return 0;
            strcpy(candidate.list_ids[candidate.list_count++], value+5);
        } else return 0;
    }
    return 0;
}
static int path_valid(const char *path, int missing)
{
    if (!wena_model_title_string_valid(path, WENA_EXECUTABLE_PATH_CAPACITY)) return 0;
#if defined(_WIN32)
    {
        DWORD attributes, error;
        attributes = GetFileAttributesA(path);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            error = GetLastError();
            return missing && (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND);
        }
        return !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
    }
#else
    {
        struct stat info;
        if (lstat(path, &info)) return missing && errno == ENOENT;
        return S_ISREG(info.st_mode);
    }
#endif
}
int wena_collapse_preferences_load(const char *path, const char *workspace,
    const char *actor, const char *board, WenaBoardCollapseState *state)
{
    char data[PREF_BYTES + 1];
    size_t count;
    int failed;
#if defined(_WIN32)
    HANDLE file;
    BY_HANDLE_FILE_INFORMATION info;
    DWORD got;
#else
    int descriptor;
    FILE *file;
    struct stat info;
#endif
    if (!state || !scope_valid(workspace, actor, board) ||
        !wena_model_title_string_valid(path, WENA_EXECUTABLE_PATH_CAPACITY)) return 0;
#if defined(_WIN32)
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND ? 2 : 0;
    failed = !GetFileInformationByHandle(file, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
    got = 0;
    if (!failed && !ReadFile(file, data, PREF_BYTES, &got, NULL)) failed = 1;
    count = got;
    if (!CloseHandle(file)) failed = 1;
#else
    if (!path_valid(path, 1)) return 0;
    descriptor = open(path, O_RDONLY | O_NONBLOCK
#ifdef O_NOFOLLOW
        | O_NOFOLLOW
#endif
    );
    if (descriptor < 0) return errno == ENOENT ? 2 : 0;
    if (fstat(descriptor, &info) || !S_ISREG(info.st_mode)) { close(descriptor); return 0; }
    file = fdopen(descriptor, "rb");
    if (!file) { close(descriptor); return 0; }
    count = fread(data, 1, PREF_BYTES, file); failed = ferror(file);
    if (fclose(file)) failed = 1;
#endif
    if (failed || count == PREF_BYTES || memchr(data, 0, count)) return 0;
    data[count] = 0;
    return parse(data, workspace, actor, board, state);
}
int wena_collapse_preferences_save(const char *path, const char *workspace,
    const char *actor, const WenaBoardCollapseState *state)
{
    WenaBoardCollapseState old;
    char temporary[WENA_EXECUTABLE_PATH_CAPACITY], data[PREF_BYTES];
    char hex[2 * WENA_EXECUTABLE_PATH_CAPACITY];
    size_t i, length;
    int failed, status;
#if defined(_WIN32)
    HANDLE file;
    DWORD written;
#else
    int descriptor;
    FILE *file;
#endif
    if (!state_valid(state) || !scope_valid(workspace, actor, state->board_id) ||
        !path_valid(path, 1) || strlen(path) + 5 > sizeof(temporary)) return 0;
    status = wena_collapse_preferences_load(path, workspace, actor, state->board_id, &old);
    if (status == WENA_COLLAPSE_PREFS_ERROR) return 0;
    hex_workspace(workspace, hex);
    sprintf(data, "WENA-COLLAPSE 1\nworkspace %s\nactor %s\nboard %s\n", hex, actor, state->board_id);
    for (i = 0; i < state->swimlane_count; ++i) {
        strcat(data, "swimlane "); strcat(data, state->swimlane_ids[i]); strcat(data, "\n");
    }
    for (i = 0; i < state->list_count; ++i) {
        strcat(data, "list "); strcat(data, state->list_ids[i]); strcat(data, "\n");
    }
    strcat(data, "end\n"); length = strlen(data);
    strcpy(temporary, path); strcat(temporary, ".tmp");
#if defined(_WIN32)
    file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    failed = !WriteFile(file, data, (DWORD)length, &written, NULL) || written != length;
    if (!failed && !FlushFileBuffers(file)) failed = 1;
    if (!CloseHandle(file)) failed = 1;
#else
    descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) return 0;
    file = fdopen(descriptor, "wb");
    if (!file) { close(descriptor); remove(temporary); return 0; }
    failed = fwrite(data, 1, length, file) != length;
    if (fflush(file)) failed = 1;
    if (!failed && fsync(descriptor)) failed = 1;
    if (fclose(file)) failed = 1;
#endif
    status = wena_collapse_preferences_load(path, workspace, actor, state->board_id, &old);
    if (failed || status == WENA_COLLAPSE_PREFS_ERROR || !path_valid(path, 1)) {
        remove(temporary); return 0;
    }
#if defined(_WIN32)
    if (!MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
#else
    if (rename(temporary, path)) {
#endif
        remove(temporary); return 0;
    }
    return 1;
}
int wena_collapse_preferences_reset(const char *path, const char *workspace,
    const char *actor, const char *board, WenaBoardCollapseState *state)
{
    WenaBoardCollapseState candidate;
    if (!state || !scope_valid(workspace, actor, board)) return 0;
    memset(&candidate, 0, sizeof(candidate)); strcpy(candidate.board_id, board);
    if (!wena_collapse_preferences_save(path, workspace, actor, &candidate)) return 0;
    *state = candidate; return 1;
}
