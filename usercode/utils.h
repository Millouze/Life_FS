#ifndef UTILS_H
#define UTILS_H

#include <bits/time.h>
#include <sys/types.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#define OUICH_MAGIC_IOCTL 'N'
#define OUICH_FILE_INFO _IOR(OUICH_MAGIC_IOCTL, 1, char *)
#define OUICH_FILE_DEFRAG _IO(OUICH_MAGIC_IOCTL, 2)

#define FILE_MAXOFFSET 4194304
#define BLK_SIZE 4096
#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define YELLOW "\033[0;33m"
#define COLOR_DEFAULT "\033[0m"

char *gen_string(size_t str_sz);
struct timespec get_time();

#endif /* UTILS_H */
