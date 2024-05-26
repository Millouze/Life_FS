#include "utils.h"

char *gen_string(size_t str_sz)
{
	char *alphabet =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
	size_t alpha_len = 62;
	short key = 0;
	uint length = rand() % str_sz;
	char *rd_string = malloc(sizeof(char) * (length + 1));

	if (!rd_string) {
		dprintf(STDERR_FILENO, "No memory\n");
		exit(EXIT_FAILURE);
	}

	for (int n = 0; n < length; n++) {
		key = rand() % alpha_len;
		rd_string[n] = alphabet[key];
	}
	rd_string[length] = '\0';

	return rd_string;
}

struct timespec get_time()
{
	struct timespec res = { 0 };
	if (clock_gettime(CLOCK_MONOTONIC, &res) < 0) {
		dprintf(STDERR_FILENO, "Error on clock get time\n");
		exit(EXIT_FAILURE);
	}
	return res;
}
