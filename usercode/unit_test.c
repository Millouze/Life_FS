#include "utils.h"

int rdwr_check(int fd, off_t offset, char *rd_str)
{
	return 0;
}

int main(int argc, char *argv[])
{
	int fd = open("test1", O_CREAT | O_RDWR | O_TRUNC, 0664);
	char *buf = gen_string(10);
	char *result = { 0 };
	memset(result, 0, strlen(buf));

	srand(time(NULL));
	off_t rd_off = rand() % 500;
	lseek(fd, 0, SEEK_SET);

	if (fd < 0) {
		dprintf(STDERR_FILENO, "UNIT_TEST : Error on main open\n");
		exit(EXIT_FAILURE);
	}

	if (write(fd, buf, 10) == 0) {
		dprintf(STDOUT_FILENO, "UNIT_TEST : write error\n");
		exit(EXIT_FAILURE);
	}

	if (read(fd, result, 10) == 0) {
		dprintf(STDOUT_FILENO, "UNIT_TEST : read error\n");
		exit(EXIT_FAILURE);
	}

	if (strncmp(buf, result, 10)) {
		printf("String Comparison is wrong : Mismatch between value wrote and read\n");
	}

	return 0;
}
