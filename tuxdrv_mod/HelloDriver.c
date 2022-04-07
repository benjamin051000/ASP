#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

int main(void) {
	int fd = open("/dev/tux0", O_WRONLY);
	if(fd == -1) {
		printf("Error: Couldn't open device.\n");
		return -1;
	}

	char* msg = "testing writing\nHello there! \n... general kenobi??";
	printf("message length: %lu\n", strlen(msg));
	write(fd, (void*)msg, strlen(msg));

	// Now, try to read
	// First, reset the offset
	//lseek(fd, 0, SEEK_SET); // lseek not working
	
	close(fd);
	fd = open("/dev/tux0", O_RDONLY);
	char buf[128];
	read(fd, &buf, sizeof buf);
	printf("Read msg: \"%s\"\n", buf);	
	close(fd);
	return 0;
}
