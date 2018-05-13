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

static struct class *seconddrv_class;
static struct class_device *seconddrv_class_dev;

volatile unsigned long *gpfcon = NULL;
volatile unsigned long *gpfdat = NULL;
volatile unsigned long *gpgcon = NULL;
volatile unsigned long *gpgdat = NULL;

static int second_drv_open(struct inode *inode, struct file *file)
{
	/* 配置 GPF 0 2为输入引脚*/
	*gpfcon &= ~((0x03<<(0*2)) | (0x03<<(2*2));
	/* 配置 GPG 3 11为输入引脚*/
	*gpgcon &= ~((0x03<<(3*2)) | (0x03<<(11*2));
	return 0;
}

ssize_t  second_drv_read(struct file *file, char __user *buf, size_t size,loff_t *ppos)
{
	//返回4个引脚的电平
	unsigned char key_vals[4];
	int regval;
	if(size != sizeof(key_vals))
		return -EINVAL;
	//读 GPF0,2
	regval = *gpfdat;
	key_vals[0] = (regval & (1<<0)) ? 1 : 0;
	key_vals[1] = (regval & (1<<2)) ? 1 : 0;
	//读 GPF3,11
	regval = *gpgdat;
	key_vals[2] = (regval & (1<<3)) ? 1 : 0;
	key_vals[3] = (regval & (1<<11)) ? 1 : 0;

	copy_to_user(buf,key_vals,sizeof(key_vals));
	return sizeof(key_vals);
}
/*
static ssize_t  second_drv_write(struct file *file, const char __user *buf, size_t count,loff_t *ppos)
{
	int val;
	printk("second_drv_write\n");
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
*/
int major;
static struct file_operations second_drv_fops = {
	.owner = THIS_MODULE,
	.open = second_drv_open,
	.read = second_drv_read,
//	.write = second_drv_write,
};
static int second_drv_init(void)//内核入口函数 
{

	major = register_chrdev(0,"second_drv",&second_drv_fops);//注册驱动程序 ，
	seconddrv_class = class_create(THIS_MODULE,"second_drv");//生成设备信息存放到/sys目录为mdev自动创建设备节点 此处创建一个类second_drv 放在sys目录下
	seconddrv_class_dev = class_device_create(seconddrv_class,NULL,MKDEV(major,0),NULL,"buttons");//在second_drv类下创建一个/dev/buttons设备
	gpfcon = (volatile unsigned long *)ioremap(0x56000050,16);
	gpfdat = gpfcon + 1;
	gpgcon = (volatile unsigned long *)ioremap(0x56000060,16);
	gpgdat = gpgcon + 1;
	return 0;
}	


static void second_drv_exit(void)//出口函数
{
	unregister_chrdev(major,"second_drv");
	class_device_unregister(seconddrv_class_dev);
	class_destroy(seconddrv_class);
	iounmap(gpfcon);
	iounmap(gpgcon);
}

module_init(second_drv_init);
module_exit(second_drv_exit);
MODULE_AUTHOR("what should I do");
MODULE_VERSION("0.1.0");
MODULE_DESCRIPTION("S3C2410/S3C2440 LED Driver");
MODULE_LICENSE("GPL");//解决 firstdrv_class firstdrv_class_dev未识别
	
