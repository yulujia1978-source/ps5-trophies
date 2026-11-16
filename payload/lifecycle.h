/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TROPHY_LIFECYCLE_H
#define TROPHY_LIFECYCLE_H
#include <stddef.h>
typedef struct {int service_fd,upgrade_fd,owned,replaced;char error[192];char directory[1024];} Instance;
int instance_acquire(Instance *instance,const char *directory,const char *version,unsigned timeout_ms);
void instance_ready(Instance *instance);
void instance_close(Instance *instance);
typedef struct {char file[1400];unsigned missing;unsigned long long checked_ms;} InstallWatch;
void install_watch_init(InstallWatch *watch,const char *root,const char *version);
int install_watch_removed(InstallWatch *watch);
#endif
