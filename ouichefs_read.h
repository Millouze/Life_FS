#ifndef _OUICHEFS_READ_H
#define _OUICHEFS_READ_H

#include "linux/fs.h"
#include "linux/printk.h"
#include "linux/uaccess.h"
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include "ouichefs.h"

ssize_t ouichefs_read_v1(struct file *file, char __user *buff,
				   size_t size, loff_t *off);

#endif /* _OUICHEFS_READ_H */
