#ifndef _READ_H
#define _READ_H

#include <linux/fs.h>

ssize_t read_v1(struct file *, char __user *, size_t, loff_t *);
ssize_t read_v2(struct file *, char __user *, size_t, loff_t *);

#endif /* READ_H */