#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "write.h"
#include "bitmap.h"

#define BLK_FULL 0x80000000
#define BLK_SIZE 0X7FF80000
#define BLK_NUM  0X7FFF


ssize_t write_v1 (struct file *file, const char __user *buf, size_t sz, loff_t *pos)
{
	pr_info("write_v1\n");

	uint32_t nr_allocs = 0;
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);

	/* Check if the write can be completed (enough space) */
	if (*pos + sz > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;
	nr_allocs = max((size_t)(*pos + sz), (size_t)file->f_inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	// Get the number of bloc to create
	int nb_blk_a_alloue = 0;
	int tmp = max((size_t)((*pos + sz) - inode->i_size), (size_t)0);
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
	size_t first_blk = *pos / sb->s_blocksize;
	size_t bg_off = *pos % sb->s_blocksize;
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

		size_t sz_to_write = min(bh->b_size - bg_off, sz_left);
		if(copy_from_user(bh->b_data + bg_off, buf + sz_write, sz_to_write) != 0) {
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
	if (sz_write + *pos > inode->i_size)
		inode->i_size = *pos + sz_write;
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);
	// Offset update
	*pos += sz_write;
	// Release block
	brelse(index_block);
	return sz_write;
}


ssize_t write_v2 (struct file *file, const char __user *buf, size_t sz, loff_t *pos)
{
	pr_info("write_v2\n");

	uint32_t nr_allocs = 0;
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);

	/* Check if the write can be completed (enough space) */
	if (*pos + sz > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;
	nr_allocs = max((size_t)(*pos + sz), (size_t)file->f_inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	if (*pos > inode->i_size) {

	}
	//sinon

	// Get the number of bloc to create
	int nb_blk_a_alloue = 0;
	nb_blk_a_alloue = sz / OUICHEFS_BLOCK_SIZE;
	if ((sz % OUICHEFS_BLOCK_SIZE) != 0)
		nb_blk_a_alloue++;
	nb_blk_a_alloue++;

	if (inode->i_blocks + nb_blk_a_alloue > 1024) {
		pr_err("Error trop de bloc");
		return -ENOSPC;
	}

	struct buffer_head *index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in read v1\n");
		return -EIO;
	}
	struct ouichefs_file_index_block *index;
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	size_t first_blk;
	size_t bg_off;
	size_t taille_lue = 0;
	

	for (size_t i = 0; i < inode->i_blocks-1; i++)
	{	
		size_t cur_blk = index->blocks[i];
		size_t taille_lue_loc = 0;
		if((cur_blk & BLK_FULL)){
			taille_lue_loc = 4096;
		} 
		else{
			taille_lue_loc = (cur_blk & BLK_SIZE >> 19);
		}

		if((taille_lue + taille_lue_loc) < *pos ){
			taille_lue += taille_lue_loc;
		}
		else{
			first_blk = i;
			break;
		}

	}
	
	bg_off = *pos - taille_lue;

	struct buffer_head *bh = sb_bread(inode->i_sb, index->blocks[first_blk]);
	if (!bh) {
		pr_err("Error sb_bread on block %d\n", index->blocks[first_blk]);
		return -EIO;
	}
	
	size_t first_blk_sz = ((index->blocks[first_blk] & BLK_FULL) ? 4096 :((index->blocks[first_blk] & BLK_SIZE)>>19)) ;
	size_t split_sz = first_blk_sz - *pos;
	first_blk_sz -= split_sz;

	//mise a jour de la taille effective du bloc coupe
	index->blocks[first_blk] &= (~BLK_SIZE);
	index->blocks[first_blk] |= (first_blk_sz<<19);

	//indication que le bloc n'est plus plein
	index->blocks[first_blk] &= (~BLK_FULL);

	// alloue le premier bloc pour le spit
	uint32_t num_blk = get_free_block(sbi);
	if (num_blk == 0) {
		pr_err("Error: get free block\n");
		brelse(bh);
		brelse(index_block);
		return -EFAULT;
	}

	struct buffer_head *split_bh = sb_bread(inode->i_sb, num_blk);

	if ( memcpy(split_bh->b_data,bh->b_data + bg_off,split_sz) ){
		//Problèmes
		pr_err("Error memcpy");
		brelse(bh);
		brelse(index_block);
		brelse(split_bh);	
		return -EFAULT;
	}

	size_t split_num = split_sz == 4096 ? 
	((BLK_FULL) | (num_blk & BLK_NUM)):
	((0 << 31) | ((split_sz <<19) & BLK_SIZE) | (num_blk & BLK_NUM));

	for (int i = inode->i_blocks - 1; i > first_blk ; i--) {
		index->blocks[i + nb_blk_a_alloue] = index->blocks[i];
	}
	index->blocks[first_blk + nb_blk_a_alloue] = split_num;

	for (int i = 1; i < nb_blk_a_alloue; i++) {
		uint32_t num_blk = get_free_block(sbi);
		if (num_blk == 0) {
			pr_err("Error: get free block\n");
			brelse(index_block);
			return -EFAULT;
		}
		index->blocks[first_blk + i] = num_blk;
		inode->i_blocks ++;
	}

	size_t sz_write = 0;
	size_t sz_left = sz;
	for (int i = first_blk + 1; i < first_blk + nb_blk_a_alloue; i++) {
		uint32_t num_cur_blk = index->blocks[i];
		struct buffer_head *bh_iter = sb_bread(inode->i_sb, num_cur_blk);
		if (!bh_iter) {
			pr_err("Error sb_bread on block %d\n", i);
			brelse(index_block);
			/** il faut faire plus de liberation que ça */
			return -EIO;
		}
		size_t sz_to_write = min(bh->b_size, sz_left);
		if(copy_from_user(bh->b_data, buf + sz_write, sz_to_write) != 0) {
			pr_err("Error: copy_from_user failed in write v1\n");
			brelse(bh);
			/** il faut faire plus de liberation que ça */
			brelse(index_block);
			return -EFAULT;
		}
		sz_left -= sz_to_write;
		sz_write += sz_to_write;

		index_block[i] = sz_to_write == 4096 ?
		(index_block[i] | BLK_FULL):
		(index_block[i] | ((sz_to_write << 19) & BLK_SIZE));

		mark_buffer_dirty(bh_iter);
		sync_dirty_buffer(bh_iter);

		// Release block
		brelse(bh_iter);
	}
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);
	
	*pos += sz_write;
	/** faire plus que ça */
	brelse(index_block);
	return sz_write;
}