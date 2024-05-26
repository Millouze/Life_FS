#include "write.h"

#include <linux/buffer_head.h>
#include "ouichefs.h"
#include "bitmap.h"
#include "common.h"

ssize_t write_v1(struct file *file, const char __user *buf, size_t sz, 
							loff_t *pos)
{
	pr_debug("write_v1\n");

	ssize_t ret = 0;
	
	if (sz == 0)
		return 0;

	// Get structs
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;	
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	
	/* Check if the write can be completed (enough space) */
	uint32_t nr_allocs = 0;
	if (*pos + sz > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;
	nr_allocs = max((size_t)(*pos + sz), (size_t)file->f_inode->i_size) /
		    OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	// Get index bloc
	struct buffer_head *index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in read v1\n");
		ret = -EIO;
		goto out;
	}
	struct ouichefs_file_index_block *index;
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	// Get the number of bloc to create
	size_t nb_blk_alloc = 0;
	size_t sz_write = max((size_t)((*pos + sz) - inode->i_size), (size_t)0);
	nb_blk_alloc = sz_write / OUICHEFS_BLOCK_SIZE;
	if ((sz_write % OUICHEFS_BLOCK_SIZE) != 0)
		nb_blk_alloc++;

	sz_write = 0;
	// Alloc all the bloc
	for (int i = 0; i < nb_blk_alloc; i++) {
		size_t num_blk = get_free_block(sbi);
		if (num_blk == 0) {
			pr_err("Error: get free block\n");
			ret = -EFAULT;
			goto free_index_block;
		}
		index->blocks[inode->i_blocks - 1] = num_blk;
		inode->i_blocks++;
	}

	size_t sz_left = sz;
	size_t bg_blk = *pos / sb->s_blocksize;
	size_t bg_off = *pos % sb->s_blocksize;
	struct buffer_head *bh;

	for (int i = bg_blk; i < inode->i_blocks - 1 && sz_left > 0; i++) {
		uint32_t num_blk = index->blocks[i];
		bh = sb_bread(inode->i_sb, num_blk);
		if (!bh) {
			pr_err("Error sb_bread on block %d\n", num_blk);
			ret = -EIO;
			goto free_index_block;
		}

		size_t sz_cpy = min((size_t) OUICHEFS_BLOCK_SIZE - bg_off, sz_left);
		if (copy_from_user(bh->b_data + bg_off, buf + sz_write, sz_cpy)) {
			pr_err("Error: copy_from_user failed in write v1\n");
			ret = -EFAULT;
			goto free_bh;
		}
		sz_left -= sz_cpy;
		sz_write += sz_cpy;
		bg_off = 0;
		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);

		// Release block
		brelse(bh);
	}
	// update inode
	if (sz_write + *pos > inode->i_size)
		inode->i_size = *pos + sz_write;
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);

	// update offset
	*pos += sz_write;
	ret = sz_write;
	goto free_index_block;

free_bh:
	brelse(bh);
free_index_block:
	brelse(index_block);
out :
	return ret;
}

ssize_t write_v2(struct file *file, const char __user *buf, size_t size,
		loff_t *pos)
{
	pr_info("write_v2\n");

	if (size == 0)
		return 0;
	
	// Get structs
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;	
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	
	/* Check if the write can be completed (enough space) */
	uint32_t nr_allocs = 0;
	if (*pos + size > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;
	nr_allocs = max((size_t)(*pos + size), (size_t)file->f_inode->i_size) /
		    OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	int ret;
	// Get index bloc
	struct buffer_head *index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in read v1\n");
		ret = -EIO;
		goto out;
	}
	struct ouichefs_file_index_block *index;
	index = (struct ouichefs_file_index_block *)index_block->b_data;
	pr_info("size %llu\n", inode->i_blocks);
	size_t sz_read = 0;
	size_t bg_off = 0;
	ssize_t first_blk = -1;
	struct buffer_head *bh;
	struct buffer_head *split_bh;
	// Get the number of bloc to create
	// if we have paddoing blocks
	if (*pos > inode->i_size) {
		pr_info("pos %lld > i_size %lld\n", *pos, inode->i_size);
		size_t nb_blk_alloc = 0;
		// Get the number of bloc to create
		size_t sz_write = *pos - inode->i_size;
		nb_blk_alloc = sz_write / OUICHEFS_BLOCK_SIZE;
		if ((sz_write % OUICHEFS_BLOCK_SIZE) != 0)
			nb_blk_alloc++;

		// Allocation of padding blocks
		for (int i = 0; i < nb_blk_alloc -1; i++) {
			size_t num_blk = get_free_block(sbi);
			if (num_blk == 0) {
				pr_err("Error in get_free_block\n");
				ret = -EFAULT;
				goto free_index_block;
			}
			num_blk |= BLK_FULL;
			index->blocks[inode->i_blocks - 2] = num_blk;
			inode->i_blocks++;
		}

		// Allocation last padding bloc
		int num_block = get_free_block(sbi);
		if (num_block == 0) {
			pr_err("Error in get_free_block\n");
			ret = -EFAULT;
			goto free_index_block;
		}

		int blk_size = ((*pos - 1) % 4096);
		num_block |= (blk_size == 0) ? BLK_FULL : ((*pos - 1) % 4096) << 19;

		index->blocks[inode->i_blocks - 1] = num_block;
		first_blk = inode->i_blocks;
		inode->i_blocks++;
	}
	// If pos < file size: split in two the block
	else {
		pr_info("i_size %lld > pos %lld\n", inode->i_size, *pos);
		get_first_blk(inode->i_blocks, index->blocks, *pos, 
						&first_blk, &sz_read);
		if (first_blk != -1) {
			pr_info("first_blk %zu != -1\n", first_blk);
			bg_off = *pos - sz_read;
			size_t first_blk_sz = GET_BLK_SIZE(index->blocks[first_blk]);
			bh = sb_bread(inode->i_sb, index->blocks[first_blk] & BLK_NUM);
			if (!bh) {
				pr_err("Error sb_bread in a data block %d\n",
				index->blocks[first_blk]);
				ret = -EIO;
				goto free_index_block;
			}

			ssize_t split_sz = 0;
			if (first_blk_sz != 0) {
				split_sz = first_blk_sz - bg_off;
			}
			pr_info("split_sz %lu\n", split_sz);
			first_blk_sz -= split_sz;
			pr_info("first blk size %zu \n split %lu \n", first_blk_sz, split_sz);
			if (!(first_blk_sz == 0 || split_sz == 0)) {
				pr_info("+++++++++++pitié je passe pas la \n");
				index->blocks[first_blk] &= (~BLK_SIZE);
				index->blocks[first_blk] |= (first_blk_sz << 19);

				// alloue le premier bloc pour le spit
				uint32_t num_blk = get_free_block(sbi);
				if (num_blk == 0) {
					pr_err("Error: get free block\n");
					ret = -EFAULT;
					goto free_bh;
				}
				if (inode->i_blocks + 1 > 1025) {
					// ToDo DEFRAG
					pr_err("Error too many blocks for a single file\n");
					return -ENOSPC;
				}
				// Split the blocks
				split_bh = sb_bread(sb, num_blk & BLK_NUM);
				if (!split_bh) {
					pr_err("Error sb_bread in the split block %d\n",
					num_blk & BLK_NUM);
					ret = -EFAULT;
					goto free_bh;
				}
				// Copy the split data into the new bloc
				if (!memcpy(split_bh->b_data, bh->b_data + bg_off, split_sz)) {
					pr_err("Error memcpy\n");
					ret = -EFAULT;
					goto free_split_bh;
				}
				// TODO change
				size_t split_num = (split_sz == 4096) ? 
					(BLK_FULL | GET_BLK_NUM(num_blk)) :
				((0 << 31) | ((split_sz << 19) & BLK_SIZE) | (num_blk & BLK_NUM));
				for (int i = inode->i_blocks - 2; i > first_blk; i--)
					index->blocks[i + 1] = index->blocks[i];
				index->blocks[first_blk + 1] = split_num;
				inode->i_blocks++;
				brelse(split_bh);
				brelse(bh);
				first_blk++;
			}
		}
	}
	pr_info("first_blk %zu\n", first_blk);
	// Alloc data blocs
	size_t nb_blk_alloc = size / OUICHEFS_BLOCK_SIZE;
	if ((size % OUICHEFS_BLOCK_SIZE) != 0)
		nb_blk_alloc++;
	
	if (inode->i_blocks + nb_blk_alloc > 1025) {
		// ToDo DEFRAG
		pr_err("Error too many blocks for a single file\n");
		return -ENOSPC;
	}

	pr_info("---inode->i_blocks %llu \n", inode->i_blocks);
	pr_info("---first_blk%ld \n", first_blk);
	pr_info("---nb_blk_alloc %lu \n", nb_blk_alloc);

	if (*pos == 0) {
		first_blk = -1;
	}
	for (int i = inode->i_blocks - 2; i > first_blk; i--) {
		pr_info("Debug: Loop iteration, i = %d, first_blk = %ld\n", i, first_blk);
		index->blocks[i + nb_blk_alloc] = index->blocks[i];
		pr_info("je fais une itération\n");
	}

	for (int i = 0; i < nb_blk_alloc; i++) {
		size_t num_block = get_free_block(sbi);
		if (num_block == 0) {
			pr_err("Error in get_free_block\n");
			brelse(index_block);
			return -EFAULT;
		}
		index->blocks[first_blk + i + 1] = num_block;
		pr_info("indice bloc %zu\n", first_blk + i + 1);
		inode->i_blocks++;
	}
	pr_info("i_size apres ajout des bloc data %llu\n", inode->i_blocks);
	// Fill data blocks
	size_t sz_write = 0;
	size_t sz_left = size;
	for (int i = 0; i < nb_blk_alloc && sz_left > 0; i++) {
		uint32_t num_blk = index->blocks[first_blk + i + 1];
		pr_info("num block %d indice %lu\n", num_blk, first_blk + i + 1);
		bh = sb_bread(inode->i_sb, num_blk & BLK_NUM);
		if (!bh) {
			pr_err("Error sb_bread on block %u\n", num_blk);
			brelse(index_block);
			return -EIO;
		}
		size_t sz_cpy = min(bh->b_size, sz_left);
		pr_info("copy %zu bytes: %s\n", sz_cpy, buf + sz_write);
		if (copy_from_user(bh->b_data + bg_off, buf + sz_write, sz_cpy)) {
			pr_err("Error: copy_from_user failed in write v2 fun\n");
			brelse(bh);
			brelse(index_block);
			return -EFAULT;
		}
		pr_info("b_data %s\n", bh->b_data + bg_off);
		sz_left -= sz_cpy;
		sz_write += sz_cpy;
		bg_off++;
		index->blocks[first_blk + i + 1] = (sz_cpy == 4096) ?
			index->blocks[first_blk + i + 1] | BLK_FULL:
			index->blocks[first_blk + i + 1] | ((sz_cpy << 19) & BLK_SIZE);
		pr_info("size %u\n", GET_BLK_SIZE(index->blocks[i]));
		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);

		// Release block
		brelse(bh);
	}
	// update inode
	inode->i_size += sz_write;
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);

	// update offset
	*pos += sz_write;
	ret = sz_write;
	goto free_index_block;

free_split_bh:
	brelse(split_bh);
free_bh:
	brelse(bh);
free_index_block:
	mark_buffer_dirty(index_block);
	sync_dirty_buffer(index_block);
	brelse(index_block);
out:
	return ret;
}
