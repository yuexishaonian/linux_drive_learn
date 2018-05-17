#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int fd;
void my_signal_fun(int signum)
{
	unsigned char key_val;
	read(fd,&key_val,1);
	printf("key_val: 0x%x\n",key_val);
}
int main(int argc,char **argv)
{
	unsigned char key_val;
	int ret;
	int oflags;
	signal(SIGIO,my_signal_fun);
		
	fd = open("/dev/buttons",O_RDWR);
	if(fd < 0)
	{
		printf("can't open!\n");	
		return -1;
	}
	
	fcntl(fd,F_SETOWN, getpid());//这一行告诉驱动程序发给谁 获取当前进程pid 告诉内核发给谁 
	oflags = fcntl(fd,F_GETFL); //这一行和下一行 改变fasync标记，最终会调用到驱动的faync > fasync_helper:初始化或释放fasync_struct结构体
	fcntl(fd,F_SETFL,oflags | FASYNC);//改变oflags调用这这个后会调用驱动程序里fifth_drv_fasync从而初始化驱动程序里button_async结构体
	while(1)
	{
		sleep(1000);	
	}
	return 0;
}
