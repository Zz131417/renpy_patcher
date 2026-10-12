/* util/dir_walk.c —— 见 dir_walk.h */
#include "util/dir_walk.h"
#include "core/plugin.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#else
#include <windows.h>
#endif

static int cmp_entry(const void *a, const void *b) {
    return strcmp(((const file_entry_t*)a)->rel_path, ((const file_entry_t*)b)->rel_path);
}

#ifndef _WIN32
static int walk(const char *base, const char *rel, file_entry_t **arr, size_t *n, size_t *cap) {
    DIR *d = opendir(base);
    if (!d) return RP_ERR_IO;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[2048], r[2048];
        snprintf(full, sizeof(full), "%s/%s", base, e->d_name);
        snprintf(r, sizeof(r), "%s%s%s", rel, rel[0] ? "/" : "", e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            walk(full, r, arr, n, cap);
        } else if (S_ISREG(st.st_mode)) {
            if (*n == *cap) { *cap = *cap ? *cap * 2 : 64; *arr = realloc(*arr, *cap * sizeof(file_entry_t)); }
            if (!*arr) { closedir(d); return RP_ERR_NOMEM; }
            file_entry_t *fe = &(*arr)[*n];
            snprintf(fe->full_path, sizeof(fe->full_path), "%s", full);
            snprintf(fe->rel_path, sizeof(fe->rel_path), "%s", r);
            fe->size = (size_t)st.st_size;
            (*n)++;
        }
    }
    closedir(d);
    return RP_OK;
}
#else
static int walk(const char *base, const char *rel, file_entry_t **arr, size_t *n, size_t *cap) {
    char pattern[2048];
    snprintf(pattern, sizeof(pattern), "%s/*", base);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return RP_OK;  /* 空目录 */
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        char full[2048], r[2048];
        snprintf(full, sizeof(full), "%s/%s", base, fd.cFileName);
        snprintf(r, sizeof(r), "%s%s%s", rel, rel[0] ? "/" : "", fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            walk(full, r, arr, n, cap);
        } else {
            if (*n == *cap) { *cap = *cap ? *cap * 2 : 64; *arr = realloc(*arr, *cap * sizeof(file_entry_t)); }
            if (!*arr) { FindClose(h); return RP_ERR_NOMEM; }
            file_entry_t *fe = &(*arr)[*n];
            snprintf(fe->full_path, sizeof(fe->full_path), "%s", full);
            snprintf(fe->rel_path, sizeof(fe->rel_path), "%s", r);
            fe->size = (size_t)fd.nFileSizeLow;
            (*n)++;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return RP_OK;
}
#endif

int collect_files(const char *dir, file_entry_t **out, size_t *n) {
    *out = NULL; *n = 0; size_t cap = 0;
    int rc = walk(dir, "", out, n, &cap);
    if (rc == RP_OK && *n > 1)
        qsort(*out, *n, sizeof(file_entry_t), cmp_entry);
    return rc;
}

void free_files(file_entry_t *files, size_t n) { (void)n; free(files); }
