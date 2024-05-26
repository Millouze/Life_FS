#include "utils.h"

/**
 * Write buf in fd at whence
 */
time_t sequential_writes(int fd, uint nb_iter, char *buf)
{
	off_t random_off = rand() % FILE_MAXOFFSET;
	lseek(fd, random_off, SEEK_SET);

	struct timespec start = get_time();

	for (int n = 0; n < nb_iter; n++) {
		size_t buf_len = strlen(buf);
		int w = write(fd, buf, buf_len);
		// Verify the return value is correct
		if (w != buf_len) {
			dprintf(STDERR_FILENO,
				"Error: sequential read return %d instead of %d\n",
				w, (int)buf_len);
			close(fd);
			exit(EXIT_FAILURE);
		}
	}

	struct timespec end = get_time();
	time_t delta = ((end.tv_sec - start.tv_sec) * 1000000) +
		       (end.tv_nsec - start.tv_nsec) / 1000;

	return delta;
}

time_t random_writes(int fd, uint nb_iter, char *buf)
{
	struct timespec start = get_time();

	for (int n = 0; n < nb_iter; n++) {
		size_t buf_len = strlen(buf);
		off_t rd_off = rand() % FILE_MAXOFFSET;
		lseek(fd, rd_off, SEEK_SET);
		int w = write(fd, buf, buf_len);

		if (w != buf_len) {
			dprintf(STDERR_FILENO,
				"Error: random write return %d instead of %d\n",
				w, (int)buf_len);
			close(fd);
			exit(EXIT_FAILURE);
		}
	}

	struct timespec end = get_time();

	time_t delta = ((end.tv_sec - start.tv_sec) * 1000) +
		       ((end.tv_nsec - start.tv_nsec) / 100000);

	return delta;
}

time_t sequential_reads(int fd, uint nb_iter, size_t rd_len)
{
	off_t rd_off = 0;
	char buf[rd_len];
	memset(buf, 0, rd_len);
	lseek(fd, rd_off, SEEK_SET);

	struct timespec start = get_time();
	for (int n = 0; n < nb_iter; n++) {
		int r = read(fd, buf, rd_len);

		if (r != rd_len) {
			dprintf(STDERR_FILENO,
				"Error: sequential read return %d instead of %d\n",
				r, (int)rd_len);
			close(fd);
			exit(EXIT_FAILURE);
		}
	}

	struct timespec end = get_time();

	time_t delta = ((end.tv_sec - start.tv_sec) * 1000) +
		       ((end.tv_nsec - start.tv_nsec) / 100000);
	return delta;
}

time_t random_reads(int fd, uint nb_iter, size_t rd_len)
{
	char buf[rd_len];
	memset(buf, 0, rd_len);
	size_t fsz = lseek(fd, 0, SEEK_END);
	struct timespec start = get_time();
	for (int n = 0; n < nb_iter; n++) {
		off_t rd_off = rand() % fsz;
		lseek(fd, rd_off, SEEK_SET);
		int r = read(fd, buf, rd_len);

		if (r != rd_len) {
			dprintf(STDERR_FILENO,
				"Error: random read return %d instead of %d\n",
				r, (int)rd_len);
			close(fd);
			exit(EXIT_FAILURE);
		}
	}

	struct timespec end = get_time();

	time_t delta = ((end.tv_sec - start.tv_sec) * 1000) +
		       (end.tv_nsec - start.tv_nsec) / 100000;
	return delta;
}

time_t complete_read(int fd)
{
	int r = 0;
	char buf[BLK_SIZE * 2];
	memset(buf, 0, BLK_SIZE * 2);
	struct timespec start = get_time();
	lseek(fd, 0, SEEK_SET);
	while ((r = read(fd, buf, BLK_SIZE * 2)) != 0) {
	}

	struct timespec end = get_time();

	time_t delta = ((end.tv_sec - start.tv_sec) * 1000) +
		       (end.tv_nsec - start.tv_nsec) / 100000;
	return delta;
}

int main(int argc, char *argv[])
{
	int fd = open("file", O_CREAT | O_RDWR | O_TRUNC, 0664);
	if (fd < 0) {
		dprintf(STDERR_FILENO, "Error on open\n");
		exit(EXIT_FAILURE);
	}

	unsigned int seed = (int)strtol(argv[1], NULL, 10);
	srand(seed);
	uint nb_iter = rand() % 100;
	size_t rd_len = rand() % 500;
	size_t str_sz = rand() % 30;
	char *buf = gen_string(str_sz);

	time_t exec_time = sequential_writes(fd, nb_iter, buf);
	dprintf(STDOUT_FILENO,
		"Execution time of %u sequential writes on the same file : %lu us\n",
		nb_iter, exec_time);

	exec_time = random_writes(fd, nb_iter, buf);
	dprintf(STDOUT_FILENO,
		"Execution time of %u random writes on the same file : %lu us\n",
		nb_iter, exec_time);

	exec_time = sequential_reads(fd, nb_iter, rd_len);
	dprintf(STDOUT_FILENO,
		"Execution time of %u sequential reads on the same file : %lu us\n",
		nb_iter, exec_time);

	exec_time = random_reads(fd, nb_iter, rd_len);
	dprintf(STDOUT_FILENO,
		"Execution time of %u random read on the same file : %lu us\n",
		nb_iter, exec_time);

	exec_time = complete_read(fd);
	dprintf(STDOUT_FILENO,
		"Execution time of a complete read of a file : %lu us\n",
		exec_time);
	close(fd);
	return 0;
}
