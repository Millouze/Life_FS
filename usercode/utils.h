#ifndef UTILS_H
#define UTILS_H

#include <bits/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#define FILE_MAXOFFSET 4194304
#define BLK_SIZE 4096

char *gen_string(size_t str_sz);
struct timespec get_time();

#endif /* UTILS_H */
