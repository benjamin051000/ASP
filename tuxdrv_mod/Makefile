
obj-m := tuxdrv.o

KERNEL_DIR = /usr/src/linux-headers-$(shell uname -r)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(shell pwd) modules
	
app: 
	gcc -o userapp HelloDriver.c

clean:
	rm -f *.o *.ko *.mod.* *.symvers *.order *~
	rm -f .tuxdrv.*.cmd .Module.symvers.cmd .modules.order.cmd tuxdrv.mod
	rm -f userapp
