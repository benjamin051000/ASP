#include <linux/module.h>	/* for modules */
#include <linux/fs.h>		/* file_operations */
#include <linux/uaccess.h>	/* copy_(to,from)_user */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/slab.h>		/* kmalloc */
#include <linux/cdev.h>		/* cdev utilities */

#define MYDEV_NAME "mycdrv"
#define ramdisk_size (size_t) (16 * PAGE_SIZE) // ramdisk size 
#define CDRV_IOC_MAGIC 'Z'
#define ASP_CLEAR_BUF _IOW(CDRV_IOC_MAGIC, 1, int)

//NUM_DEVICES defaults to 3 unless specified during insmod
int NUM_DEVICES = 3;
module_param(NUM_DEVICES, int, S_IRUGO);

int asp_major = 0;
int asp_minor = 0;

struct class * class;


struct ASP_mycdrv {
	struct cdev cdev; // Char device structure (NOT a pointer)
	char *ramdisk;
    struct semaphore sem; // Mutex
	dev_t devNo; // Device number (major and minor)

	// any other field you may want to add
};
struct ASP_mycdrv* devices; // Heap-allocated array of devices.

int mycdrv_open(struct inode* inode, struct file* file) {
    struct ASP_mycdrv* device; // Device information
    // Get device struct from inode.
    device = container_of(inode->i_cdev, struct ASP_mycdrv, cdev);
    file->private_data = device; // TODO what is this useful for?

	pr_info("OPENING device: %s%d\n", MYDEV_NAME, MINOR(device->devNo));
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
    int nbytes;
	if ((lbuf + *ppos) > ramdisk_size) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	// nbytes = lbuf - copy_to_user(buf, mycdrv.ramdisk + *ppos, lbuf);
	*ppos += nbytes;
	
    pr_info("\tREAD: nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
    return nbytes;
}

ssize_t mycdrv_write(struct file *file, const char __user * buf, size_t lbuf, loff_t * ppos) {
    int nbytes;
	if ((lbuf + *ppos) > ramdisk_size) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	// nbytes = lbuf - copy_from_user(mycdrv.ramdisk + *ppos, buf, lbuf);
	*ppos += nbytes;
    pr_info("\t[WRITE] nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
    return nbytes;
}

int __init cdrv_init(void) {
    const struct file_operations mycdrv_fops = {
        .owner = THIS_MODULE,
        // .llseek = NULL,
        .read = mycdrv_read,
        .write = mycdrv_write,
        // .ioctl = NULL,
        .open = mycdrv_open,
        .release = mycdrv_release,
    };

    // Register range of device numbers to this driver.
    dev_t devNo;
    int error = alloc_chrdev_region(&devNo, 0, NUM_DEVICES, MYDEV_NAME);
    if(error < 0) {
        pr_err("Error: Couldn't register chrdev region.\n");
        return 1;
    }
    asp_major = MAJOR(devNo);

    pr_info("Major number (from kernel) = %d\n", asp_major);

    // Allocate devices on the heap.
    devices = kmalloc(NUM_DEVICES*sizeof(struct ASP_mycdrv), GFP_KERNEL);

    // if(!devices) {
    //     // TODO error handle
    // }

    memset(devices, 0, NUM_DEVICES*sizeof(struct ASP_mycdrv));

    class = class_create(THIS_MODULE, "lab5class");
    
    // Initialize device cdev structs.
    int i;
    for(i = 0; i < NUM_DEVICES; i++) {
        struct ASP_mycdrv* device = &devices[i]; // Reference to current device
        sema_init(&device->sem, 1); // Initialize semaphore
        
        // Initialize char device struct
        device->devNo = MKDEV(asp_major, asp_minor + i); // Set dev number
        cdev_init(&device->cdev, &mycdrv_fops); // Initialize cdev, add fops
        device->cdev.owner = THIS_MODULE; // Set owner (LDD3 says to do this)
        
        // Initialize device's ramdisk
        device->ramdisk = kmalloc(ramdisk_size, GFP_KERNEL);
        memset(device->ramdisk, 0, ramdisk_size); // Reset ramdisk space
        
        // Make the device node.
        device_create(class, NULL, device->devNo, NULL, "mycdrv%d", MINOR(device->devNo)); // TODO do I need to save this value for anything?

        // Add cdev to the system.
        int err = cdev_add(&device->cdev, device->devNo, 1);
        
        if(err) pr_warn("Error %d adding mycdrv%d\n", err, i);
        else pr_info("Device \"mycdrv%d\" created.\n", i);
    }

    // Everything is set up. Add char device to system
    pr_info("Lab5 module initialized\n");
    return 0;
}

void __exit cdrv_exit(void) {
    int i;
    for(i = 0; i < NUM_DEVICES; i++) {
        struct ASP_mycdrv* device = &devices[i]; // Reference to current device
        
        kfree(device->ramdisk);
        cdev_del(&device->cdev);
        device_destroy(class, device->devNo);
    }

    class_unregister(class);
    class_destroy(class);
    
    unregister_chrdev_region(MKDEV(asp_major, asp_minor), NUM_DEVICES);
    pr_info("Unregistered lab 5 module\n");
}

module_init(cdrv_init);
module_exit(cdrv_exit);

MODULE_AUTHOR("Benjamin Wheeler");
MODULE_LICENSE("GPL v2");
