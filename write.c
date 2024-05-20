#include "write.h"

ssize_t write_v1 (struct file *f, const char __user *buf, size_t sz, loff_t *off) {
	pr_debug("write_v1\n");

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

	size_t first_blk = *off / sb->s_blocksize;
	size_t bg_off = *off % sb->s_blocksize;
}
