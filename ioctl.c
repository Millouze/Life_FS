// SPDX-License-Identifier: GPL-2.0

#include "ioctl.h"

#include <linux/buffer_head.h>
#include <linux/ioctl.h>

#include "common.h"
#include "ouichefs.h"
#include "defragmentation.h"

#define OUICH_MAGIC_IOCTL 'N'
#define OUICH_FILE_INFO _IOR(OUICH_MAGIC_IOCTL, 1, char *)
#define OUICH_FILE_DEFRAG _IO(OUICH_MAGIC_IOCTL, 2)

/**
 * Get file
 * number of used blocks
 * number of partially filled blocks
 * number of bytes wasted due to internal fragmentation
 * list of all used blocks with their number and effective size
 */
static int file_info(struct file *file, char *buf)
{
	pr_info(" In file info function :");

	// Get structs
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;
	struct ouichefs_file_index_block *index;
	struct buffer_head *index_block;
	size_t bytes_wasted = 0;

	// Get index bloc
	index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in read v1\n");
		return -EIO;
	}

	index = (struct ouichefs_file_index_block *)index_block->b_data;
	pr_info("number of used block : %llu\n", inode->i_blocks);
	// size_t blk_bytes_wasted = 0;
	for (int i = 0; i < inode->i_blocks - 1; i++) {
		size_t blk_info = index->blocks[i];
		size_t num_blk = GET_BLK_NUM(blk_info);

		if (GET_BLK_FULL(blk_info)) {
			pr_info("Block %zu is full\n", num_blk);
		} else {
			size_t blk_sz = GET_BLK_SIZE(blk_info);
			size_t blk_bytes_wasted = OUICHEFS_BLOCK_SIZE - blk_sz;

			pr_info("Block %zu is partially filled.", num_blk);
			pr_info("    size %zu", blk_sz);
			pr_info("    bytes wastes %zu\n", blk_bytes_wasted);
			bytes_wasted += blk_bytes_wasted;
		}
	}
	pr_info("Total bytes wasted on fragmentation : %zu\n", bytes_wasted);
	return 0;
}

long ouichefs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) != OUICH_MAGIC_IOCTL) {
		pr_err("Wrong magic number in ioctl\n");
		return -EINVAL;
	}

	switch (cmd) {
	case OUICH_FILE_INFO:
		file_info(file, (char *)arg);
		break;

	case OUICH_FILE_DEFRAG:
		ouich_defrag(file);
		break;

	default:
		pr_err("Unsupported IOCTL");
		return -EINVAL;
	}
	return 0;
}
