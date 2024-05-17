#include "ouichefs_read.h"

/* Parameters definition :
size : number of bytes to read, provided by user programm

*/
ssize_t ouichefs_read_v1(struct file *file, char __user *buff, size_t size,
			 loff_t *off)
{
	pr_info("Coucou c'est moi c'est la fonction, comment ça va les copaings ?\n");
	size_t size_left = size;
	size_t size_read = 0;
	int err_nb = 0;
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct buffer_head *index_block = sb_bread(sb, ci->index_block);
	struct buffer_head *bh;
	if (!index_block)
		return -EIO;

	for (int i = 0; inode->i_blocks; i++) {
		uint32_t num_block;
		if (memcpy(&num_block, index_block->b_data + i * 4, 4)) {
			return -1;
		}

		bh = sb_bread(sb, num_block);
		int bytes_left = min(size_left, bh->b_size);
		if ((err_nb = copy_to_user(buff + size_read, bh->b_data,
					   bytes_left)) == 0) {
			pr_err("Error copying memory to user space");
			return err_nb;
		}
		size_left -= bytes_left;
		size_read += bytes_left;
	}
	brelse(index_block);
	brelse(bh);
	return size_read;
}
