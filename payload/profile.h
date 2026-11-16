/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TROPHY_PROFILE_H
#define TROPHY_PROFILE_H
#include <stddef.h>
/* Always returns a terminated display label. ID remains the storage key. */
void profile_name(const char *uid,char *out,size_t size);
#endif
