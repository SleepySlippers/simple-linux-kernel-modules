#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>

MODULE_LICENSE("MIT");
MODULE_AUTHOR("arseny staroverov.ag@phystech.ru");
MODULE_DESCRIPTION("");
MODULE_VERSION("0.0");

#define MY_MAJOR  266 // i had bad experience with `alloc_chrdev_region` so i decided to hardcode
#define MY_MINOR  0
#define MY_DEV_COUNT 1

static dev_t devno = MKDEV(MY_MAJOR, MY_MINOR);

struct cdev cdev;

static struct class *phonebook_class;

struct device *phone_book_dev;

#define PHONEBOOK_MODULE_NAME ("phonebook")

static int device_open(struct inode *inode, struct file *filp) {
    printk(KERN_INFO "Phonebook device opened\n");
    return 0;
}

static int device_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "Phonebook device closed\n");
    return 0;
}

static ssize_t device_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    uint8_t *data = "Hello from the kernel world!\n";
    size_t datalen = strlen(data);

    if (datalen < *offset) {
        pr_err("fiasco: offset overrun the content");
        return 0;
    }

    data += *offset;
    datalen = strlen(data);

    if (count > datalen) {
        count = datalen;
    }

    if (copy_to_user(buf, data, count)) {
        return -EFAULT;
    }

    *offset += count;

    return count;
}

static ssize_t device_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    size_t maxdatalen = 30, ncopied;
    uint8_t databuf[maxdatalen+1];

    if (count < maxdatalen) {
        maxdatalen = count;
    }

    ncopied = copy_from_user(databuf, buf, maxdatalen);

    if (ncopied == 0) {
        printk("Copied %zd bytes from the user\n", maxdatalen);
    } else {
        printk("Could't copy %zd bytes from the user\n", ncopied);
    }

    databuf[maxdatalen] = 0;

    printk("Data from the user: %s\n", databuf);

    return count;
}

struct file_operations phonebook_fops = {
    .owner =   THIS_MODULE,
    .open = device_open,
    .release = device_release,  
    .read = device_read,
    .write = device_write,
};

static int phonebook_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
    printk(KERN_INFO "add uevent var");
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int __init phonebook_init(void) {


    printk(KERN_INFO "Phonebook started\n");

    int rc = -EINVAL;

    rc = register_chrdev_region(devno, MY_DEV_COUNT , PHONEBOOK_MODULE_NAME);
    
    // rc = alloc_chrdev_region(&devno, 0, 1, PHONEBOOK_MODULE_NAME); // sad
    if (rc) {
		pr_err("Unable to create phonebook chrdev: %i\n", rc);
		return rc;
	}
    printk(KERN_INFO "region allocated\n");

    phonebook_class = class_create(PHONEBOOK_MODULE_NAME);
    phonebook_class->dev_uevent = phonebook_uevent;

    cdev_init(&cdev, &phonebook_fops);

    rc = cdev_add(&cdev, devno, 1);
	if (rc) {
		pr_err("cdev_add() failed %d\n", rc);
        return rc;
	}
    printk(KERN_INFO "cdev_added\n");

    phone_book_dev = device_create(phonebook_class, NULL, devno, NULL,
				    "%s-%d", PHONEBOOK_MODULE_NAME, 0);

    if (IS_ERR(phone_book_dev)) {
		rc = PTR_ERR(phone_book_dev);
		pr_err("Unable to create coproc-%d %d\n", MINOR(devno), rc);
        return rc;
	}

    printk(KERN_INFO "device created\n");

    return 0;
}

static void __exit phonebook_exit(void) {
    printk(KERN_INFO "Phonebook exited\n");
}

module_init(phonebook_init);
module_exit(phonebook_exit);