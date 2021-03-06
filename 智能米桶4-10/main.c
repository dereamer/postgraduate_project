/*  主函数到时候画一个程序框图，将各个功能模块分别继承在各自的函数中，然后根据模块间的调用关系写出集成好的函数，然后在主函数中调用即可，循环外主要是各个模块的初始化。*/
#include "stdio.h"
#include "stdlib.h"
#include "sys.h"
#include "includes.h"
#include "stm32f10x_exti.h"
//#include "stm32f10x_tim.h"
#include "chumi.h"
#include "weight.h"
#include "lcd.h"
#include "led.h"
#include "temp.h"
#include "usart.h"
#include "picture.h"
//yixia wei tupian xianshi diaoyong 
#include "delay.h"
#include "malloc.h"  
#include "MMC_SD.h" 
#include "ff.h"  
#include "exfuns.h"
#include "fontupd.h"
#include "text.h"	
#include "piclib.h"	
#include "string.h"	
#include "sound.h"
#include "communication.h"

#define WK PAin(0)

//开始任务
#define START_TASK_PRIO		10
//任务堆栈大小	
#define START_STK_SIZE 		256
//任务控制块
OS_TCB StartTaskTCB;
//任务堆栈	
CPU_STK START_TASK_STK[START_STK_SIZE];
//任务函数
void start_task(void *p_arg);



//出米任务
#define CHUMI_TASK_PRIO		4
//任务堆栈大小	
#define CHUMI_STK_SIZE 		256
//任务控制块
OS_TCB CHUMI_TaskTCB;
//任务堆栈	
CPU_STK CHUMI_TASK_STK[CHUMI_STK_SIZE];
void chumi_task(void *p_arg);

//显示图片任务
#define SHOWPIC_TASK_PRIO		5
//任务堆栈大小	
#define SHOWPIC_STK_SIZE 		512
//任务控制块
OS_TCB SHOWPIC_TaskTCB;
//任务堆栈	
CPU_STK SHOWPIC_TASK_STK[SHOWPIC_STK_SIZE];
void showpic_task(void *p_arg);


	
////////////////////////////////////////////////////////

u8 temp;
u8 humi;
int count_time=0;
int wk_flag=1;
int wk_count=0;
int sleep_count=0;
char temp_str[20]="1@temp:";
char humi_str[10]="humi:";
extern s32 Weight_Shiwu;
extern char Uart2_Buf[Buf2_Max];//串口2接收缓�
extern   int chumi_flag;


void WKUP_Init(void)
{	
  GPIO_InitTypeDef  GPIO_InitStructure;  		  
	NVIC_InitTypeDef NVIC_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;


	GPIO_InitStructure.GPIO_Pin =GPIO_Pin_0;	 //PA.0
	GPIO_InitStructure.GPIO_Mode =GPIO_Mode_IPD;//上拉输入
	GPIO_Init(GPIOA, &GPIO_InitStructure);	//初始化IO
    //使用外部中断方式
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);	//中断线0连接GPIOA.0

  EXTI_InitStructure.EXTI_Line = EXTI_Line0;	//设置按键所有的外部线路
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;			//设外外部中断模式:EXTI线路为中断请求
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;  //上升沿触发
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);	// 初始化外部中断

	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn; //使能按键所在的外部中断通道
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; //先占优先级2级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2; //从优先级2级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //使能外部中断通道
	NVIC_Init(&NVIC_InitStructure); //根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器

}


void send_temp_humi()
{
	   char *t;
		 DHT11_Read_Data(&temp,&humi);	
		 t=convert_temp_humi(temp,humi,temp_str,humi_str);
	   sendmes_to_server(t);
	   myfree(t);
	   
}
//主函数
int main(void)
{
	
	OS_ERR err;
	CPU_SR_ALLOC();
	
	  delay_init();
	  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    uart_init(115200); 
	  uart2_init(115200);
	  check_server("1:");
		show_initial();
	  	   
 
	
		mem_init();			//初始化内部内存池	
 	  exfuns_init();		//为fatfs相关变量申请内存  
    f_mount(fs[0],"0:",1);	//挂载SD卡
	  VS_Init();
	  
    Init_HX711pin();
    Get_Maopi();	
    delay_ms(1000);
  	delay_ms(1000);	 //称毛皮重量
	  Get_Maopi();

     motor_pin_init(); 		 
		 DHT11_Init();
		 
    while(temp==0|humi==0)  //模块刚刚初始化读数为0
		 {
		 DHT11_Read_Data(&temp,&humi);
		 }	
		 LCD_Clear(WHITE);
		 mp3_play_begin();
		 WKUP_Init();
	OSInit(&err);		    	//初始化UCOSIII
	OS_CRITICAL_ENTER();	//进入临界区			 
	//创建开始任务
	OSTaskCreate((OS_TCB 	* )&StartTaskTCB,		//任务控制块
				 (CPU_CHAR	* )"start task", 		//任务名字
                 (OS_TASK_PTR )start_task, 			//任务函数
                 (void		* )0,					//传递给任务函数的参数
                 (OS_PRIO	  )START_TASK_PRIO,     //任务优先级
                 (CPU_STK   * )&START_TASK_STK[0],	//任务堆栈基地址
                 (CPU_STK_SIZE)START_STK_SIZE/10,	//任务堆栈深度限位
                 (CPU_STK_SIZE)START_STK_SIZE,		//任务堆栈大小
                 (OS_MSG_QTY  )0,					//任务内部消息队列能够接收的最大消息数目,为0时禁止接收消息
                 (OS_TICK	  )0,					//当使能时间片轮转时的时间片长度，为0时为默认长度，
                 (void   	* )0,					//用户补充的存储区
                 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR, //任务选项
                 (OS_ERR 	* )&err);				//存放该函数错误时的返回值
	OS_CRITICAL_EXIT();	//退出临界区	 
	OSStart(&err);      //开启UCOSIII
}


//开始任务函数
void start_task(void *p_arg)
{
	OS_ERR err;
	CPU_SR_ALLOC();
	p_arg = p_arg;
	
	CPU_Init();
#if OS_CFG_STAT_TASK_EN > 0u
   OSStatTaskCPUUsageInit(&err);  	//统计任务                
#endif
	
#ifdef CPU_CFG_INT_DIS_MEAS_EN		//如果使能了测量中断关闭时间
    CPU_IntDisMeasMaxCurReset();	
#endif
	
#if	OS_CFG_SCHED_ROUND_ROBIN_EN  //当使用时间片轮转的时候
	 //使能时间片轮转调度功能,时间片长度为1个系统时钟节拍，既1*5=5ms
	OSSchedRoundRobinCfg(DEF_ENABLED,1,&err);  
#endif	
	
	
	OS_CRITICAL_ENTER();	//进入临界区
								
	//创建出米任务
	OSTaskCreate((OS_TCB 	* )&CHUMI_TaskTCB,		
				 (CPU_CHAR	* )"chumi_task", 		
                 (OS_TASK_PTR )chumi_task, 			
                 (void		* )0,					
                 (OS_PRIO	  )CHUMI_TASK_PRIO,     
                 (CPU_STK   * )&CHUMI_TASK_STK[0],	
                 (CPU_STK_SIZE)CHUMI_STK_SIZE/10,	
                 (CPU_STK_SIZE)CHUMI_STK_SIZE,		
                 (OS_MSG_QTY  )0,					
                 (OS_TICK	  )0,  					
                 (void   	* )0,					
                 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR,
                 (OS_ERR 	* )&err);	

		
  //创建显示图片任务
		OSTaskCreate((OS_TCB 	* )&SHOWPIC_TaskTCB,		
				 (CPU_CHAR	* )"showpic_task", 		
                 (OS_TASK_PTR )showpic_task, 			
                 (void		* )0,					
                 (OS_PRIO	  )SHOWPIC_TASK_PRIO,     
                 (CPU_STK   * )&SHOWPIC_TASK_STK[0],	
                 (CPU_STK_SIZE)SHOWPIC_STK_SIZE/10,	
                 (CPU_STK_SIZE)SHOWPIC_STK_SIZE,		
                 (OS_MSG_QTY  )0,					
                 (OS_TICK	  )0,  					
                 (void   	* )0,					
                 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR,
                 (OS_ERR 	* )&err);		
								 
	OS_CRITICAL_EXIT();	//退出临界区
	OSTaskDel((OS_TCB*)0,&err);	//删除start_task任务自身
}


//出米任务函数
void chumi_task(void *p_arg)
{
    OS_ERR err;
	   while(1)
	 {
		if(chumi_flag==1)
		{
		 LCD_LED=1;
		 OSTaskSuspend((OS_TCB*)&SHOWPIC_TaskTCB,&err);
   	 judge_ser_data();
		 chumi_flag=0;
		 respone_server("1:ok");
		 OSTaskResume((OS_TCB*)&SHOWPIC_TaskTCB,&err);
		 OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_HMSM_STRICT,&err);
		}
   else
    {
			if((!WK)&&wk_flag==1)  sleep_count++;
			if(sleep_count==100) 
			{
				LCD_LED=0;
				OSTaskSuspend((OS_TCB*)&SHOWPIC_TaskTCB,&err);
				wk_flag=0;
				sleep_count=0;
			}
			if(wk_flag==1)
			{
			   OSTaskResume((OS_TCB*)&SHOWPIC_TaskTCB,&err);
			}
		  OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_HMSM_STRICT,&err);
		}	
    count_time++;
    if(count_time==100) 
   { 
	  send_temp_humi();
	  count_time=0;
	 }		
	 }		
}

void showpic_task(void *p_arg)
{
	  OS_ERR err;
    CPU_SR_ALLOC();	
	  while(1)
		{
		if(wk_flag==1)
		{
		OS_CRITICAL_ENTER();
    LCD_Clear(WHITE);
    show_picture("0:/PICTURE1/xiaohui.jpg",0,0,lcddev.width,lcddev.height);
		OS_CRITICAL_EXIT();
	  OSTimeDlyHMSM(0,0,5,0,OS_OPT_TIME_HMSM_STRICT,&err);
		OS_CRITICAL_ENTER();
	  LCD_Clear(WHITE);
	  show_picture("0:/PICTURE2/xiaohui.jpg",0,0,lcddev.width,lcddev.height);
		OS_CRITICAL_EXIT();
	  OSTimeDlyHMSM(0,0,5,0,OS_OPT_TIME_HMSM_STRICT,&err);
		OS_CRITICAL_ENTER();
		LCD_Clear(WHITE);
    LCD_ShowString(30,40,200,24,24,"WELCOME TO SWJTU...");
	  LCD_ShowString(50,100,150,16,16,"temp:  C,humi:  %");
	  LCD_ShowxNum(90,100,temp,2,16,0); 
	  LCD_ShowxNum(162,100,humi,2,16,0); 	
		OS_CRITICAL_EXIT();	
		OSTimeDlyHMSM(0,0,5,0,OS_OPT_TIME_HMSM_STRICT,&err);	
    }			
	}

}


void EXTI0_IRQHandler(void)
{
	   if(EXTI_GetITStatus(EXTI_Line0)!=RESET)
		 {
			  wk_count++;
     }
		 if(wk_count==3)
		 {
		    LCD_LED=1;
			  wk_flag=1;
			  wk_count=0;
		 }
		 EXTI_ClearITPendingBit(EXTI_Line0); 
}





