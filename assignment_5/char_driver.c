#include <linux/module.h>	/* for modules */
#include <linux/fs.h>		/* file_operations */
#include <linux/uaccess.h>	/* copy_(to,from)_user */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/slab.h>		/* kmalloc */
#include <linux/cdev.h>		/* cdev utilities */

#define MYDEV_NAME "mycdev"
#define RAMDISK_SIZE (size_t) (16 * PAGE_SIZE) // ramdisk size 
#define CDRV_IOC_MAGIC 'Z'
#define ASP_CLEAR_BUF _IOW(CDRV_IOC_MAGIC, 1, int)

//NUM_DEVICES defaults to 3 unless specified during insmod
int NUM_DEVICES = 3;
module_param(NUM_DEVICES, int, S_IRUGO);

int asp_major = 0;
int asp_minor = 0;

struct class * device_class;


struct ASP_mycdrv {
	struct cdev cdev; // Char device structure (NOT a pointer)
	char *ramdisk;
    size_t ramdisk_size;
    struct semaphore sem; // Mutex
	dev_t devNo; // Device number (major and minor)

	// any other field you may want to add
};
struct ASP_mycdrv* devices; // Heap-allocated array of devices.

int mycdrv_open(struct inode* inode, struct file* file) {
    struct ASP_mycdrv* device; // Device information
    pr_info("mycdrv_open()...\n");
    // Get device struct from inode.
    device = container_of(inode->i_cdev, struct ASP_mycdrv, cdev);
    pr_info("\t got device object\n");
    file->private_data = device; // Other functions can use this to get the struct
    pr_info("assigned private_data field to this device.\n");

	pr_info("Opened device: %s%d\n", MYDEV_NAME, MINOR(device->devNo));
    return 0;
}

int mycdrv_release(struct inode *inode, struct file *file) {
    struct ASP_mycdrv* device; // Device information
    // Get device struct from inode.
    device = container_of(inode->i_cdev, struct ASP_mycdrv, cdev);

	pr_info("CLOSING device: %s%d\n", MYDEV_NAME, MINOR(device->devNo));
	return 0;
}

ssize_t mycdrv_read(struct file* file, char __user *buf, size_t lbuf, loff_t * ppos) {
    struct ASP_mycdrv* device = file->private_data;

    int nbytes;
	if ((lbuf + *ppos) > RAMDISK_SIZE) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
    if(down_interruptible(&device->sem)) {
        return -ERESTARTSYS; // From LDD3
    }

	nbytes = lbuf - copy_to_user(buf, device->ramdisk + *ppos, lbuf);
	*ppos += nbytes;

	up(&device->sem);
    pr_info("\tREAD: nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
    return nbytes;
}

ssize_t mycdrv_write(struct file *file, const char __user * buf, size_t lbuf, loff_t * ppos) {
    struct ASP_mycdrv* device = file->private_data;
    
    int nbytes;
	if ((lbuf + *ppos) > RAMDISK_SIZE) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
    if(down_interruptible(&device->sem)) {
        return -ERESTARTSYS; // From LDD3
    }

	nbytes = lbuf - copy_from_user(device->ramdisk + *ppos, buf, lbuf);
	*ppos += nbytes;

    up(&device->sem);
    pr_info("\t[WRITE] nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
    return nbytes;
}

loff_t mycdrv_llseek(struct file* file, loff_t offset, int origin) {
    loff_t new_pos;
    struct ASP_mycdrv* device = file->private_data;
    
    if(down_interruptible(&device->sem)) {
        return -ERESTARTSYS;
    }

    switch(origin) {
        case SEEK_SET: new_pos = offset;
        break;
        case SEEK_CUR: new_pos = file->f_pos + offset;
        break;
        case SEEK_END: new_pos = RAMDISK_SIZE + offset;
        break;
        default:
        return -EINVAL;
    }

    up(&device->sem);
    return new_pos;
}

long mycdrv_ioctl(struct file* file, unsigned cmd, unsigned long arg) {
    struct ASP_mycdrv* device = file->private_data;
    
    // Ensure  the magic number is correct
    if(_IOC_TYPE(cmd) != CDRV_IOC_MAGIC) return -ENOTTY;

    down_interruptible(&device->sem);

    switch(cmd) {
        case ASP_CLEAR_BUF:
            pr_warn("Clearing device buffer...\n");
            memset(device->ramdisk, 0, RAMDISK_SIZE);
        break;
        default:
            pr_info("User specified invalid ioctl command.\n");
            return 1; // Errors
    }
    
    up(&device->sem);

    return 0; // No errors
}


void setup_device(struct ASP_mycdrv* device, int index) {
    const struct file_operations fops = {
        .owner = THIS_MODULE,
        .llseek = mycdrv_llseek,
        .read = mycdrv_read,
        .write = mycdrv_write,
        .unlocked_ioctl = mycdrv_ioctl,
        .open = mycdrv_open,
        .release = mycdrv_release,
    };

    sema_init(&device->sem, 1);
    device->devNo = MKDEV(asp_major, asp_minor + index); // Device stores its own id
    device->cdev.owner = THIS_MODULE;
    cdev_init(&device->cdev, &fops);
    device->ramdisk = kzalloc(RAMDISK_SIZE, GFP_KERNEL); // kzalloc = kmalloc + memset(0). Nice.

    cdev_add(&device->cdev, device->devNo, 1);
    device_create(device_class, NULL, device->devNo, NULL, "%s%d", MYDEV_NAME, index);
    
    pr_info("setup_device: Device \"%s%d\" created.\n", MYDEV_NAME, index);
}

int __init cdrv_init(void) {
    int i, err;
    dev_t devNo;
    struct ASP_mycdrv* device;

    // Register range of device numbers to this driver.
    err = alloc_chrdev_region(&devNo, 0, NUM_DEVICES, MYDEV_NAME);
    if(err < 0) {
        pr_err("Error: Couldn't register chrdev region.\n");
        return 1;
    }
    asp_major = MAJOR(devNo);

    pr_info("Major number (from kernel) = %d\n", asp_major);

    // Allocate devices on the heap.
    devices = kmalloc(NUM_DEVICES*sizeof(struct ASP_mycdrv), GFP_KERNEL);

    if(!devices) {
        pr_err("ERROR: Couldn't allocate devices array.\n");
    }

    memset(devices, 0, NUM_DEVICES*sizeof(struct ASP_mycdrv));

    device_class = class_create(THIS_MODULE, "lab5class");
    
    // Initialize device cdev structs.
    for(i = 0; i < NUM_DEVICES; i++) {
        device = &devices[i]; // Reference to current device
        setup_device(device, i);
    }

    // Everything is set up. Add char device to system
    pr_info("Lab5 module initialized\n");
    return 0;
}

void __exit cdrv_exit(void) {
    int i;
    struct ASP_mycdrv* device;
    pr_info("Removing char_driver...\n");

    for(i = 0; i < NUM_DEVICES; i++) {
        device = &devices[i]; // Reference to current device
        pr_info("Cleaning device %d\n", i);

        pr_info("\tfreeing ramdisk...\n");
        kfree(device->ramdisk);
        
        pr_info("\tdeleting cdev...\n");
        cdev_del(&device->cdev);

        pr_info("\tdestroying device...\n");
        device_destroy(device_class, device->devNo);
    }

    // pr_info("unregistering class\n");
    // class_unregister(device_class);
    
    pr_info("destroying class\n");
    class_destroy(device_class);
    
    pr_info("freeing devices array\n");
    kfree(devices);
    
    pr_info("unregistering chrdev region\n"); 
    unregister_chrdev_region(MKDEV(asp_major, asp_minor), NUM_DEVICES);

    pr_info("Unregistered lab 5 module\n");
}

module_init(cdrv_init);
module_exit(cdrv_exit);

MODULE_AUTHOR("Benjamin Wheeler");
MODULE_LICENSE("GPL v2");
