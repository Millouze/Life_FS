#ifndef _IOCTL_H
#define _IOCTL_H

#include <linux/fs.h>

long ouichefs_ioctl(struct file *, unsigned int, unsigned long);

#endif /* IOCTL_H */