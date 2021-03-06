#include <linux/cdev.h>    /* cdev utilities */
#include <linux/fs.h>      /* file_operations */
#include <linux/init.h>    /* module_init, module_exit */
#include <linux/module.h>  /* for modules */
#include <linux/slab.h>    /* kmalloc */
#include <linux/uaccess.h> /* copy_(to,from)_user */
#include <linux/list.h>    /* Linked list operations */

MODULE_AUTHOR("Benjamin Wheeler");
MODULE_LICENSE("GPL v2");

#define MYDEV_NAME "mycdev"
#define RAMDISK_SIZE (size_t)(16 * PAGE_SIZE)
#define ASP_CLEAR_BUF _IOW('Z', 1, int)

int mycdrv_open(struct inode *inode, struct file *file);
int mycdrv_release(struct inode *inode, struct file *file);
ssize_t mycdrv_read(struct file *file, char __user *buf, size_t lbuf, loff_t *ppos);
ssize_t mycdrv_write(struct file *file, const char __user *buf, size_t lbuf, loff_t *ppos);
long mycdrv_ioctl(struct file *file, unsigned cmd, unsigned long arg);
loff_t mycdrv_llseek(struct file *file, loff_t offset, int origin);

/**
 * @brief Specifies the number of devices that should be created on insmod.
 * Default is 3. 
 * Override when running insmod: `insmod tuxdrv.ko NUM_DEVICES=<new num>`
 */
int NUM_DEVICES = 3;
module_param(NUM_DEVICES, int, S_IRUGO);

typedef struct {
	struct cdev cdev; // cdev device struct
	char *ramdisk; // Data 
	loff_t eof; // Location of end of written data (0 to RAMDISK_SIZE)
	struct semaphore sem;
	// int devNo stored in cdev.dev. No need to store it twice
	struct list_head list; // Linked list of devices
} mycdrv_t;

/**
 * @brief Allocate and initialize a new mycdrv_t object.
 * 
 * @param list Linked list to add this new object to.
 * @param devNo Device number (major + minor) for this device.
 * @param deviceClass
 */
void mycdrv_t_create(struct list_head* list, dev_t devNo, struct class* deviceClass) {
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

	mycdrv_t* new_device = kmalloc(sizeof(mycdrv_t), GFP_KERNEL);
	
    new_device->ramdisk = kzalloc(RAMDISK_SIZE, GFP_KERNEL);
	sema_init(&new_device->sem, 1);
	cdev_init(&new_device->cdev, &FOPS);
	cdev_add(&new_device->cdev, devNo, 1); // 1 device per struct in this lab.
	device_create(deviceClass, NULL, devNo, NULL, "%s%d", MYDEV_NAME, MINOR(devNo));
	
	// Add to linked list.
	list_add(&new_device->list, list);
}

/**
 * Delete a device and all its associated data.
 */
void mycdrv_t_destroy(mycdrv_t* device, struct class* deviceClass) {
	device_destroy(deviceClass, device->cdev.dev);
	kfree(device->ramdisk);
	cdev_del(&device->cdev);
    pr_notice(MYDEV_NAME ": removing device from linked list...\n");
    // Remove device from list
    // list_del(&device->list); // TODO BUG Does not appear to be working

    // Delete heap-allocated device
    pr_notice(MYDEV_NAME ": freeing device...\n");
    kfree(device);
}

/**
 * @brief Linked list of devices
 */
LIST_HEAD(devices);

dev_t first;
unsigned int count = 1;
struct class *device_class;

/**
 * open() syscall
 */
int mycdrv_open(struct inode *inode, struct file *file) {
	// Set file private data to the device for easy access in other functions.
	mycdrv_t* device = container_of(inode->i_cdev, mycdrv_t, cdev);
	file->private_data = device;

    pr_info(MYDEV_NAME ": open(): %s%d\n", MYDEV_NAME, MINOR(device->cdev.dev));
    return 0;
}

/**
 * close() syscall
 */
int mycdrv_release(struct inode *inode, struct file *file) {
	mycdrv_t* device = file->private_data;

    pr_info(MYDEV_NAME ": close(): %s%d\n", MYDEV_NAME, MINOR(device->cdev.dev));
    return 0;
}

/**
 * read() syscall
 */
ssize_t mycdrv_read(struct file *file, char __user *buf, size_t lbuf,
                           loff_t *ppos) {
	mycdrv_t* device = file->private_data;

    int nbytes;
    if ((lbuf + *ppos) > RAMDISK_SIZE) {
        pr_info("trying to read past end of device,"
                "aborting because this is just a stub!\n");
        return 0;
    }

    if (down_interruptible(&device->sem)) {
        return -ERESTARTSYS; // From LDD3
    }

    nbytes = lbuf - copy_to_user(buf, device->ramdisk + *ppos, lbuf);
    *ppos += nbytes;
    pr_info(MYDEV_NAME ": read(): nbytes=%d, pos=%d\n", nbytes, (int)*ppos);

    up(&device->sem);

    return nbytes;
}

/**
 * write() syscall
 */
ssize_t mycdrv_write(struct file *file, const char __user *buf,
                            size_t lbuf, loff_t *ppos) {
	mycdrv_t* device = file->private_data;

    int nbytes;
    if ((lbuf + *ppos) > RAMDISK_SIZE) {
        pr_info("trying to read past end of device,"
                "aborting because this is just a stub!\n");
        return 0;
    }
    
    if (down_interruptible(&device->sem)) {
        return -ERESTARTSYS; // From LDD3
    }

    nbytes = lbuf - copy_from_user(device->ramdisk + *ppos, buf, lbuf);
    *ppos += nbytes;
    pr_info(MYDEV_NAME ": write(): nbytes=%d, pos=%d\n", nbytes, (int)*ppos);

    if (device->eof < *ppos) device->eof = *ppos;

    up(&device->sem);

    return nbytes;
}

/**
 * ioctl() syscall
 */
long mycdrv_ioctl(struct file *file, unsigned cmd, unsigned long arg) {
	mycdrv_t* device = file->private_data;

    // Ensure  the magic number is correct
    if (_IOC_TYPE(cmd) != 'Z') return -ENOTTY;

    if (down_interruptible(&device->sem)) {
        return -ERESTARTSYS; // From LDD3
    }

    switch (cmd) {
    case ASP_CLEAR_BUF:
        pr_info(MYDEV_NAME ": Clearing device buffer...\n");
        memset(device->ramdisk, 0, RAMDISK_SIZE);
        file->f_pos = 0;
        device->eof = 0;
        break;
    default:
        pr_err(MYDEV_NAME ": Invalid ioctl command.\n");
        up(&device->sem);
        return -1; // Error
    }

    up(&device->sem);

    return 0;
}

/**
 * lseek() syscall
 */
loff_t mycdrv_llseek(struct file *file, loff_t offset, int origin) {
	mycdrv_t* device = file->private_data;

    loff_t new_pos;

    if (down_interruptible(&device->sem)) {
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
        new_pos = device->eof + offset;
        break;
    default:
        up(&device->sem);
        return -EINVAL;
    }

    // Ensure new_pos is actually valid
    if (new_pos < 0) {
        up(&device->sem);
        return -EINVAL;
    }
    else if(new_pos >= RAMDISK_SIZE) {
        // Reallocate ramdisk to fulfill request
        device->ramdisk = krealloc(device->ramdisk, new_pos, GFP_KERNEL);
        // Set new space to zero.
        memset(device->ramdisk + device->eof, 0, new_pos); // first arg may need +1
    }

    pr_info(MYDEV_NAME ": lseek(): new_pos=%lld\n", new_pos);

    // Update file pointer
    file->f_pos = new_pos;

    up(&device->sem);

    return new_pos;
}

/**
 * Module initialization function
 */
int __init my_init(void) {
    int err, i;
    struct list_head* e_ptr;
	mycdrv_t* e;

	pr_info(MYDEV_NAME ": Number of devices: %d", NUM_DEVICES);

    err = alloc_chrdev_region(&first, 0, NUM_DEVICES, MYDEV_NAME);

    if (err < 0) {
        pr_err(MYDEV_NAME ": Couldn't register chrdev region.\n");
        return -1;
    }

    pr_info(MYDEV_NAME ": Major number assigned = %d\n", MAJOR(first));

    device_class = class_create(THIS_MODULE, "tuxdrv_cls");
    // device_create(device_class, NULL, first, NULL, "tux0");

    for(i = 0; i < NUM_DEVICES; i++) {
        dev_t d = MKDEV(MAJOR(first), i);
	    mycdrv_t_create(&devices, d, device_class);
    }

	pr_info(MYDEV_NAME ": Devices created:\n");
	
	list_for_each(e_ptr, &devices) {
		e = list_entry(e_ptr, mycdrv_t, list);
		
		pr_info("\t%s%d\n", MYDEV_NAME, MINOR(e->cdev.dev));
	}

	pr_notice(MYDEV_NAME ": Module created successfully.\n");

    return 0;
}

/**
 * Module exit function
 */
void __exit my_exit(void) {
	struct list_head* e_ptr;
	mycdrv_t* e;

    pr_info(MYDEV_NAME ": Removing module...\n");

	pr_warn("Destroying devices:\n");

	list_for_each(e_ptr, &devices) {
		e = list_entry(e_ptr, mycdrv_t, list);
		
		pr_warn("\t%s%d\n", MYDEV_NAME, MINOR(e->cdev.dev));
		
		mycdrv_t_destroy(e, device_class);

	}

    class_destroy(device_class);

    unregister_chrdev_region(first, count);

    pr_notice(MYDEV_NAME ": Module removed.\n");
}

module_init(my_init);
module_exit(my_exit);
