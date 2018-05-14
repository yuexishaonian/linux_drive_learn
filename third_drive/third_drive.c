#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/poll.h>


static struct class *thirddrv_class;
static struct class_device *thirddrv_class_dev;

volatile unsigned long *gpfcon = NULL;
volatile unsigned long *gpfdat = NULL;
volatile unsigned long *gpgcon = NULL;
volatile unsigned long *gpgdat = NULL;

static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

/* 中断事件标志, 中断服务程序将它置1，forth_drv_read将它清0 */
static volatile int ev_press = 0;

struct pin_desc{
	unsigned int pin;
	unsigned int key_val;
};
/*
// *键值: 按下时 , 0x01,0x02,0x03,0x04 
// *键值: 松开时 , 0x81,0x82,0x83,0x84 
*/
static unsigned char key_val;

struct pin_desc pins_desc[4] = {
	{S3C2410_GPF0,0x01},
	{S3C2410_GPF2,0x02},
	{S3C2410_GPG3,0x03},
	{S3C2410_GPG11,0x04},
};

static irqreturn_t buttons_irq(int irq, void *dev_id)
{
	struct pin_desc * pindesc = (struct pin_desc *)dev_id;
	unsigned int pinval;
	pinval = s3c2410_gpio_getpin(pindesc->pin);
	if(pinval)
	{
		//松开
		key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		//按下
		key_val = pindesc->key_val;
	}
	ev_press = 1;                  /* 表示中断发生了 */
    wake_up_interruptible(&button_waitq);   /* 唤醒休眠的进程 */
	
	return IRQ_RETVAL(IRQ_HANDLED);
}
static int third_drv_open(struct inode *inode, struct file *file)
{
	request_irq(IRQ_EINT0,buttons_irq,IRQT_BOTHEDGE,"S2",&pins_desc[0]);	
	request_irq(IRQ_EINT2,buttons_irq,IRQT_BOTHEDGE,"S3",&pins_desc[1]);
	request_irq(IRQ_EINT11,buttons_irq,IRQT_BOTHEDGE,"S4",&pins_desc[2]);
	request_irq(IRQ_EINT19,buttons_irq,IRQT_BOTHEDGE,"S5",&pins_desc[3]);
	return 0;
}
static int third_drv_close(struct inode *inode, struct file *file)
{
	free_irq(IRQ_EINT0,&pins_desc[0]);	
	free_irq(IRQ_EINT2,&pins_desc[1]);	
	free_irq(IRQ_EINT11,&pins_desc[2]);	
	free_irq(IRQ_EINT19,&pins_desc[3]);	
	return 0;
}

ssize_t  third_drv_read(struct file *file, char __user *buf, size_t size,loff_t *ppos)
{
	//返回4个引脚的电平
	if(size != 1)
		return -EINVAL;
	//如果没有按键动作发生，休眠 让出cpu 不返回
	wait_event_interruptible(button_waitq, ev_press);
	
	//有按键发生 返回键值
	copy_to_user(buf,&key_val,1);
	ev_press = 0;
	
	return 1;
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
	.open = third_drv_open,
	.read = third_drv_read,
	.release = third_drv_close,
	
//	.write = second_drv_write,
};
static int third_drv_init(void)//内核入口函数 
{

	major = register_chrdev(0,"third_drv",&second_drv_fops);//注册驱动程序 ，
	thirddrv_class = class_create(THIS_MODULE,"third_drv");//生成设备信息存放到/sys目录为mdev自动创建设备节点 此处创建一个类second_drv 放在sys目录下
	thirddrv_class_dev = class_device_create(thirddrv_class,NULL,MKDEV(major,0),NULL,"buttons");//在second_drv类下创建一个/dev/buttons设备
	gpfcon = (volatile unsigned long *)ioremap(0x56000050,16);
	gpfdat = gpfcon + 1;
	gpgcon = (volatile unsigned long *)ioremap(0x56000060,16);
	gpgdat = gpgcon + 1;
	return 0;
}	


static void third_drv_exit(void)//出口函数
{
	unregister_chrdev(major,"third_drv");
	class_device_unregister(thirddrv_class_dev);
	class_destroy(thirddrv_class);
	iounmap(gpfcon);
	iounmap(gpgcon);
}

module_init(third_drv_init);
module_exit(third_drv_exit);
MODULE_AUTHOR("what should I do");
MODULE_VERSION("0.1.0");
MODULE_DESCRIPTION("S3C2410/S3C2440 LED Driver");
MODULE_LICENSE("GPL");//解决 firstdrv_class firstdrv_class_dev未识别
	
