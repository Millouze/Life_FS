/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _WRITE_H
#define _WRITE_H

#include <linux/fs.h>

ssize_t write_v1(struct file *file, const char __user *buf, size_t size,
								loff_t *pos);
ssize_t write_v2(struct file *file, const char __user *buf, size_t size,
								loff_t *pos);

#endif /* WRITE_H */
