#include <linux/module.h>	/* for modules */
#include <linux/fs.h>		/* file_operations */
#include <linux/uaccess.h>	/* copy_(to,from)_user */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/slab.h>		/* kmalloc */
#include <linux/cdev.h>		/* cdev utilities */

#define MYDEV_NAME "mycdrv"

#define ramdisk_size (size_t) (16 * PAGE_SIZE) // ramdisk size 

//NUM_DEVICES defaults to 3 unless specified during insmod
static int NUM_DEVICES = 3;

#define CDRV_IOC_MAGIC 'Z'
#define ASP_CLEAR_BUF _IOW(CDRV_IOC_MAGIC, 1, int)

struct ASP_mycdrv {
	struct cdev dev;
	char *ramdisk;
	struct semaphore sem;
	int devNo;
	// any other field you may want to add
};

static int mycdrv_open(struct inode* inode, struct file* file) {
	pr_info(" OPENING device: %s:\n\n", MYDEV_NAME);
    return 0;
}

static int mycdrv_release(struct inode *inode, struct file *file) {
	pr_info(" CLOSING device: %s:\n\n", MYDEV_NAME);
	return 0;
}

static ssize_t mycdrv_read(struct file* file, char __user *buf, size_t lbuf, loff_t * ppos) {
	pr_info("\n READING function, nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
    return 0;
}

static ssize_t mycdrv_write(struct file *file, const char __user * buf, size_t lbuf, loff_t * ppos) {
    pr_info("\n WRITING function, nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
    return 0;
}



static const struct file_operations mycdrv_fops = {
    .owner = THIS_MODULE,
    .read = mycdrv_read,
    .write = mycdrv_write,
    .open = mycdrv_open,
    .release = mycdrv_release,
};

static int __init cdrv_init(void) {
    pr_info("Registered lab 5 device!\n");
    return 0;
}

static void __exit cdrv_exit(void) {
    pr_info("Unregistered lab 5 device.\n");
}

module_init(cdrv_init);
module_exit(cdrv_exit);

MODULE_AUTHOR("Benjamin Wheeler");
MODULE_LICENSE("GPL v2");
