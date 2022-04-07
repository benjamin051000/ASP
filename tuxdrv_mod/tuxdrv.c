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
#define RAMDISK_SIZE (size_t) (16*PAGE_SIZE)

#define CLEAR_BUF _IOW('Z', 1, int)


static dev_t first;
static unsigned int count = 1;
static struct cdev my_cdev;
loff_t end_of_data = 0; // End of current data in ramdisk

struct class* device_class;

static int mycdrv_open(struct inode *inode, struct file *file)
{
	pr_info("tuxdrv: open(): %s:\n\n", MYDEV_NAME);
	return 0;
}

static int mycdrv_release(struct inode *inode, struct file *file)
{
	pr_info("tuxdrv: close(): %s:\n\n", MYDEV_NAME);
	return 0;
}

static ssize_t
mycdrv_read(struct file *file, char __user * buf, size_t lbuf, loff_t * ppos)
{
	int nbytes;
	if ((lbuf + *ppos) > RAMDISK_SIZE) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	nbytes = lbuf - copy_to_user(buf, ramdisk + *ppos, lbuf);
	*ppos += nbytes;
	pr_info("tuxdrv: read(): nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
	return nbytes;
}

static ssize_t
mycdrv_write(struct file *file, const char __user * buf, size_t lbuf,
	     loff_t * ppos)
{
	int nbytes;
	if ((lbuf + *ppos) > RAMDISK_SIZE) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	nbytes = lbuf - copy_from_user(ramdisk + *ppos, buf, lbuf);
	*ppos += nbytes;
	pr_info("tuxdrv: write(): nbytes=%d, pos=%d\n", nbytes, (int)*ppos);

	if(end_of_data < *ppos)
		end_of_data = *ppos;

	return nbytes;
}

long mycdrv_ioctl(struct file *file, unsigned cmd, unsigned long arg) {
	// Ensure  the magic number is correct
    if(_IOC_TYPE(cmd) != 'Z') return -ENOTTY;

	switch(cmd) {
		case CLEAR_BUF:
			pr_info("tuxdrv: Clearing device buffer...\n");
            memset(ramdisk, 0, RAMDISK_SIZE);
		break;
		default:
		pr_err("tuxdrv: Invalid ioctl command.\n");
		return -1; // Error
	}

	return 0;
}

loff_t mycdrv_llseek(struct file* file, loff_t offset, int origin) {
	loff_t new_pos;
    
    switch(origin) {
        case SEEK_SET: new_pos = offset;
        break;
        case SEEK_CUR: new_pos = file->f_pos + offset;
        break;
        case SEEK_END: new_pos = end_of_data + offset;
        break;
        default:
        return -EINVAL;
    }

    // Ensure new_pos is actually valid
	if(new_pos < 0) return -EINVAL;
	
	pr_info("tuxdrv: lseek(): new_pos=%lld", new_pos);
	
	// Update file pointer
	file->f_pos = new_pos;

    return new_pos;
}

static const struct file_operations mycdrv_fops = {
	.owner = THIS_MODULE,
	.read = mycdrv_read,
	.write = mycdrv_write,
	.open = mycdrv_open,
	.release = mycdrv_release,
	.unlocked_ioctl = mycdrv_ioctl,
	.llseek = mycdrv_llseek,
};

static int __init my_init(void)
{
	int err;
	ramdisk = kmalloc(RAMDISK_SIZE, GFP_KERNEL);
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
