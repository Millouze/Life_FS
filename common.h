/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _COMMON_H
#define _COMMON_H

#include <linux/types.h>

#define BLK_FULL 0x80000000
#define BLK_SIZE 0X7FF80000
#define BLK_NUM 0X7FFF

#define GET_BLK_FULL(val) ((val & BLK_FULL) >> 31)
#define GET_BLK_SIZE(val) ((val & BLK_SIZE) >> 19)
#define GET_BLK_NUM(val)  (val & BLK_NUM)

#define GET_SIZE(val) (GET_BLK_FULL(val) == 1 ? 4096 : GET_BLK_SIZE(val))
#define IS_EMPTY(val) \
	((GET_BLK_FULL(val) == 0 && GET_BLK_SIZE(val) == 0) ? 1 : 0)

/**
 * Get block where pos is located
 */
void get_first_blk(size_t, uint32_t *, loff_t, size_t *, size_t *);

#endif /* COMMON_H */
