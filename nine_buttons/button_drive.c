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



static struct class *ninedrv_class;
static struct class_device *ninedrv_class_dev;

volatile unsigned long *gpfcon = NULL;
volatile unsigned long *gpfdat = NULL;
volatile unsigned long *gpgcon = NULL;
volatile unsigned long *gpgdat = NULL;


static struct timer_list button_timer;//定义一个定时器
static DECLARE_WAIT_QUEUE_HEAD(button_waitq);
static DECLARE_MUTEX(button_lock);//定义互斥锁
static struct fasync_struct *button_async;
static struct pin_desc *irq_pd;
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
	mod_timer(&button_timer,jiffies+HZ/100);//修改超时时间 每隔10ms有一个系统时钟中断，在系统时钟中断里jiffies会累加 HZ=100 即jiffies+1 超时时间为10ms
	irq_pd = (struct pin_desc *)dev_id;
	return IRQ_RETVAL(IRQ_HANDLED);
}
static int nine_drv_open(struct inode *inode, struct file *file)
{
#if 0
	if(!atomic_dec_and_test(&can_open))//canopen自减操作后测试是否为0 为0则为true 则继续执行申请中断函数
	{
		atomic_inc(&can_open);//can_open自加1
		return -EBUSY;
	}
#endif
	if(file->f_flags & O_NONBLOCK)
	{
		if(down_trylock(&button_lock))
			return -EBUSY;
	}
	else
	{
		//获取信号量
		down(&button_lock);
	}
	
	request_irq(IRQ_EINT0,buttons_irq,IRQT_BOTHEDGE,"S2",&pins_desc[0]);	
	request_irq(IRQ_EINT2,buttons_irq,IRQT_BOTHEDGE,"S3",&pins_desc[1]);
	request_irq(IRQ_EINT11,buttons_irq,IRQT_BOTHEDGE,"S4",&pins_desc[2]);
	request_irq(IRQ_EINT19,buttons_irq,IRQT_BOTHEDGE,"S5",&pins_desc[3]);
	return 0;
}
static int nine_drv_close(struct inode *inode, struct file *file)
{
	free_irq(IRQ_EINT0,&pins_desc[0]);	
	free_irq(IRQ_EINT2,&pins_desc[1]);	
	free_irq(IRQ_EINT11,&pins_desc[2]);	
	free_irq(IRQ_EINT19,&pins_desc[3]);		
	up(&button_lock);//释放信号量
	return 0;
}

static unsigned nine_drv_poll(struct file *file,poll_table *wait)
{
	unsigned int mask = 0;
	poll_wait(file,&button_waitq,wait);
	if(ev_press)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}
ssize_t  nine_drv_read(struct file *file, char __user *buf, size_t size,loff_t *ppos)
{
	//返回4个引脚的电平
	if(size != 1)
		return -EINVAL;
	if(file->f_flags & O_NONBLOCK)
	{
		if(!ev_press)//如果没有按键动作发生 有按键发生ev_press == 1 不返回
			return -EAGAIN;
	}
	else
	{
		//如果没有按键动作发生，休眠 让出cpu 不返回
		wait_event_interruptible(button_waitq, ev_press);
	}

	//有按键发生 返回键值
	copy_to_user(buf,&key_val,1);
	ev_press = 0;
	
	return 1;
}

int major;
static int nine_drv_fasync (int fd, struct file *filp, int on)
{
	printk("driver: nine_drv_fasync\n");
	return fasync_helper (fd, filp, on, &button_async);
}
static void buttons_timer_function(unsigned long data)
{
	struct pin_desc * pindesc = irq_pd;
	unsigned int pinval;
	if(!pindesc)
		return;
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
	kill_fasync(&button_async,SIGIO,POLL_IN);//给应用程序发送信号 POLL_IN代表有数据等待读取
}
static struct file_operations nine_drv_fops = {
	.owner = THIS_MODULE,
	.open = nine_drv_open,
	.read = nine_drv_read,
	.release = nine_drv_close,
	.poll    =  nine_drv_poll,
	.fasync	 =  nine_drv_fasync,
};
static int nine_drv_init(void)//内核入口函数 
{
	init_timer(&button_timer);
	button_timer.function = buttons_timer_function;
	add_timer(&button_timer);//
	
	major = register_chrdev(0,"nine_drv",&nine_drv_fops);//注册驱动程序 ，
	ninedrv_class = class_create(THIS_MODULE,"nine_drv");//生成设备信息存放到/sys目录为mdev自动创建设备节点 此处创建一个类second_drv 放在sys目录下
	ninedrv_class_dev = class_device_create(ninedrv_class,NULL,MKDEV(major,0),NULL,"buttons");//在second_drv类下创建一个/dev/buttons设备
	gpfcon = (volatile unsigned long *)ioremap(0x56000050,16);
	gpfdat = gpfcon + 1;
	gpgcon = (volatile unsigned long *)ioremap(0x56000060,16);
	gpgdat = gpgcon + 1;
	return 0;
}	


static void nine_drv_exit(void)//出口函数
{
	unregister_chrdev(major,"nine_drv");
	class_device_unregister(ninedrv_class_dev);
	class_destroy(ninedrv_class);
	iounmap(gpfcon);
	iounmap(gpgcon);
}

module_init(nine_drv_init);
module_exit(nine_drv_exit);
MODULE_AUTHOR("what should I do");
MODULE_VERSION("0.1.0");
MODULE_DESCRIPTION("S3C2410/S3C2440 LED Driver");
MODULE_LICENSE("GPL");//解决 firstdrv_class firstdrv_class_dev未识别
	
