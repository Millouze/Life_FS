#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/**
 * Get time
*/
struct timespec get_time()
{
	struct timespec res = { 0 };
	if (clock_gettime(CLOCK_MONOTONIC, &res) < 0) {
		dprintf(STDERR_FILENO, "Error on clock get time\n");
		exit(EXIT_FAILURE);
	}
	return res;
}

/**
 * Write buf in fd at whence
*/
void write_at(int fd, char *buf, int buf_len, int offset, int whence)
{
	// Shift on the 2nd bloc
	off_t before = lseek(fd, offset, whence);

	struct timespec start = get_time();

	int w = write(fd, buf, buf_len);
	// Verify the return value is correct
	if (w != buf_len) {
		dprintf(STDERR_FILENO, "Error: write return %d instead of %d\n", w, buf_len);
		close(fd);
		exit(EXIT_FAILURE);
	}

	struct timespec end = get_time();
	printf("Time of execution of write : %ld ns\n",
	       (end.tv_sec - start.tv_sec) * 1000000000 +
		       (end.tv_nsec - start.tv_nsec));

	// Verify that the offset is up to date
	off_t after = lseek(fd, 0, SEEK_CUR);
	if (before != after - buf_len) {
		dprintf(STDERR_FILENO, "Error : the offset was not updatted\n");
		close(fd);
		exit(EXIT_FAILURE);
	}
}

void read_file(int fd)
{
	lseek(fd, 0, SEEK_SET);
	struct timespec start = get_time();

	char c = 0;
	int i = -1;
	// Check if the file is correct
	while (read(fd, &c, 1) > 0) {
		++i;
		switch (c) {
		case '\0':
			if (i == 2048 || i == 4100 || i == 4099) {
				dprintf(STDERR_FILENO, "Error: missplaced \\0 \n");
				close(fd);
			}
			break;
		case '1':
			if (i != 4097) {
				dprintf(STDERR_FILENO, "Error: missplaced 1\n");
				close(fd);
				exit(EXIT_FAILURE);
			}
			break;
		case '2':
			if (i != 2048) {
				dprintf(STDERR_FILENO, "Error: missplaced 2\n");
				close(fd);
				exit(EXIT_FAILURE);
			}
			break;
		case EOF:
			if (i != 4098) {
				dprintf(STDERR_FILENO, "Error: missplaced EOF\n");
				close(fd);
				exit(EXIT_FAILURE);
			}
			break;
		default:
			dprintf(STDERR_FILENO, "Error: unexpected character\n");
			close(fd);
			exit(EXIT_FAILURE);
		}
	}
	struct timespec end = get_time();

	printf("Time of read all file : %ld ns\n", (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec));
	
	char buf[15] = { 0 };
	// lseek 5 bytes before EOF
	off_t before = lseek(fd, -5, SEEK_END);
	// read more than the end of file
	int r = read(fd, buf, 15);
	// check the return value
	if (r != 5) {
		dprintf(STDERR_FILENO, "Error read result %d instead of %d\n",
			r, 5);
		return;
	}
	// check if offset is up to date
	off_t after = lseek(fd, 0, SEEK_CUR);
	if (before != after - r) {
		dprintf(STDERR_FILENO, "Error : the offset was not updatted\n");
		close(fd);
		exit(EXIT_FAILURE);
	}
	// check if read return 0
	r = read(fd, buf, 1);
	if (r != 0) {
		dprintf(STDERR_FILENO, "Error read result %d instead of %d\n", r , 0);
		close(fd);
		exit(EXIT_FAILURE);
	}
}

int main()
{
	int fd = open("file", O_CREAT | O_RDWR | O_TRUNC, 0664);
	if (fd < 0) {
		dprintf(STDERR_FILENO, "Error on open\n");
		exit(EXIT_FAILURE);
	}

	write_at(fd, "1", 1, 4097, SEEK_SET);
	write_at(fd, "2", 1, 2048, SEEK_SET);

	read_file(fd);

	close(fd);
	return 0;
}
