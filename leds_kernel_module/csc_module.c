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

#define HW_REGS_BASE    0xff200000
#define HW_REGS_SPAN    0x00200000
#define HW_REGS_MASK    (HW_REGS_SPAN - 1)
#define MAX_DEV	        1

#define DEV_TREE_LABEL  "altr,leds"
#define LED_PIO_BASE    0x10
#define DEVNAME         "Leds Module"

void * LW_virtual; // Lightweight bridge base address
volatile int *LEDR_ptr; // virtual addresses

static int led_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

static const struct file_operations led_dev_ops = {
	.owner 	= THIS_MODULE,
	.write	= led_dev_write
};

struct mychar_device_data {
	struct cdev cdev;
};

static int dev_major = 0;
static struct class *led_dev_class = NULL;
static struct mychar_device_data led_dev_data[MAX_DEV];

static int led_dev_uevent(struct device *dev, struct kobj_uevent_env *env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int init_handler(struct platform_device * pdev){
	int ret;

	//Koppel fysiek geheugenbereik aan pointer
	LW_virtual = ioremap(HW_REGS_BASE, HW_REGS_SPAN);
    printk(KERN_INFO "Init handler.");

	LEDR_ptr = LW_virtual + LED_PIO_BASE; //offset naar PIO registers
	*LEDR_ptr = 0;

	dev_t dev;

    ret = alloc_chrdev_region(&dev, 0, MAX_DEV, "leds_test_module");

    dev_major = MAJOR(dev);

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
static int clean_handler(struct platform_device *pdev)
{

	*LEDR_ptr = 0; // Alle leds_test_module uitzettten
	iounmap (LW_virtual); //mapping ongedaan maken

	int i;

    for (i = 0; i < MAX_DEV; i++) {
        device_destroy(led_dev_class, MKDEV(dev_major, i));
    }

    class_unregister(led_dev_class);
    class_destroy(led_dev_class);

    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

	return 0;
}

static ssize_t led_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    size_t maxdatalen = 30, ncopied;
    uint8_t databuf[maxdatalen];

    printk("Writing device: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

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
    sscanf(databuf, "%i", LEDR_ptr);

    return count;
}


/*
* Hierin beschrijven we welk device we willen koppelen
* aan onze module. Deze moet in de device-tree overeen-
* komen! 
*/
static const struct of_device_id mijn_module_id[] ={
	{.compatible = DEV_TREE_LABEL},
	{}
};

//handlers e.d. koppelen
static struct platform_driver mijn_module_driver = {
	.driver = {
	 	.name = DEVNAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mijn_module_id),
	},
	.probe = init_handler,
	.remove = clean_handler
};

//module registreren
module_platform_driver(mijn_module_driver);