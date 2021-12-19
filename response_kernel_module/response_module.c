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
#define METER_BASE      0x28
#define DEVNAME         "Response Meter Module"
#define MAX_DEV	        1

// 0 (state) en 1 (resp_time) zijn leesbaar. reg 20 (delay) is schrijfbaar.

// Virtual and bridge base addresses
void * LW_virtual;
volatile int *RTM_ptr;

static int rtm_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);
static ssize_t rtm_dev_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static long rtm_dev_ioctl(struct file * file, unsigned int cmd, unsigned long arg);
static int rtm_dev_release(struct inode *inode, struct file *file);

// File operations struct
static const struct file_operations rtm_dev_ops = {
	.owner 	        = THIS_MODULE,
    .read           = rtm_dev_read,
    .write	        = rtm_dev_write,
    .release        = rtm_dev_release,
    .unlocked_ioctl = rtm_dev_ioctl
};

// Character device data
struct rtm_char_dev_data {
	struct cdev cdev;
};

// Needed variables for character devices
static int dev_major = 0;
static struct class *rtm_dev_class = NULL;
static struct rtm_char_dev_data rtm_dev_data[MAX_DEV];
static struct task_struct *task = NULL ;

static int rtm_dev_uevent(struct device *dev, struct kobj_uevent_env *env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

irq_handler_t irq_handler(int irq, void *dev_id, struct pt_regs * regs){
    struct kernel_siginfo info;
    memset(&info, 0, sizeof(struct kernel_siginfo));

    info.si_signo   = 10;                   // nummer 10 voor CSC10 :)
    info.si_code    = SI_QUEUE;
    info.si_int     = *RTM_ptr;             // Contains state of response meter FSM.
    printk( KERN_INFO "In IRQ met %i!\n", info.si_int);

    if (task != NULL) {
        if(send_sig_info (10, & info, task) < 0){
            printk(KERN_INFO "Couldn't send signal to application!\n");
        }
    }

	return (irq_handler_t) IRQ_HANDLED;
}

static int init_handler(struct platform_device * pdev){
    // Map physical memory to pointers and turn displays off
	LW_virtual = ioremap(HW_REGS_BASE, HW_REGS_SPAN);
	RTM_ptr = LW_virtual + METER_BASE;      
    // TODO: Write 0 in delay register.

    int irq_num = platform_get_irq(pdev, 0);
	printk(KERN_ALERT DEVNAME ": IRQ %d wordt geregistreert!\n", irq_num);
	int ret = request_irq(irq_num, (irq_handler_t) irq_handler, 0, DEVNAME, NULL);

    // Configure character device region
	dev_t dev;
    ret |= alloc_chrdev_region(&dev, 0, MAX_DEV, "response_module");
    dev_major = MAJOR(dev);

    // Create character device class
    rtm_dev_class = class_create(THIS_MODULE, "response_module");
    rtm_dev_class->dev_uevent = rtm_dev_uevent;

    int i;
    for (i = 0; i < MAX_DEV; i++) {
        cdev_init(&rtm_dev_data[i].cdev, &rtm_dev_ops);
        rtm_dev_data[i].cdev.owner = THIS_MODULE;

        cdev_add(&rtm_dev_data[i].cdev, MKDEV(dev_major, i), 1);
        device_create(rtm_dev_class, NULL, MKDEV(dev_major, i), NULL, "response_module");
    }

	return ret;
}

static long rtm_dev_ioctl(struct file * file, unsigned int cmd, unsigned long arg){
    if (cmd == _IOW('a', 'a', int32_t *)){
        printk(KERN_INFO "Registering task\n");
        task = get_current();
    }
    return 0;
}

static int rtm_dev_release(struct inode *inode, struct file *file){
    struct task_struct * ref_task = get_current();
    printk ( KERN_INFO " Device bestand vrijgegeven \n") ;
    if(ref_task == task) {
        task = NULL;
    }
    return 0;
}

static int clean_handler(struct platform_device *pdev){
    // Unregister IRQ
    int irq_num = platform_get_irq(pdev, 0);
	printk(KERN_ALERT DEVNAME ": IRQ %d wordt vrijgegeven!\n", irq_num);

    // Undo mapping
	iounmap(LW_virtual);
    free_irq(irq_num, NULL);
    // TODO: Write 0 in delay register

	int i;
    for (i = 0; i < MAX_DEV; i++) {
        device_destroy(rtm_dev_class, MKDEV(dev_major, i));
    }

    // Unregister character device class and region
    class_unregister(rtm_dev_class);
    class_destroy(rtm_dev_class);
    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

	return 0;
}

// Returns response tim in ms.
static ssize_t rtm_dev_read(struct file *file, char __user *buf, size_t count, loff_t *offset){
    uint8_t data[50];
    uint16_t response_time = *(RTM_ptr) >> 16;
    sprintf(data, "%i", response_time);
    size_t datalen = strlen(data);

    if (count > datalen) {
        count = datalen;
    }

    if (copy_to_user(buf, data, count)) {
        return -EFAULT;
    }

    return count;
}

static ssize_t rtm_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset){
    size_t maxdatalen = 30;
    uint8_t databuf[maxdatalen];

    if (count < maxdatalen) {
        maxdatalen = count;
    }

    databuf[maxdatalen] = 0;
    int value;
    sscanf(databuf, "%i", &value);
    // Write value into delay register
    // TODO: Verify if this underneath is right?
    *(RTM_ptr + 7) = value;

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