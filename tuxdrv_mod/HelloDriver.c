#include <unistd.h>
#include <sys/types.h>
#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>


#define CLEAR_BUF _IOW('Z', 1, int)


int main(void) {
	int fd = open("/dev/tux0", O_WRONLY);
	if(fd == -1) {
		printf("Error: Couldn't open device.\n");
		return -1;
	}

	char* msg = "testing writing\nHello there! \n... general kenobi??";
	printf("message length: %lu\n", strlen(msg));
	write(fd, (void*)msg, strlen(msg));
	close(fd);

	// Now, try to read
	// First, reset the offset
	//lseek(fd, 0, SEEK_SET); // lseek not working
	
	fd = open("/dev/tux0", O_RDONLY);
	char buf[128];
	read(fd, &buf, sizeof buf);
	printf("Read msg: \"%s\"\n", buf);	
	close(fd);

	//////////////////////////////////
	// Test ioctl
	//////////////////////////////////
	fd = open("/dev/tux0", O_RDWR);
	int rc = ioctl(fd, CLEAR_BUF, 0);
	if(rc == -1) {
		printf("ERROR: ioctl failed\n");
		close(fd);
		return -1;
	}
	// Now, read to ensure it actually cleared the buffer.
	read(fd, &buf, sizeof buf);
	printf("Read msg (after clear): \"%s\"\n", buf);	
	close(fd);

	return 0;
}
