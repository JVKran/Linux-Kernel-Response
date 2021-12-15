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
#define DEV_TREE_LABEL  "altr,leds"
#define LED_PIO_BASE    0x10
#define DEVNAME         "Leds Module"
#define MAX_DEV	        1
#define MAX_DATA_LEN    30

// Virtual and bridge base addresses
void * LW_virtual;
volatile int *LEDR_ptr;

static int led_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

static const struct file_operations led_dev_ops = {
	.owner 	= THIS_MODULE,
	.write	= led_dev_write
};

struct led_char_dev_data {
	struct cdev cdev;
};

static int dev_major = 0;
static struct class *led_dev_class = NULL;
static struct led_char_dev_data led_dev_data[MAX_DEV];

static int led_dev_uevent(struct device *dev, struct kobj_uevent_env *env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int init_handler(struct platform_device * pdev){
    // Map physical memory to pointers and turn leds off
	LW_virtual = ioremap(HW_REGS_BASE, HW_REGS_SPAN);
	LEDR_ptr = LW_virtual + LED_PIO_BASE;
	*LEDR_ptr = 0;

    // Configure character device region
	dev_t dev;
    int ret = alloc_chrdev_region(&dev, 0, MAX_DEV, "leds_test_module");
    dev_major = MAJOR(dev);

    // Create character device class
    led_dev_class = class_create(THIS_MODULE, "leds_test_module");
    led_dev_class->dev_uevent = led_dev_uevent;

    int i;
    for (i = 0; i < MAX_DEV; i++) {
        cdev_init(&led_dev_data[i].cdev, &led_dev_ops);
        led_dev_data[i].cdev.owner = THIS_MODULE;

        cdev_add(&led_dev_data[i].cdev, MKDEV(dev_major, i), 1);
        device_create(led_dev_class, NULL, MKDEV(dev_major, i), NULL, "leds_test_module");
    }

	return ret;
}
static int clean_handler(struct platform_device *pdev){
    // Turn leds off and undo mapping
	*LEDR_ptr = 0;
	iounmap (LW_virtual);

	int i;
    for (i = 0; i < MAX_DEV; i++) {
        device_destroy(led_dev_class, MKDEV(dev_major, i));
    }

    // Unregister character device class and region
    class_unregister(led_dev_class);
    class_destroy(led_dev_class);
    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

	return 0;
}

static ssize_t led_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset){
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
    sscanf(databuf, "%i", LEDR_ptr);

    return count;
}

static const struct of_device_id led_module_id[] ={
	{.compatible = DEV_TREE_LABEL},
	{}
};

static struct platform_driver led_module_driver = {
	.driver = {
	 	.name = DEVNAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(led_module_id),
	},
	.probe = init_handler,
	.remove = clean_handler
};

module_platform_driver(led_module_driver);