#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

static struct class *firstdrv_class;
static struct class_devices *firstdrv_class_dev;

static int first_drv_open(struct inode *inode, struct file *file)
{
	printk("first_drv_open\n");
	return 0;
}
/*
static ssize_t  first_drv_read(struct file *file, char __user *buf, size_t size,loff_t *ppos)
{
	printk("first_drv_read");
	return 0;
}*/
static ssize_t  first_drv_write(struct file *file, const char __user *buf, size_t count,loff_t *ppos)
{
	printk("first_drv_write\n");
	return 0;
}

int major;
static struct file_operations first_drv_fops = {
	.owner = THIS_MODULE,
	.open = first_drv_open,
//	.read = first_drv_read,
	.write = first_drv_write,
};
static int first_drv_init(void)//内核入口函数 
{

	major = register_chrdev(0,"first_drv",&first_drv_fops);//注册驱动程序 将major 111和first_drv_fops一起挂接到内核数组中，
	firstdrv_class = class_create(THIS_MODULE,"first_drv");//生成设备信息存放到/sys目录为mdev自动创建设备节点 此处创建一个类first_drv 放在sys目录下
	firstdrv_class_dev = class_device_create(firstdrv_class,NULL,MKDEV(major,0),NULL,"xyz");//在first_drv类下创建一个xyz设备
	return 0;
}	//APP中根据111就可以找到该first_drv_fops结构体对应的函数


static void first_drv_exit(void)//出口函数
{
	unregister_chrdev(major,"first_drv");
	class_device_unregister(firstdrv_class_dev);
	class_destroy(firstdrv_class);
}

module_init(first_drv_init);
module_exit(first_drv_exit);
MODULE_LICENSE("GPL");//解决 firstdrv_class firstdrv_class_dev未识别
	
