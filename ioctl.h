#ifndef _IOCTL_H
#define _IOCTL_H

#include "ouichefs.h"
#include "bitmap.h"
#include "linux/ioctl.h"
#include <linux/module.h>
#include <linux/buffer_head.h>

#define OUICH_MAGIC_IOCTL 'N'
#define OUICH_FILE_INFO _IOR(OUICH_MAGIC_IOCTL, 1, char *)
#define OUICH_FILE_DEFRAG _IO(OUICH_MAGIC_IOCTL, 2)

long ouichefs_ioctl(struct file *, unsigned int, unsigned long);

#endif /* IOCTL_H */
