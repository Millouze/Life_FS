/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _READ_H
#define _READ_H

#include <linux/fs.h>

ssize_t read_v1(struct file *file, char __user *buf, size_t size, loff_t *pos);
ssize_t read_v2(struct file *file, char __user *buf, size_t size, loff_t *pos);

#endif /* READ_H */
