#include "ioctl.h"

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

	default:
		pr_err("Unsupported IOCTL");
		return -EINVAL;
	}
	return 0;
}
