#ifndef _WRITE_H
#define _WRITE_H

#include <linux/fs.h>

ssize_t write_v1 (struct file *, const char __user *, size_t, loff_t *);
ssize_t write_v2 (struct file *, const char __user *, size_t, loff_t *);

#endif /* WRITE_H */