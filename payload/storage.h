/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TROPHY_STORAGE_H
#define TROPHY_STORAGE_H
#include <stddef.h>
#include <stdint.h>
#define TROPHY_MAX 4096
extern const char *storage_root;
int valid_uid(const char *s);
int valid_set(const char *s);
unsigned char *read_file(const char *path, size_t limit, size_t *size);
unsigned char *archive_entry(const char *set, const char *entry, size_t *size);
unsigned char *game_icon(const char *set,size_t *size);
char *catalog_json(void);
char *state_json(const char *uid,const char *set);
#endif
