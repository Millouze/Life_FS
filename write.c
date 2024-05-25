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

	size_t sz_read = 0;
	size_t bg_off = 0;
	size_t first_blk = -1;
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
		pr_info("nb_blk_alloc %zu \n", nb_blk_alloc);
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
			// pr_info("");
			first_blk_sz -= split_sz;
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
	for (int i = inode->i_blocks - 2; i > first_blk; i--)
		index->blocks[i + nb_blk_alloc] = index->blocks[i];

	pr_info("nb_blk_alloc_data %zu\n", nb_blk_alloc);
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
		pr_info("num block %d\n", num_blk);
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
		index->blocks[i] = (sz_cpy == 4096) ?
			index->blocks[i] | BLK_FULL:
			index->blocks[i] | ((sz_cpy << 19) & BLK_SIZE);

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
	brelse(index_block);
out:
	return ret;
}

// static ssize_t fun(struct file *file, const char __user *buf, size_t sz,
// 		   loff_t *pos)
// {
// 	pr_info("Dans la fonction fun\n");
// 	struct inode *inode = file->f_inode;
// 	struct super_block *sb = inode->i_sb;
// 	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
// 	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);

// 	struct buffer_head *index_block = sb_bread(sb, info->index_block);
// 	if (index_block == NULL) {
// 		pr_err("Error: sb_bread for index_block in read v1\n");
// 		return -EIO;
// 	}
// 	struct ouichefs_file_index_block *index;
// 	index = (struct ouichefs_file_index_block *)index_block->b_data;

// 	// Get the number of bloc to create
// 	int nb_blk_a_alloue = 0;
// 	int tmp = (*pos) - inode->i_size;
// 	nb_blk_a_alloue = tmp / OUICHEFS_BLOCK_SIZE;
// 	if ((sz % OUICHEFS_BLOCK_SIZE) != 0)
// 		nb_blk_a_alloue++;

// 	pr_info("In fun: nb_blk_a_alloue = %d\n", nb_blk_a_alloue);

// 	//allocation des blocs de padding
// 	for (int i = 0; i < nb_blk_a_alloue - 1; i++) {
// 		int num_block = get_free_block(sbi);
// 		if (num_block == 0) {
// 			pr_err("Error in get_free_block\n");
// 			brelse(index_block);
// 			return -EFAULT;
// 		}
// 		num_block |= BLK_FULL;
// 		index->blocks[inode->i_blocks - 2] = num_block;
// 		inode->i_blocks++;
// 	}

// 	//Allocation du dernier bloc de padding qui peut aussi avoir de la data
// 	int num_block = get_free_block(sbi);
// 	if (num_block == 0) {
// 		pr_err("Error in get_free_block\n");
// 		brelse(index_block);
// 		return -EFAULT;
// 	}

// 	pr_info("In fun: num_block =%d\n", num_block);

// 	int taille_blk;
// 	if (*pos == 0) {
// 		taille_blk = sz % 4096;
// 		num_block |= (taille_blk << 19);
// 	} else {
// 		taille_blk = ((*pos - 1) % 4096);
// 		num_block |= (taille_blk == 0) ? BLK_FULL :
// 						 (((*pos - 1) % 4096) << 19);
// 	}
// 	int first_blk = inode->i_blocks - 1;
// 	index->blocks[inode->i_blocks - 1] = num_block;
// 	inode->i_blocks++;

// 	pr_info("In fun: num_block =%d after the shenanigans first_blk=%d\n",
// 		num_block & BLK_NUM, first_blk);
// 	//determiner si le permier bloc de data est aussi le dernier bloc de padding
// 	if (taille_blk == 0) {
// 		first_blk++;
// 	}

// 	//calcul du nb de bloc a allouer pour les datas
// 	int nb_b_data = (sz - (*pos % 4096)) / 4096;
// 	if ((sz - (*pos % 4096)) % 4096 == 0) {
// 		nb_b_data++;
// 	}

// 	pr_info("In fun: nb_b_data = %d\n", nb_b_data);

// 	//allocation des blocs de data
// 	for (int i = 0; i < nb_b_data - 1; i++) {
// 		num_block = get_free_block(sbi);
// 		if (num_block == 0) {
// 			pr_err("Error in get_free_block\n");
// 			brelse(index_block);
// 			return -EFAULT;
// 		}
// 		index->blocks[inode->i_blocks - 2] = num_block;
// 		inode->i_blocks++;
// 	}

// 	//remplissage des blocs de data
// 	size_t bg_off = taille_blk;
// 	size_t sz_write = 0;
// 	size_t sz_left = sz;

// 	pr_info("first_blk %d is block number %u et i_blocks %llu\n", first_blk,
// 		index->blocks[first_blk] & BLK_NUM, inode->i_blocks);

// 	for (int i = first_blk; i < inode->i_blocks && sz_left > 0; i++) {
// 		uint32_t num_blk = index->blocks[i];
// 		pr_info("Reading block %u\n", num_blk & BLK_NUM);
// 		struct buffer_head *bh =
// 			sb_bread(inode->i_sb, num_blk & BLK_NUM);
// 		if (!bh) {
// 			pr_err("Error sb_bread on block %u\n", num_blk);
// 			brelse(index_block);
// 			return -EIO;
// 		}

// 		size_t sz_to_write = min(bh->b_size - bg_off, sz_left);
// 		if (copy_from_user(bh->b_data + bg_off, buf + sz_write,
// 				   sz_to_write) != 0) {
// 			pr_err("Error: copy_from_user failed in write v2 fun\n");
// 			brelse(bh);
// 			brelse(index_block);
// 			return -EFAULT;
// 		}
// 		sz_left -= sz_to_write;
// 		sz_write += sz_to_write;
// 		bg_off = 0;
// 		index->blocks[i] = sz_to_write == 4096 ?
// 					   (index->blocks[i] | BLK_FULL) :
// 					   (index->blocks[i] |
// 					    ((sz_to_write << 19) & BLK_SIZE));
// 		mark_buffer_dirty(bh);
// 		sync_dirty_buffer(bh);

// 		// Release block
// 		brelse(bh);
// 	}
// 	inode->i_mtime = inode->i_ctime = current_time(inode);

// 	*pos += sz_write;
// 	inode->i_size += sz_write;
// 	mark_inode_dirty(inode);
// 	//inode->i_blocks+= nb_blk_a_alloue + nb_b_data + 1;
// 	/** faire plus que ça */
// 	// WARNING LEAKS
// 	brelse(index_block);
// 	pr_info("Value returned by fun %zu and inode->i_blocks value: %llu\n",
// 		sz_write, inode->i_blocks);
// 	return sz_write;
// }

// ssize_t write_v2(struct file *file, const char __user *buf, size_t sz,
// 		 loff_t *pos)
// {
// 	pr_info("write_v2\n");

// 	if (!sz)
// 		return sz;

// 	uint32_t nr_allocs = 0;
// 	struct inode *inode = file->f_inode;
// 	struct super_block *sb = inode->i_sb;
// 	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
// 	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);

// 	/* Check if the write can be completed (enough space) */
// 	if (*pos + sz > OUICHEFS_MAX_FILESIZE)
// 		return -ENOSPC;
// 	nr_allocs = max((size_t)(*pos + sz), (size_t)file->f_inode->i_size) /
// 		    OUICHEFS_BLOCK_SIZE;
// 	if (nr_allocs > file->f_inode->i_blocks - 1)
// 		nr_allocs -= file->f_inode->i_blocks - 1;
// 	else
// 		nr_allocs = 0;
// 	if (nr_allocs > sbi->nr_free_blocks)
// 		return -ENOSPC;

// 	if (*pos > inode->i_size) {
// 	}
// 	//sinon
// 	pr_info("Valeur de i_size %lld\n", inode->i_size);
// 	// Get the number of bloc to create
// 	int nb_blk_a_alloue = 0;
// 	nb_blk_a_alloue = sz / OUICHEFS_BLOCK_SIZE;
// 	if ((sz % OUICHEFS_BLOCK_SIZE) != 0)
// 		nb_blk_a_alloue++;
// 	nb_blk_a_alloue++;

// 	if (inode->i_blocks + nb_blk_a_alloue > 1025) {
// 		pr_err("Error too many blocks for a single file\n");
// 		return -ENOSPC;
// 	}

// 	struct buffer_head *index_block = sb_bread(sb, info->index_block);
// 	if (index_block == NULL) {
// 		pr_err("Error: sb_bread for index_block in read v2\n");
// 		return -EIO;
// 	}
// 	struct ouichefs_file_index_block *index;
// 	index = (struct ouichefs_file_index_block *)index_block->b_data;

// 	ssize_t first_blk = -1;
// 	size_t bg_off;
// 	size_t taille_lue = 0;

// 	for (size_t i = 0; i < inode->i_blocks - 1; i++) {
// 		size_t cur_blk = index->blocks[i];
// 		size_t taille_lue_loc = 0;
// 		if ((cur_blk & BLK_FULL)) {
// 			taille_lue_loc = 4096;
// 		} else {
// 			taille_lue_loc = (cur_blk & BLK_SIZE >> 19);
// 		}

// 		if ((taille_lue + taille_lue_loc) < *pos) {
// 			taille_lue += taille_lue_loc;
// 		} else {
// 			first_blk = i;
// 			break;
// 		}
// 	}

// 	pr_info("inode->i_blocks: %llu first_blk: %zu nb_blk_alloue: %d\n",
// 		inode->i_blocks, first_blk & BLK_NUM, nb_blk_a_alloue);
// 	if (first_blk == -1) {
// 		brelse(index_block);
// 		//replace with real function call
// 		return fun(file, buf, sz, pos);
// 	}

// 	bg_off = *pos - taille_lue;

// 	struct buffer_head *bh =
// 		sb_bread(inode->i_sb, index->blocks[first_blk] & BLK_NUM);
// 	if (!bh) {
// 		pr_err("Error sb_bread in a data block %d\n",
// 		       index->blocks[first_blk]);
// 		brelse(index_block);
// 		return -EIO;
// 	}

// 	size_t first_blk_sz =
// 		((index->blocks[first_blk] & BLK_FULL) ?
// 			 4096 :
// 			 ((index->blocks[first_blk] & BLK_SIZE) >> 19));
// 	pr_info("First_blk_size calculated : %zu \n", first_blk_sz);

// 	ssize_t split_sz = 0;
// 	if (first_blk_sz == 0) {
// 		split_sz = first_blk_sz - *pos;
// 	}
// 	pr_info("split_sz calculated : %zd \n", split_sz);
// 	first_blk_sz -= split_sz;

// 	//mise a jour de la taille effective du bloc coupe
// 	index->blocks[first_blk] &= (~BLK_SIZE);
// 	index->blocks[first_blk] |= (first_blk_sz << 19);

// 	//indication que le bloc n'est plus plein
// 	index->blocks[first_blk] &= (~BLK_FULL);

// 	pr_info("Après split calcul du num du block coupé:\n FULL : %d, SIZE: %d, NUM: %d\n",
// 		(index->blocks[first_blk] & BLK_FULL) >> 31,
// 		(index->blocks[first_blk] & BLK_SIZE) >> 19,
// 		index->blocks[first_blk] & BLK_NUM);

// 	// alloue le premier bloc pour le spit
// 	uint32_t num_blk = get_free_block(sbi);
// 	if (num_blk == 0) {
// 		pr_err("Error: get free block\n");
// 		brelse(bh);
// 		brelse(index_block);
// 		return -EFAULT;
// 	}

// 	struct buffer_head *split_bh = sb_bread(sb, num_blk & BLK_NUM);
// 	if (!split_bh) {
// 		pr_err("Error sb_bread in the split block %d\n",
// 		       num_blk & BLK_NUM);
// 		brelse(index_block);
// 		brelse(bh);
// 		return -EIO;
// 	}

// 	pr_info("Split block ok bg_off= %zu, split_sz = %lu, *pos= %llu\n",
// 		bg_off, split_sz, *pos);

// 	if (!memcpy(split_bh->b_data, bh->b_data + bg_off, split_sz)) {
// 		//Problèmes
// 		pr_err("Error memcpy\n");
// 		brelse(bh);
// 		brelse(index_block);
// 		brelse(split_bh);
// 		return -EFAULT;
// 	}

// 	// Si bloc split est plein car on shift toutes les données,
// 	// On mets le flag plein et on positionne le numéro de bloc,
// 	// Sinon on positionne flag non plein et on positionne la taille et le numéro de bloc
// 	size_t split_num = (split_sz == 4096) ?
// 		((BLK_FULL) | (num_blk & BLK_NUM)) :
// 	((0 << 31) | ((split_sz << 19) & BLK_SIZE) | (num_blk & BLK_NUM));
// 	pr_info("inode->i_blocks: %llu first_blk: %zu nb_blk_alloue: %d\n",
// 		inode->i_blocks, first_blk, nb_blk_a_alloue);
// 	for (int i = inode->i_blocks - 2; i > first_blk; i--) {
// 		index->blocks[i + nb_blk_a_alloue] = index->blocks[i];
// 	}
// 	index->blocks[first_blk + nb_blk_a_alloue] = split_num;
// 	inode->i_blocks++; // WARNING AJOUT PREMATURE

// 	for (int i = 1; i < nb_blk_a_alloue; i++) {
// 		uint32_t num_blk = get_free_block(sbi);
// 		if (num_blk == 0) {
// 			pr_err("Error: get free block\n");
// 			brelse(index_block);
// 			return -EFAULT;
// 		}
// 		index->blocks[first_blk + i] = num_blk;
// 		inode->i_blocks++;
// 	}

// 	size_t sz_write = 0;
// 	size_t sz_left = sz;
// 	for (int i = first_blk + 1; i < first_blk + nb_blk_a_alloue; i++) {
// 		uint32_t num_cur_blk = index->blocks[i];
// 		struct buffer_head *bh_iter =
// 			sb_bread(inode->i_sb, num_cur_blk & BLK_NUM);
// 		if (!bh_iter) {
// 			pr_err("Error sb_bread on block %d\n", i);
// 			brelse(index_block);
// 			brelse(bh);
// 			brelse(split_bh);
// 			// il faut faire plus de liberation que ça
// 			return -EIO;
// 		}
// 		size_t sz_to_write = min(bh->b_size, sz_left);
// 		pr_info("Reading block data block %u\n", num_blk & BLK_NUM);
// 		if (copy_from_user(bh->b_data, buf + sz_write, sz_to_write) !=
// 		    0) {
// 			pr_err("Error: copy_from_user failed in write v2\n");
// 			brelse(bh);
// 			brelse(split_bh);
// 			// il faut faire plus de liberation que ça
// 			brelse(index_block);
// 			return -EFAULT;
// 		}
// 		pr_info("bh_bdata %s\n", bh->b_data);
// 		sz_left -= sz_to_write;
// 		sz_write += sz_to_write;

// 		index->blocks[i] = sz_to_write == 4096 ?
// 					   (index->blocks[i] | BLK_FULL) :
// 					   (index->blocks[i] |
// 					    ((sz_to_write << 19) & BLK_SIZE));

// 		mark_buffer_dirty(bh_iter);
// 		sync_dirty_buffer(bh_iter);

// 		// Release block
// 		brelse(bh_iter);
// 	}

// 	// A ENLEVER
// 	//int sz_write = sz;

// 	inode->i_mtime = inode->i_ctime = current_time(inode);
// 	mark_inode_dirty(inode);

// 	*pos += sz_write;
// 	inode->i_size += sz_write;
// 	//faire plus que ça
// 	brelse(bh);
// 	brelse(split_bh);
// 	brelse(index_block);
// 	return sz_write;
// }