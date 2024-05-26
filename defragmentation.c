// SPDX-License-Identifier: GPL-2.0

#include <linux/buffer_head.h>

#include "common.h"
#include "ouichefs.h"
#include "bitmap.h"

/**
 * Return if the file need defragmentation
 * Frees all empty blocks and pulls following blocks up in the index block list
 */
static int checkfrag(struct file *file)
{
	// Get structs
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *index_block;

	// Get index bloc
	index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in defrag IOCTL\n");
		return -EIO;
	}
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	int frag = 0;
	int blk_sup = 0;
	int blk_num;
	int blk_sz;
	int blk_full;
	struct buffer_head *bh;

	for (int i = 0; i <= inode->i_blocks - 2; i++) {
		blk_num = GET_BLK_NUM(index->blocks[i]);
		blk_sz = GET_BLK_SIZE(index->blocks[i]);
		blk_full = GET_BLK_FULL(index->blocks[i]);

		bh = sb_bread(sb, blk_num);
		if (bh == NULL) {
			pr_err("Error: sb_bread for index_block in defrag IOCTL\n");
			brelse(index_block);
			return -EIO;
		}

		// If a bloc (not the last) is not full we have to defragment
		if ((i != inode->i_blocks - 2) && (!blk_full && blk_sz > 0))
			frag = 1;

		// If empty bloc -> put
		if ((!blk_full) && (blk_sz == 0)) {
			put_block(sbi, blk_num);
			inode->i_blocks--;
			for (int j = i; j < inode->i_blocks - 2; j++)
				index->blocks[j] = index->blocks[j + 1];
			i--;
			blk_sup = 1;
		}
		brelse(bh);
	}
	if (blk_sup) {
		mark_buffer_dirty(index_block);
		sync_dirty_buffer(index_block);
		inode->i_mtime = inode->i_ctime = current_time(inode);
	}
	brelse(index_block);
	mark_inode_dirty(inode);
	return frag;
}

/**
 * Checks for fragmentation and resolves it if there is some
 */
int ouich_defrag(struct file *file)
{
	int checkret = checkfrag(file);

	if (!checkret)
		return 0;

	// Get structs
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *index_block;

	// Get index bloc
	index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in defrag IOCTL\n");
		return -EIO;
	}
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	// Beyond this point there is fragmentation to be unmade
	int blk_numA;
	int blk_szA;
	int blk_fullA;
	struct buffer_head *bhA;

	int blk_numB;
	int blk_szB;
	int blk_fullB;
	struct buffer_head *bhB;

	int ponction;

	char *buf = kmalloc(sizeof(char) * 4096, GFP_KERNEL);

	while (checkret) {
		for (int i = 0; i <= inode->i_blocks - 2; i++) {
			blk_numA = GET_BLK_NUM(index->blocks[i]);
			blk_szA = GET_BLK_SIZE(index->blocks[i]);
			blk_fullA = GET_BLK_FULL(index->blocks[i]);

			bhA = sb_bread(sb, blk_numA);
			if (bhA == NULL) {
				pr_err("Error: sb_bread bhA in defrag IOCTL\n");
				kfree(buf);
				brelse(index_block);
				return -EIO;
			}

			for (int j = i + 1;
			     (j <= (inode->i_blocks - 2)) && (!blk_fullA);
			     j++) {
				blk_numB = GET_BLK_NUM(index->blocks[j]);
				blk_szB = GET_BLK_SIZE(index->blocks[j]);
				blk_fullB = (GET_BLK_FULL(index->blocks[j]));
				if (blk_fullB) {
					blk_szB = OUICHEFS_BLOCK_SIZE;
				}
				bhB = sb_bread(sb, blk_numB);
				if (bhB == NULL) {
					pr_err("Error: sb_bread bhB in defrag IOCTL\n");
					kfree(buf);
					brelse(index_block);
					return -EIO;
				}
				ponction = min(4096 - blk_szA, blk_szB);
				memcpy(bhA->b_data + blk_szA, bhB->b_data,
				       ponction);
				blk_szA += ponction;
				if (blk_szA == 4096)
					blk_fullA = BLK_FULL;
				index->blocks[i] =
					blk_fullA |
					((blk_szA << 19) & BLK_SIZE) | blk_numA;
				if (ponction == blk_szB) {
					//Block B shoul be empty
					blk_szB = 0;
					index->blocks[j] = blk_numB;
				} else {
					//Remainder of block B data should be pulled at the start of b_data
					memcpy(buf, bhB->b_data + ponction,
					       blk_szB - ponction);
					memcpy(bhB->b_data, buf,
					       blk_szB - ponction);
					blk_szB -= ponction;
					index->blocks[j] =
						((blk_szB << 19) & BLK_SIZE) |
						blk_numB;
				}
				brelse(bhB);
			}
			brelse(bhA);
		}
		checkret = checkfrag(file);
	}
	kfree(buf);
	return 1;
}
