#include "write.h"
#include "bitmap.h"

ssize_t write_v1 (struct file *file, const char __user *buf, size_t sz, loff_t *off) {
	pr_info("write_v1\n");

	uint32_t nr_allocs = 0;
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);

	/* Check if the write can be completed (enough space) */
	if (*off + sz > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;
	nr_allocs = max((size_t)(*off + sz), (size_t)file->f_inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	// Get the number of bloc to create
	int nb_blk_a_alloue = 0;
	int tmp = (*off - inode->i_size) + sz;
	nb_blk_a_alloue = tmp / OUICHEFS_BLOCK_SIZE;
	if ((sz % OUICHEFS_BLOCK_SIZE) != 0)
		nb_blk_a_alloue++;
	
	// Get the bloc with number of other bloc
	struct buffer_head *index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in read v1\n");
		return -EIO;
	}
	struct ouichefs_file_index_block *index;
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	for (int i = 0; i < nb_blk_a_alloue; i++) {
		uint32_t num_blk = get_free_block(sbi);
		if (num_blk == 0) {
			pr_err("Error: get free block\n");
			brelse(index_block);
			return -EFAULT;
		}
		index->blocks[inode->i_blocks - 1] = num_blk;
		inode->i_blocks ++;
	}
	size_t first_blk = *off / sb->s_blocksize;
	size_t bg_off = *off % sb->s_blocksize;
	size_t sz_write = 0;
	size_t sz_left = sz;

	for (int i = first_blk; i < inode->i_blocks - 1 && sz_left > 0; i++) {
		uint32_t num_blk = index->blocks[i];
		struct buffer_head *bh = sb_bread(inode->i_sb, num_blk);
		if (!bh) {
			pr_err("Error sb_bread on block %d\n", num_blk);
			brelse(index_block);
			return -EIO;
		}

		size_t sz_to_write = min(bh->b_size, sz_left);
		if(copy_from_user(bh->b_data + bg_off, buf + sz_write, sz_to_write == 0)) {
			pr_err("Error: copy_from_user failed in write v1\n");
			brelse(bh);
			brelse(index_block);
			return -EFAULT;
		}
		sz_left -= sz_to_write;
		sz_write += sz_to_write;
		bg_off = 0;
		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);

		// Release block
		brelse(bh);
	}
	inode->i_size += tmp;
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);
	// Offset update
	*off += tmp;
	// Release block
	brelse(index_block);
	return sz_write;
}
