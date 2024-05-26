// SPDX-License-Identifier: GPL-2.0

#include "common.h"

void get_first_blk(size_t nb_blk, uint32_t *blocks, loff_t pos, size_t *bg_blk,
		  size_t *sz_read)
{
	for (int i = 0; i < nb_blk - 1; i++) {
		size_t num_blk = blocks[i];
		size_t blk_size_read = 0;

		if (GET_BLK_FULL(num_blk))
			blk_size_read = 4096;
		else
			blk_size_read = GET_BLK_SIZE(num_blk);

		if ((*sz_read + blk_size_read) < pos) {
			*sz_read += blk_size_read;
		} else {
			*bg_blk = i;
			return;
		}
	}
	*bg_blk = -1;
}
