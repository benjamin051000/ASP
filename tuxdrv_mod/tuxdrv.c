/* **************** LDD:2.0 s_02/lab1_chrdrv.c **************** */
/*
 * The code herein is: Copyright Jerry Cooperstein, 2012
 *
 * This Copyright is retained for the purpose of protecting free
 * redistribution of source.
 *
 *     URL:    http://www.coopj.com
 *     email:  coop@coopj.com
 *
 * The primary maintainer for this code is Jerry Cooperstein
 * The CONTRIBUTORS file (distributed with this
 * file) lists those known to have contributed to the source.
 *
 * This code is distributed under Version 2 of the GNU General Public
 * License, which you should have received with the source.
 *
 */
/* 
Sample Character Driver 
@*/

#include <linux/module.h>	/* for modules */
#include <linux/fs.h>		/* file_operations */
#include <linux/uaccess.h>	/* copy_(to,from)_user */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/slab.h>		/* kmalloc */
#include <linux/cdev.h>		/* cdev utilities */

#define MYDEV_NAME "mycdrv"

static char *ramdisk;
#define ramdisk_size (size_t) (16*PAGE_SIZE)

static dev_t first;
static unsigned int count = 1;
static struct cdev my_cdev;

struct class* device_class;

static int mycdrv_open(struct inode *inode, struct file *file)
{
	pr_info("tuxdrv: OPENING device: %s:\n\n", MYDEV_NAME);
	return 0;
}

static int mycdrv_release(struct inode *inode, struct file *file)
{
	pr_info("tuxdrv: CLOSING device: %s:\n\n", MYDEV_NAME);
	return 0;
}

static ssize_t
mycdrv_read(struct file *file, char __user * buf, size_t lbuf, loff_t * ppos)
{
	int nbytes;
	if ((lbuf + *ppos) > ramdisk_size) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	nbytes = lbuf - copy_to_user(buf, ramdisk + *ppos, lbuf);
	*ppos += nbytes;
	pr_info("tuxdrv: READING function, nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
	return nbytes;
}

static ssize_t
mycdrv_write(struct file *file, const char __user * buf, size_t lbuf,
	     loff_t * ppos)
{
	int nbytes;
	if ((lbuf + *ppos) > ramdisk_size) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	nbytes = lbuf - copy_from_user(ramdisk + *ppos, buf, lbuf);
	*ppos += nbytes;
	pr_info("tuxdrv: WRITING function, nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
	return nbytes;
}

static const struct file_operations mycdrv_fops = {
	.owner = THIS_MODULE,
	.read = mycdrv_read,
	.write = mycdrv_write,
	.open = mycdrv_open,
	.release = mycdrv_release,
};

static int __init my_init(void)
{
	int err;
	ramdisk = kmalloc(ramdisk_size, GFP_KERNEL);
	err = alloc_chrdev_region(&first, 0, 1, MYDEV_NAME);

	if(err < 0) {
		pr_err("tuxdrv: Couldn't register chrdev region.\n");
		return -1;
	}

	pr_info("tuxdrv: Major number assigned = %d\n", MAJOR(first));
	
	cdev_init(&my_cdev, &mycdrv_fops);
	cdev_add(&my_cdev, first, count);
//	pr_info("\nSucceeded in registering character device %s\n", MYDEV_NAME);
	pr_info("tuxdrv: Registered!\n");

	device_class = class_create(THIS_MODULE, "tuxdrv_cls");
	device_create(device_class, NULL, first, NULL, "tux0");
	return 0;
}

static void __exit my_exit(void)
{
	pr_info("tuxdrv: Deinitializing...\n");

	pr_info("tuxdrv: \tDestroying device\n");
	device_destroy(device_class, first);

	pr_info("tuxdrv: \tDestroying class\n");
	class_destroy(device_class);

	pr_info("tuxdrv: \tFreeing ramdisk\n");
	kfree(ramdisk);

	pr_info("tuxdrv: \tDeleting cdev\n");
	cdev_del(&my_cdev);

	pr_info("tuxdrv: \tUnregistering chrdev region\n");
	unregister_chrdev_region(first, count);

	pr_info("tuxdrv: Unregistered. Goodbye.\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Benjamin Wheeler");
MODULE_LICENSE("GPL v2");
