// SPDX-License-Identifier: GPL-2.0

#include "read.h"

#include <linux/buffer_head.h>

#include "common.h"
#include "ouichefs.h"

ssize_t read_v1(struct file *file, char __user *buf, size_t size, loff_t *pos)
{
	// Get structs
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;
	struct ouichefs_file_index_block *index;
	struct buffer_head *index_block;

	// Flags managment
	if (file->f_flags & O_WRONLY)
		return -EPERM;

	// If read after EOF or nothing to read
	if (inode->i_size < *pos || size == 0)
		return 0;

	// Get index bloc
	index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in read v1\n");
		return -EIO;
	}
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	size_t sz_read = 0;
	size_t sz_left = size;
	size_t first_blk = *pos / OUICHEFS_BLOCK_SIZE;
	size_t bg_off = *pos % OUICHEFS_BLOCK_SIZE;

	// loop until i_blocks - 1 bc index_bloc count but is not in the array
	for (int i = first_blk; i < inode->i_blocks - 1 && sz_left > 0; i++) {
		// Get block to read
		uint32_t num_blk = index->blocks[i];
		struct buffer_head *bh = sb_bread(sb, num_blk);

		if (!bh) {
			pr_err("Error: sb_bread on block %d\n", num_blk);
			brelse(index_block);
			return -EIO;
		}

		size_t sz_max = OUICHEFS_BLOCK_SIZE - bg_off;
		size_t sz_cpy = min(sz_left, sz_max);
		// if last block
		if (i == inode->i_blocks - 2) {
			sz_cpy = inode->i_size % 4096;
			sz_cpy = ((inode->i_size / 4096) == 0) ? sz_cpy : 4096;
		}
		// if we have nothing to read error
		if (sz_cpy == 0) {
			pr_err("Error: nothing to read\n");
			brelse(bh);
			brelse(index_block);
			return -EFAULT;
		}
		if (copy_to_user(buf + sz_read, bh->b_data + bg_off, sz_cpy)) {
			pr_err("Error: copy_to_user failed in read v1\n");
			brelse(bh);
			brelse(index_block);
			return -EFAULT;
		}

		// update variable
		sz_left -= sz_cpy;
		sz_read += sz_cpy;
		bg_off = 0;

		// Release block
		brelse(bh);
	}
	// Offset update
	*pos += sz_read;
	// Release block
	brelse(index_block);
	return sz_read;
}

ssize_t read_v2(struct file *file, char __user *buf, size_t size, loff_t *pos)
{
	// Get structs
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;
	struct buffer_head *index_block;
	struct ouichefs_file_index_block *index;

	// Flags managment
	if (file->f_flags & O_WRONLY)
		return -EPERM;

	// If read after EOF or nothing to read
	if (inode->i_size < *pos || size == 0)
		return 0;

	// Get index bloc
	index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in read v2\n");
		return -EIO;
	}
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	size_t sz_read = 0;
	size_t sz_left = size;
	ssize_t bg_blk = 0;

	get_first_blk(inode->i_blocks, index->blocks, *pos, &bg_blk, &sz_read);
	if (bg_blk == -1)
		bg_blk = 0;

	size_t bg_off = (*pos - sz_read);

	sz_read = 0;
	// loop until i_blocks - 1 bc index_bloc count but is not in the array
	for (int i = bg_off; i < inode->i_blocks - 1 && sz_left > 0; i++) {
		// Get block to read
		uint32_t num_blk = index->blocks[i];
		struct buffer_head *bh = sb_bread(sb, GET_BLK_NUM(num_blk));

		if (!bh) {
			pr_err("Error: sb_bread on block %d\n", num_blk);
			brelse(index_block);
			return -EIO;
		}

		size_t sz_max = GET_SIZE(num_blk) - bg_off;
		size_t sz_cpy = min(sz_left, sz_max);

		if (sz_cpy == 0)
			continue;
		if (copy_to_user(buf + sz_read, bh->b_data + bg_off, sz_cpy)) {
			pr_err("Error: copy_to_user failed in read v2\n");
			brelse(bh);
			brelse(index_block);
			return -EFAULT;
		}

		// update variable
		sz_left -= sz_cpy;
		sz_read += sz_cpy;
		bg_off = 0;

		// Release block
		brelse(bh);
	}
	// Offset update
	*pos += sz_read;
	// Release block
	brelse(index_block);
	return sz_read;
}
