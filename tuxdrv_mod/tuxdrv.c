#include <linux/cdev.h>    /* cdev utilities */
#include <linux/fs.h>      /* file_operations */
#include <linux/init.h>    /* module_init, module_exit */
#include <linux/module.h>  /* for modules */
#include <linux/slab.h>    /* kmalloc */
#include <linux/uaccess.h> /* copy_(to,from)_user */

#define MYDEV_NAME "mycdrv"

#define RAMDISK_SIZE (size_t)(16 * PAGE_SIZE)

#define ASP_CLEAR_BUF _IOW('Z', 1, int)

typedef struct {
	struct cdev cdev; // cdev device struct
	dev_t dev_num; // Device number (major and minor)
	char *ramdisk; // Data 
	loff_t data_size; // Location of end of written data (0 to RAMDISK_SIZE)
	struct semaphore sem;
} tuxdrv_t;

tuxdrv_t device; // Module instance

dev_t first;
unsigned int count = 1;
struct class *device_class;

int mycdrv_open(struct inode *inode, struct file *file) {
    pr_info("tuxdrv: open(): %s\n", MYDEV_NAME);
    return 0;
}

int mycdrv_release(struct inode *inode, struct file *file) {
    pr_info("tuxdrv: close(): %s\n", MYDEV_NAME);
    return 0;
}

ssize_t mycdrv_read(struct file *file, char __user *buf, size_t lbuf,
                           loff_t *ppos) {
    int nbytes;
    if ((lbuf + *ppos) > RAMDISK_SIZE) {
        pr_info("trying to read past end of device,"
                "aborting because this is just a stub!\n");
        return 0;
    }

    if (down_interruptible(&device.sem)) {
        return -ERESTARTSYS; // From LDD3
    }

    nbytes = lbuf - copy_to_user(buf, device.ramdisk + *ppos, lbuf);
    *ppos += nbytes;
    pr_info("tuxdrv: read(): nbytes=%d, pos=%d\n", nbytes, (int)*ppos);

    up(&device.sem);

    return nbytes;
}

ssize_t mycdrv_write(struct file *file, const char __user *buf,
                            size_t lbuf, loff_t *ppos) {
    int nbytes;
    if ((lbuf + *ppos) > RAMDISK_SIZE) {
        pr_info("trying to read past end of device,"
                "aborting because this is just a stub!\n");
        return 0;
    }

    if (down_interruptible(&device.sem)) {
        return -ERESTARTSYS; // From LDD3
    }

    nbytes = lbuf - copy_from_user(device.ramdisk + *ppos, buf, lbuf);
    *ppos += nbytes;
    pr_info("tuxdrv: write(): nbytes=%d, pos=%d\n", nbytes, (int)*ppos);

    if (device.data_size < *ppos) device.data_size = *ppos;

    up(&device.sem);

    return nbytes;
}

// TODO reset file position pointer to 0
long mycdrv_ioctl(struct file *file, unsigned cmd, unsigned long arg) {
    // Ensure  the magic number is correct
    if (_IOC_TYPE(cmd) != 'Z') return -ENOTTY;

    if (down_interruptible(&device.sem)) {
        return -ERESTARTSYS; // From LDD3
    }

    switch (cmd) {
    case ASP_CLEAR_BUF:
        pr_info("tuxdrv: Clearing device buffer...\n");
        memset(device.ramdisk, 0, RAMDISK_SIZE);
        break;
    default:
        pr_err("tuxdrv: Invalid ioctl command.\n");
        up(&device.sem);
        return -1; // Error
    }

    up(&device.sem);

    return 0;
}

// TODO In the case of a request that goes beyond end of the buffer, your implementation needs to expand the buffer and fill the new region with zeros.
loff_t mycdrv_llseek(struct file *file, loff_t offset, int origin) {
    loff_t new_pos;

    if (down_interruptible(&device.sem)) {
        return -ERESTARTSYS; // From LDD3
    }

    switch (origin) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = file->f_pos + offset;
        break;
    case SEEK_END:
        new_pos = device.data_size + offset;
        break;
    default:
        up(&device.sem);
        return -EINVAL;
    }

    // Ensure new_pos is actually valid
    if (new_pos < 0) {
        up(&device.sem);
        return -EINVAL;
    }

    pr_info("tuxdrv: lseek(): new_pos=%lld\n", new_pos);

    // Update file pointer
    file->f_pos = new_pos;

    up(&device.sem);

    return new_pos;
}

int __init my_init(void) {
    /* File operations struct. Must have static
    lifetime because it is accessed for the entirety
    of the module's lifetime.*/
    static const struct file_operations FOPS = {
        .owner = THIS_MODULE,
        .read = mycdrv_read,
        .write = mycdrv_write,
        .open = mycdrv_open,
        .release = mycdrv_release,
        .unlocked_ioctl = mycdrv_ioctl,
        .llseek = mycdrv_llseek,
    };

    int err;
    device.ramdisk = kmalloc(RAMDISK_SIZE, GFP_KERNEL);
    err = alloc_chrdev_region(&first, 0, 1, MYDEV_NAME);

    if (err < 0) {
        pr_err("tuxdrv: Couldn't register chrdev region.\n");
        return -1;
    }

    pr_info("tuxdrv: Major number assigned = %d\n", MAJOR(first));

    sema_init(&device.sem, 1); // TODO does this need to be destroyed?

    cdev_init(&device.cdev, &FOPS);
    cdev_add(&device.cdev, first, count);
    pr_info("tuxdrv: Registered!\n");

    device_class = class_create(THIS_MODULE, "tuxdrv_cls");
    device_create(device_class, NULL, first, NULL, "tux0");
    return 0;
}

void __exit my_exit(void) {
    pr_info("tuxdrv: Removing module...\n");

    device_destroy(device_class, first);

    class_destroy(device_class);

    kfree(device.ramdisk);

    cdev_del(&device.cdev);

    unregister_chrdev_region(first, count);

    pr_info("tuxdrv: Module removed.\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Benjamin Wheeler");
MODULE_LICENSE("GPL v2");
