#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "read.h"

ssize_t read_v1(struct file *file, char __user *buf, size_t size, loff_t *pos)
{
	pr_debug("read_v1\n");

	// Get struct
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;

	// Get the bloc with number of other bloc
	struct buffer_head *index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in read v1\n");
		return -EIO;
	}
	struct ouichefs_file_index_block *index;
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	// Init variable
	size_t sz_read = 0;
	size_t sz_left = size;
	size_t first_blk = *pos / sb->s_blocksize;
	size_t bg_off = *pos % sb->s_blocksize;

	// i_blocks - 1 (index bloc)
	for (int i = first_blk; i < inode->i_blocks - 1 && sz_left > 0; i++) {
		// Get block to read
		uint32_t num_blk = index->blocks[i];
		struct buffer_head *bh = sb_bread(inode->i_sb, num_blk);
		if (!bh) {
			pr_err("Error sb_bread on block %d\n", num_blk);
			brelse(index_block);
			return -EIO;
		}

		// size max that we have to read in block
		size_t sz_max = bh->b_size - bg_off;
		size_t sz_cpy = min(sz_left, sz_max);
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

