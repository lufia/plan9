#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

/*
 * BUG: never works
 */

int
setgid(gid_t gid)
{
	USED(gid);
	errno = EPERM;
	return -1;
}
