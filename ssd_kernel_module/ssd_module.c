#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

MODULE_LICENSE("GPL");

// Memory management
#define HW_REGS_BASE    0xff200000
#define HW_REGS_SPAN    0x00200000
#define HW_REGS_MASK    (HW_REGS_SPAN - 1)

// Module and hardware configuration
#define DEV_TREE_LABEL  "response,meter"
#define CONTROLLER_BASE 0x20
#define DEVNAME         "Seven Segment Module"
#define MAX_DEV	        1

// Virtual and bridge base addresses
void * LW_virtual;
volatile int *SSD_ptr;

void show_value(unsigned int value){
	int data = value % 10;
	data |= (value / 10 % 10) << 4;
	data |= (value / 100 % 10) << 8;
	data |= (value / 1000 % 10) << 12;
    *(SSD_ptr + 1) = data;

    data =  (value / 10000 % 10);
	data |= (value / 100000 % 10) << 4;
	*SSD_ptr = data;
}

static int ssd_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

// File operations struct
static const struct file_operations ssd_dev_ops = {
	.owner 	= THIS_MODULE,
	.write	= ssd_dev_write
};

// Character device data and configuration
struct ssd_char_dev_data {
	struct cdev cdev;
};

static int dev_major = 0;
static struct class *ssd_dev_class = NULL;
static struct ssd_char_dev_data ssd_dev_data[MAX_DEV];

static int ssd_dev_uevent(struct device *dev, struct kobj_uevent_env *env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int init_handler(struct platform_device * pdev){
    // Map physical memory to pointers and turn displays off
	LW_virtual = ioremap(HW_REGS_BASE, HW_REGS_SPAN);
	SSD_ptr = LW_virtual + CONTROLLER_BASE;
    show_value(0);

    // Configure character device region
	dev_t dev;
    int ret = alloc_chrdev_region(&dev, 0, MAX_DEV, "ssd_test_module");
    dev_major = MAJOR(dev);

    // Create character device class
    ssd_dev_class = class_create(THIS_MODULE, "ssd_test_module");
    ssd_dev_class->dev_uevent = ssd_dev_uevent;

    int i;
    for (i = 0; i < MAX_DEV; i++) {
        cdev_init(&ssd_dev_data[i].cdev, &ssd_dev_ops);
        ssd_dev_data[i].cdev.owner = THIS_MODULE;

        cdev_add(&ssd_dev_data[i].cdev, MKDEV(dev_major, i), 1);
        device_create(ssd_dev_class, NULL, MKDEV(dev_major, i), NULL, "ssd_test_module");
    }

	return ret;
}
static int clean_handler(struct platform_device *pdev){
    // Turn displays off and undo mapping
	show_value(0);
	iounmap (LW_virtual);

	int i;
    for (i = 0; i < MAX_DEV; i++) {
        device_destroy(ssd_dev_class, MKDEV(dev_major, i));
    }

    // Unregister character device class and region
    class_unregister(ssd_dev_class);
    class_destroy(ssd_dev_class);
    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

	return 0;
}

static ssize_t ssd_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset){
    size_t maxdatalen = 30;
    uint8_t databuf[maxdatalen];

    if (count < maxdatalen) {
        maxdatalen = count;
    }

    size_t ncopied = copy_from_user(databuf, buf, maxdatalen);
    if (ncopied == 0) {
        printk("Could't copy %zd bytes from the user\n", ncopied);
    }

    databuf[maxdatalen] = 0;
    int value;
    sscanf(databuf, "%i", &value);
    show_value(value);

    return count;
}

static const struct of_device_id ssd_module_id[] ={
	{.compatible = DEV_TREE_LABEL},
	{}
};

static struct platform_driver ssd_module_driver = {
	.driver = {
	 	.name = DEVNAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ssd_module_id),
	},
	.probe = init_handler,
	.remove = clean_handler
};

module_platform_driver(ssd_module_driver);