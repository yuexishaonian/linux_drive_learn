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
static struct class_device *firstdrv_class_dev;

volatile unsigned long *gpfcon = NULL;
volatile unsigned long *gpfdat = NULL;

static int first_drv_open(struct inode *inode, struct file *file)
{
	printk("first_drv_open\n");
	/* 配置 GP4 5 6为输出*/
	*gpfcon &= ~((0x03<<(4*2)) | (0x03<<(5*2)) | (0x03<<(6*2)));
	*gpfcon |=  ((0x01<<(4*2)) | (0x01<<(5*2)) | (0x01<<(6*2)));
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
	int val;
	printk("first_drv_write\n");
	copy_from_user(&val,buf,count);//从用户空间向内核空间传数据
	if(val == 1)
	{	
		*gpfdat &= ~(1<<4)|(1<<5)|(1<<6);
	}
	else
	{
		*gpfdat |= (1<<4)|(1<<5)|(1<<6);
	}
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
	gpfcon = (volatile unsigned long *)ioremap(0x56000050,16);
	gpfdat = gpfcon + 1;
	return 0;
}	//APP中根据111就可以找到该first_drv_fops结构体对应的函数


static void first_drv_exit(void)//出口函数
{
	unregister_chrdev(major,"first_drv");
	class_device_unregister(firstdrv_class_dev);
	class_destroy(firstdrv_class);
	iounmap(gpfcon);
}

module_init(first_drv_init);
module_exit(first_drv_exit);
MODULE_AUTHOR("what should I do");
MODULE_VERSION("0.1.0");
MODULE_DESCRIPTION("S3C2410/S3C2440 LED Driver");
MODULE_LICENSE("GPL");//解决 firstdrv_class firstdrv_class_dev未识别
	
