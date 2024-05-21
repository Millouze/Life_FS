#include "ouichefs_read.h"
#include "linux/buffer_head.h"
#include "linux/fs.h"
#include "linux/printk.h"
#include "ouichefs.h"

ssize_t read_v1(struct file *file, char __user *buff, size_t size, loff_t *off)
{
	pr_info("ouichefs_read_v1\n");

	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;

	sb->s_op->sync_fs(sb,0);

	struct buffer_head *index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in read v1\n");
		return -EIO;
	}

	struct ouichefs_file_index_block *index;
	index = (struct ouichefs_file_index_block *)index_block->b_data;
	pr_info("Data in index block:\n\t %d\n",index_block->b_data[0]);

	size_t size_left = size;
	size_t size_read = 0;
	struct buffer_head *bh;
	int err_nb = 0;

	size_t first_blk = *off / sb->s_blocksize;
	size_t start_offset = *off % sb->s_blocksize;

	for (int i = first_blk; i < inode->i_blocks - 1 && size_left > 0; i++) {
		uint32_t num_blk = index->blocks[i];
		pr_info("DEBUG: Reading block number: %d \n", num_blk);
		//bh = sb_getblk(inode->i_sb, num_blk);
		
		bh = sb_bread(inode->i_sb, num_blk);
		sync_dirty_buffer(bh);
		if (!bh) {
			pr_err("Error sb_bread on block %d\n", num_blk);
			brelse(index_block);
			return -EIO;
		}
		//bh_read(bh, 0);
		pr_info("Size of data in block %zu and data in block:\n\t %d\n",bh->b_size,bh->b_data[0]);
		size_t bh_size_adjusted = bh->b_size - start_offset;
		size_t bytes_to_copy = min(size_left, bh_size_adjusted);
		err_nb = copy_to_user(buff + size_read,
				      bh->b_data + start_offset, bytes_to_copy);
		if (err_nb != 0) {
			pr_err("Error: copy_to_user failed in read v1\n");
			brelse(bh); // Release buffer_head before returning
			brelse(index_block);
			return err_nb;
		}

		size_left -= bytes_to_copy;
		size_read += bytes_to_copy;

		brelse(bh); // Release the current buffer_head
		start_offset = 0; // Reset start_offset after the first block
	}

	*off += size_read;
	brelse(index_block); // Release the index_block at the end

	return size_read;
}
