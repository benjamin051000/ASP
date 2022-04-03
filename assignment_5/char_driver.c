#include <linux/module.h>	/* for modules */
#include <linux/fs.h>		/* file_operations */
#include <linux/uaccess.h>	/* copy_(to,from)_user */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/slab.h>		/* kmalloc */
#include <linux/cdev.h>		/* cdev utilities */

#define MYDEV_NAME "mycdrv"

#define ramdisk_size (size_t) (16 * PAGE_SIZE) // ramdisk size 

//NUM_DEVICES defaults to 3 unless specified during insmod
// static int NUM_DEVICES = 3;
static unsigned count = 1;


#define CDRV_IOC_MAGIC 'Z'
#define ASP_CLEAR_BUF _IOW(CDRV_IOC_MAGIC, 1, int)

struct ASP_mycdrv {
	struct cdev dev; // Char device structure (NOT a pointer)
	char *ramdisk;
    struct semaphore sem; // Mutex
	dev_t devNo; // Device number (major and minor)

	// any other field you may want to add
};
struct ASP_mycdrv mycdrv;


static int mycdrv_open(struct inode* inode, struct file* file) {
	pr_info("OPENING device: %s\n\n", MYDEV_NAME);
    return 0;
}

static int mycdrv_release(struct inode *inode, struct file *file) {
	pr_info("CLOSING device: %s\n\n", MYDEV_NAME);
	return 0;
}

static ssize_t mycdrv_read(struct file* file, char __user *buf, size_t lbuf, loff_t * ppos) {
    int nbytes;
	if ((lbuf + *ppos) > ramdisk_size) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	nbytes = lbuf - copy_to_user(buf, mycdrv.ramdisk + *ppos, lbuf);
	*ppos += nbytes;
	
    pr_info("\tREAD: nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
    return nbytes;
}

static ssize_t mycdrv_write(struct file *file, const char __user * buf, size_t lbuf, loff_t * ppos) {
	if ((lbuf + *ppos) > ramdisk_size) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	int nbytes = lbuf - copy_from_user(mycdrv.ramdisk + *ppos, buf, lbuf);
	*ppos += nbytes;
    pr_info("\t[WRITE] nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
    return nbytes;
}

static struct class * class;


static int __init cdrv_init(void) {
    // Allocate space for the ramdisk.
    mycdrv.ramdisk = kmalloc(ramdisk_size, GFP_KERNEL);
    // Make device number
    mycdrv.devNo = MKDEV(420, 0);
    // Register range of device numbers to this driver.
    if(register_chrdev_region(mycdrv.devNo, count, MYDEV_NAME)) {
        pr_err("Error: Couldn't register chrdev region.\n");
        return 1;
    }

    const struct file_operations mycdrv_fops = {
        .owner = THIS_MODULE,
        .llseek = NULL,
        .read = mycdrv_read,
        .write = mycdrv_write,
        // .ioctl = NULL,
        .open = mycdrv_open,
        .release = mycdrv_release,
    };

    // Initialize cdev struct
    mycdrv.dev.owner = THIS_MODULE; // https://static.lwn.net/images/pdf/LDD3/ch03.pdf pg 15 says to do this
    cdev_init(&mycdrv.dev, &mycdrv_fops); // Initialize cdev
    class = class_create(THIS_MODULE, "my_class");
    device_create(class, NULL, mycdrv.devNo, NULL, "mycdrv%d", MINOR(mycdrv.devNo));

    // Everything is set up. Add char device to system
    cdev_add(&mycdrv.dev, mycdrv.devNo, count);
    pr_info("Registered lab 5 device!\n");
    return 0;
}

static void __exit cdrv_exit(void) {
    cdev_del(&mycdrv.dev);
    
    device_destroy(class, mycdrv.devNo);
    
    unregister_chrdev_region(mycdrv.devNo, count);
    pr_info("Unregistered lab 5 device.\n");
    kfree(mycdrv.ramdisk);
}

module_init(cdrv_init);
module_exit(cdrv_exit);

MODULE_AUTHOR("Benjamin Wheeler");
MODULE_LICENSE("GPL v2");
