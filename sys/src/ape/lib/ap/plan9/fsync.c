#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

int
fsync(int fd)
{
	USED(fd);
	/* TODO: should fsync return an error? */
	return 0;
}
