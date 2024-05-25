#include "ioctl.h"
#include "linux/buffer_head.h"
#include "linux/fs.h"
#include "linux/sched/signal.h"
#include <string.h>

#define BLK_FULL 0x80000000
#define BLK_SIZE 0X7FF80000
#define BLK_NUM 0X7FFF

int file_info(struct file *file, char *buf)
{
	pr_info("In file info function");
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);

	// Get the bloc with number of other bloc
	struct buffer_head *index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in read v1\n");
		return -EIO;
	}
	struct ouichefs_file_index_block *index;
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	pr_info("number od used block %llu\n", inode->i_blocks);
	size_t blk_bytes_wasted = 0;
	size_t bytes_wasted = 0;
	size_t sz;
	size_t num_blk;
	size_t blk_info;
	for (int i = 0; i < inode->i_blocks - 1; i++) {
		blk_info = index->blocks[i];
		num_blk = (blk_info & BLK_NUM);
		if ((blk_info & BLK_FULL) == 0) {
			sz = (blk_info & BLK_SIZE) >> 19;
			blk_bytes_wasted = OUICHEFS_BLOCK_SIZE - sz;
			bytes_wasted += blk_bytes_wasted;
			pr_info("Block %zu is partially filled \tsize :%zu wastes %zu\n",
				num_blk, sz, blk_bytes_wasted);
		} else {
			pr_info("Block %zu is full\n", num_blk);
		}
	}
	pr_info("Total bytes wasted on fragmentation : %zu\n", bytes_wasted);

	// int ret = copy_to_user(buf, "File informations", sizeof("File informations"));
	return 0;
}

/**
*Returns 0 if the file has no fragmentation
*Frees all empty blocks and pulls following blocks up in the index block list 
*
*/
int checkfrag(struct file *file)
{
	pr_info("In checkfrag function");
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);

	// Get the bloc with number of other bloc
	struct buffer_head *index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in defrag IOCTL\n");
		return -EIO;
	}
	struct ouichefs_file_index_block *index;
	index = (struct ouichefs_file_index_block *)index_block->b_data;

	int frag = 0;
	int blk_num;
	int blk_sz;
	int blk_full;
	struct buffer_head *bh;

	for (int i = 0; i <= inode->i_blocks-2; i++) {
		blk_num = index->blocks[i] & BLK_NUM;
		blk_sz = (index->blocks[i] & BLK_SIZE)>>19;
		blk_full = index->blocks[i] & BLK_FULL;
		bh = sb_bread(sb,blk_num);
		if (bh == NULL) {
			pr_err("Error: sb_bread for index_block in defrag IOCTL\n");
			brelse(index_block);
			return -EIO;
		}

		//bloc qui n'est pas le dernier n'est pas plein ?
		if((i != inode->i_blocks-2) && (!blk_full && blk_sz>0)){
			frag=1;
		}

		//verification que le bloc n'est pas plein et que sa taille est 0
		if( (!blk_full) && (blk_sz == 0) ){
			//bloc vide on le vire
			put_block(sbi,blk_num);
			inode->i_blocks--;
			for(int j = i; j< inode->i_blocks-2; j++){
				index->blocks[j] = index->blocks[j+1];
			}
			i--;	
		}
		
		brelse(bh);
		
	}
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);
	brelse(index_block);
	return frag;
}


/**
*Checks for fragmentation and resolves it if there is some
*/
int ouich_defrag(struct file *file)
{
	pr_info("In file defrag function");
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *info = OUICHEFS_INODE(inode);

	// Get the bloc with number of other bloc
	struct buffer_head *index_block = sb_bread(sb, info->index_block);
	if (index_block == NULL) {
		pr_err("Error: sb_bread for index_block in defrag IOCTL\n");
		return -EIO;
	}
	struct ouichefs_file_index_block *index;
	index = (struct ouichefs_file_index_block *)index_block->b_data;


	int checkret = checkfrag(file);
	if(!checkret){
		pr_info("File has no fragmentation exiting IOCTL");
		return 0;
	}
	
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
	
	char buf[4096];
	
	while(checkret){
		for(int i =0; i <= inode->i_blocks - 2; i++){
					
			blk_numA = index->blocks[i] & BLK_NUM;
			blk_szA = (index->blocks[i] & BLK_SIZE)>>19;
			blk_fullA = index->blocks[i] & BLK_FULL;
			bhA = sb_bread(sb,blk_numA);
			if (bhA == NULL) {
				pr_err("Error: sb_bread bhA in defrag IOCTL\n");
				brelse(index_block);
				return -EIO;
			}
			
			for(int j = i+1; (j<= inode->i_blocks-2) && (!blk_fullA);i++){
			
				blk_numB = index->blocks[j] & BLK_NUM;
				blk_szB = (index->blocks[j] & BLK_SIZE)>>19;
				blk_fullB = index->blocks[j] & BLK_FULL;
				bhB = sb_bread(sb,blk_numB);
				if (bhB == NULL) {
					pr_err("Error: sb_bread bhB in defrag IOCTL\n");
					brelse(index_block);
					return -EIO;
				}
				ponction = min(4096- blk_szA, blk_szB);
				memcpy(bhA->b_data + blk_szA, bhB->b_data, ponction);
				blk_szA += ponction;
				if(blk_szA == 4096){
					blk_fullA = BLK_FULL;
				}
				index->blocks[i] = blk_fullA | ((blk_szA<<19)& BLK_SIZE)|blk_numA;
				if(ponction == blk_szB){
					//On a vidé le bloc B
					blk_szB= 0;
					index->blocks[j] = blk_numB;
				}else{
					//On doit déplacer le contenu du bloc B
					memcpy(buf,bhB->b_data + ponction, blk_szB - ponction );
					memcpy(bhB->b_data, buf,blk_szB - ponction );
					blk_szB -= ponction;
					index->blocks[j] = ((blk_szB<<19)& BLK_SIZE)|blk_numB;
				}
				brelse(bhB);
			}
			
			
			
		}
		checkret = checkfrag(file);
	}
	return 1;
}

long ouichefs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	pr_info("ouichefs IOCTL");

	pr_info("%c\n", _IOC_TYPE(cmd));
	if (_IOC_TYPE(cmd) != OUICH_MAGIC_IOCTL) {
		pr_err("Wrong magic number in ioctl\n");
		return -EINVAL;
	}

	switch (cmd) {
	case OUICH_FILE_INFO:

		file_info(file, (char *)arg);
		break;

	case OUICH_FILE_DEFRAG:
		ouich_defrag(file);
		break;

	default:
		pr_err("Unsupported IOCTL");
		return -EINVAL;
	}
	return 0;
}
