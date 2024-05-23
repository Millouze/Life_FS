#include "asm-generic/errno-base.h"
#include "linux/buffer_head.h"
#include "linux/percpu-refcount.h"
#include "linux/printk.h"

#include "write.h"
#include "bitmap.h"

#define BLK_FULL 0x80000000
#define BLK_SIZE 0X7FF80000
#define BLK_NUM 0X7FFF

ssize_t write_v1(struct file *file, const char __user *buf, size_t sz,
		 loff_t *pos)
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
	nr_allocs = max((size_t)(*pos + sz), (size_t)file->f_inode->i_size) /
		    OUICHEFS_BLOCK_SIZE;
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
		inode->i_blocks++;
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
		if (copy_from_user(bh->b_data + bg_off, buf + sz_write,
				   sz_to_write) != 0) {
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

static ssize_t fun(struct file *file, const char __user *buf, size_t sz,
		   loff_t *pos)
{
	pr_info("Dans la fonction fun\n");
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);

	struct buffer_head *index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in read v1\n");
		return -EIO;
	}
	struct ouichefs_file_index_block *index;
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	// Get the number of bloc to create
	int nb_blk_a_alloue = 0;
	int tmp = (*pos) - inode->i_size;
	nb_blk_a_alloue = tmp / OUICHEFS_BLOCK_SIZE;
	if ((sz % OUICHEFS_BLOCK_SIZE) != 0)
		nb_blk_a_alloue++;

	//allocation des blocs de padding
	for (int i = 0; i < nb_blk_a_alloue - 1; i++) {
		int num_block = get_free_block(sbi);
		if (num_block == 0) {
			pr_err("Error in get_free_block");
			brelse(index_block);
			return -EFAULT;
		}
		num_block |= BLK_FULL;
		index->blocks[inode->i_blocks - 1] = num_block;
		inode->i_blocks++;
	}

	//Allocation du dernier bloc de padding qui peut aussi avoir de la data
	int num_block = get_free_block(sbi);
	if (num_block == 0) {
		pr_err("Error in get_free_block");
		brelse(index_block);
		return -EFAULT;
	}
	int taille_blk = ((*pos - 1) % 4096);
	num_block |= (taille_blk == 0) ? BLK_FULL : (((*pos - 1) % 4096) << 19);
	int first_blk = inode->i_blocks;
	index->blocks[inode->i_blocks++] = num_block;

	//determiner si le permier bloc de data est aussi le dernier bloc de padding
	if (taille_blk == 0) {
		first_blk++;
	}

	//calcul du nb de bloc a allouer pour les datas
	int nb_b_data = (sz - (*pos % 4096)) / 4096;
	if ((sz - (*pos % 4096)) % 4096 == 0) {
		nb_b_data++;
	}

	//allocation des blocs de data
	for (int i = 0; i < nb_b_data - 1; i++) {
		num_block = get_free_block(sbi);
		if (num_block == 0) {
			pr_err("Error in get_free_block");
			brelse(index_block);
			return -EFAULT;
		}
		index->blocks[inode->i_blocks - 1] = num_block;
		inode->i_blocks++;
	}

	//remplissage des blocs de data
	size_t bg_off = taille_blk;
	size_t sz_write = 0;
	size_t sz_left = sz;

	pr_info("first_blk %d et i_blocks %llu\n", first_blk, inode->i_blocks);

	for (int i = first_blk; i < inode->i_blocks && sz_left > 0; i++) {
		uint32_t num_blk = index->blocks[i];
		pr_info("Reading block %u\n", num_blk & BLK_NUM);
		struct buffer_head *bh =
			sb_bread(inode->i_sb, num_blk & BLK_NUM);
		if (!bh) {
			pr_err("Error sb_bread on block %u\n", num_blk);
			brelse(index_block);
			return -EIO;
		}

		size_t sz_to_write = min(bh->b_size - bg_off, sz_left);
		if (copy_from_user(bh->b_data + bg_off, buf + sz_write,
				   sz_to_write) != 0) {
			pr_err("Error: copy_from_user failed in write v1\n");
			brelse(bh);
			brelse(index_block);
			return -EFAULT;
		}
		sz_left -= sz_to_write;
		sz_write += sz_to_write;
		bg_off = 0;
		index->blocks[i] = sz_to_write == 4096 ?
					   (index->blocks[i] | BLK_FULL) :
					   (index->blocks[i] |
					    ((sz_to_write << 19) & BLK_SIZE));
		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);

		// Release block
		brelse(bh);
	}
	inode->i_mtime = inode->i_ctime = current_time(inode);

	*pos += sz_write;
	inode->i_size += sz_write;
	mark_inode_dirty(inode);
	//inode->i_blocks+= nb_blk_a_alloue + nb_b_data + 1;
	/** faire plus que ça */
	brelse(index_block);
	pr_info("Value returned by fun %zu and inode->i_blocks value: %llu\n",
		sz_write, inode->i_blocks);
	return sz_write;
}

ssize_t write_v2(struct file *file, const char __user *buf, size_t sz,
		 loff_t *pos)
{
	pr_info("write_v2\n");

	if (!sz)
		return sz;

	uint32_t nr_allocs = 0;
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);

	/* Check if the write can be completed (enough space) */
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

	if (*pos > inode->i_size) {
	}
	//sinon
	pr_info("Valeur de i_size %lld\n",inode->i_size);
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
		pr_err("Error: sb_bread for index_block in read v2\n");
		return -EIO;
	}
	struct ouichefs_file_index_block *index;
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	size_t first_blk = -1;
	size_t bg_off;
	size_t taille_lue = 0;

	for (size_t i = 0; i < inode->i_blocks - 1; i++) {
		size_t cur_blk = index->blocks[i];
		size_t taille_lue_loc = 0;
		if ((cur_blk & BLK_FULL)) {
			taille_lue_loc = 4096;
		} else {
			taille_lue_loc = (cur_blk & BLK_SIZE >> 19);
		}

		if ((taille_lue + taille_lue_loc) < *pos) {
			taille_lue += taille_lue_loc;
		} else {
			first_blk = i;
			break;
		}
	}

	pr_info("inode->i_blocks: %llu first_blk: %zu nb_blk_alloue: %d\n",
		inode->i_blocks, first_blk, nb_blk_a_alloue);
	if (first_blk == -1) {
		brelse(index_block);
		//replace with real function call
		return fun(file, buf, sz, pos);
	}

	bg_off = *pos - taille_lue;

	struct buffer_head *bh =
		sb_bread(inode->i_sb, index->blocks[first_blk]);
	if (!bh) {
		pr_err("Error sb_bread on block %d\n",
		       index->blocks[first_blk]);
		brelse(index_block);
		return -EIO;
	}

	size_t first_blk_sz =
		((index->blocks[first_blk] & BLK_FULL) ?
			 4096 :
			 ((index->blocks[first_blk] & BLK_SIZE) >> 19));
	size_t split_sz = first_blk_sz - *pos;
	first_blk_sz -= split_sz;

	//mise a jour de la taille effective du bloc coupe
	index->blocks[first_blk] &= (~BLK_SIZE);
	index->blocks[first_blk] |= (first_blk_sz << 19);

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

	struct buffer_head *split_bh = sb_bread(inode->i_sb, num_blk & BLK_NUM);

	if (!memcpy(split_bh->b_data, bh->b_data + bg_off, split_sz)) {
		//Problèmes
		pr_err("Error memcpy\n");
		brelse(bh);
		brelse(index_block);
		brelse(split_bh);
		return -EFAULT;
	}

	size_t split_num = split_sz == 4096 ?
				   ((BLK_FULL) | (num_blk & BLK_NUM)) :
				   ((0 << 31) | ((split_sz << 19) & BLK_SIZE) |
				    (num_blk & BLK_NUM));
	pr_info("inode->i_blocks: %llu first_blk: %zu nb_blk_alloue: %d\n",
		inode->i_blocks, first_blk, nb_blk_a_alloue);
	for (int i = inode->i_blocks - 1; i > first_blk; i--) {
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
		inode->i_blocks++;
	}

	size_t sz_write = 0;
	size_t sz_left = sz;
	for (int i = first_blk + 1; i < first_blk + nb_blk_a_alloue; i++) {
		uint32_t num_cur_blk = index->blocks[i];
		struct buffer_head *bh_iter =
			sb_bread(inode->i_sb, num_cur_blk & BLK_NUM);
		if (!bh_iter) {
			pr_err("Error sb_bread on block %d\n", i);
			brelse(index_block);
			brelse(bh);
			brelse(split_bh);
			// il faut faire plus de liberation que ça 
			return -EIO;
		}
		size_t sz_to_write = min(bh->b_size, sz_left);
		pr_info("Reading block %u\n", num_blk & BLK_NUM);
		if (copy_from_user(bh->b_data, buf + sz_write, sz_to_write) !=
		    0) {
			pr_err("Error: copy_from_user failed in write v2\n");
			brelse(bh);
			brelse(split_bh);
			// il faut faire plus de liberation que ça
			brelse(index_block);
			return -EFAULT;
		}
		sz_left -= sz_to_write;
		sz_write += sz_to_write;

		index->blocks[i] = sz_to_write == 4096 ?
					   (index->blocks[i] | BLK_FULL) :
					   (index->blocks[i] |
					    ((sz_to_write << 19) & BLK_SIZE));

		mark_buffer_dirty(bh_iter);
		sync_dirty_buffer(bh_iter);

		// Release block
		brelse(bh_iter);
	}

	// A ENLEVER
	//int sz_write = sz;

	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);

	*pos += sz_write;
	inode->i_size += sz_write;
	//faire plus que ça
	brelse(bh);
	brelse(split_bh);
	brelse(index_block);
	return sz_write;
}
