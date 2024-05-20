#ifndef _READ_H
#define _READ_H

#include <linux/buffer_head.h>
#include <linux/types.h>
#include "ouichefs.h"

ssize_t read_v1(struct file *, char __user *, size_t, loff_t *);

#endif /* READ_H */
