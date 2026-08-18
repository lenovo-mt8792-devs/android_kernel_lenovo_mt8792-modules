#ifndef __KEYBOARD_CORE__H__
#define __KEYBOARD_CORE__H__

#include <linux/ioctl.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/alarmtimer.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
//#include <mt-plat/aee.h>
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/delay.h>
//#include <mt-plat/mtk_boot_common.h>
#include <linux/hid.h>
#include <linux/hid-debug.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h>

#include <linux/fcntl.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/uio.h>

#include <linux/of_gpio.h>
#include <linux/of_irq.h>
//#include "../../../gpio/gpiolib.h"

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/iio/consumer.h>
#include <linux/workqueue.h>

#include <linux/tn_common.h>
#define WAKEUP_NAME "tn_wakeup"
#define KEYBOARD_NAME "tn_keyboard"
#define TOUCHPAD_NAME "tn_touchpad"
#define KEYBOARD_CORE_NAME "tinno,tn_keyboard"
#define VERDOR_ID   0x17EF
#define PRODUCT_ID  0x6175

#define TOUCH_X_MAX    2944//2998//2944 //1079 //800  //2560
#define TOUCH_Y_MAX    1840//1771//1840 //2407 //1280 //1536
#define TOUCH_FINGER_MAX 4


#define UART_BUFFER_SIZE 256

#define CONFIG_KEYBOARD_DEBUG
#define CONFIG_KEYBOARD_ERR
#define CONFIG_KEYBOARD_INFO
//#define CONFIG_KEYBOARD_KA018
#define CONFIG_KEYBOARD_KA021
//#define CONFIG_KEYBOARD_KA027
//#define CONFIG_ADC_SUPPORT
#define CONFIG_PLUG_SUPPORT

#define CONFIG_POWER_CTRL_SUPPORT

#define CONFIG_BOARD_V4_SUPPORT

#ifdef CONFIG_KEYBOARD_INFO
#define KB_TAG   "TN_KB:"
#define kb_info(fmt, args...)   pr_err(KB_TAG fmt,##args)
#else
#define kb_info(fmt, args...)   do { } while (0)
#endif


#ifdef CONFIG_KEYBOARD_DEBUG
#define KB_TAG   "TN_KB:"
#define kb_debug(fmt, args...)   pr_err(KB_TAG fmt,##args)
#else
#define kb_debug(fmt, args...)   do { } while (0)
#endif

#ifdef CONFIG_KEYBOARD_ERR
#define KB_TAG   "TN_KB:"
#define kb_err(fmt, args...)   pr_err(KB_TAG fmt,##args)
#else
#define kb_err(fmt, args...)   do { } while (0)
#endif


#define KEYBOARD_CONNECT_STATUS         (1<<0)
#define KEYBOARD_POWER_ON_STATUS        (1<<1)
#define KEYBOARD_CAPSLOCK_ON_STATUS     (1<<2)
#define KEYBOARD_MUTEDISABLE_ON_STATUS  (1<<3)
#define KEYBOARD_MICDISABLE_ON_STATUS   (1<<4)
#define KEYBOARD_INPUT_INIT_STATUS      (1<<5)

#ifdef CONFIG_KEYBOARD_KA027
#define KEY_LOCKSCREEN          0x38e
#define KEY_SWITCHLANUAGE       0x390
#define KEY_MICDISABLE          0x391
#define KEY_TOUCHPANELMUTE      0x392
#define KEY_GLOBALSEARCH        0x393
#define KEY_FULLSCREEN          0x394
#define KEY_SPLITSCREEN         0x395
#define KEY_SUPERINTCON         0x397
#define KEY_CUSTOMERAPP1        0x398
#define KEY_CUSTOMERAPP2        0x399
#define KEY_KB_ENABLE           0x28e
#define KEY_KB_DISABLE          0x28f
#endif

#ifdef CONFIG_KEYBOARD_KA021
#define KEY_LOCKSCREEN          0x280
#define KEY_SWITCHLANUAGE       0x282
#define KEY_MICDISABLE          0x283
#define KEY_TOUCHPANELMUTE      0x284
#define KEY_GLOBALSEARCH        0x285
#define KEY_FULLSCREEN          0x286
#define KEY_SPLITSCREEN         0x287
#define KEY_SUPERINTCON         0x289
#define KEY_CUSTOMERAPP1        0x28a
#define KEY_CUSTOMERAPP2        0x28b
#define KEY_KB_ENABLE           0x28e
#define KEY_KB_DISABLE          0x28f
#endif

#ifdef CONFIG_KEYBOARD_KA018
#define KEY_MICDISABLE          634 //0x27a
#define KEY_FULLSCREEN          635 //0x27b
#define KEY_SPLITSCREEN         636 //0x27c
#define KEY_LOCKSCREEN          637 //0x27d
#define KEY_SWITCHLANUAGE       638 //0x27e
#endif


enum{
     KEYBOARD_PLUG_IN_EVENT =  0x01,
     KEYBOARD_PLUG_VCC_EVENT,
     KEYBOARD_PLUG_OUT_EVENT,
     KEYBOARD_HOST_LCD_ON_EVENT,
     KEYBOARD_HOST_LCD_OFF_EVENT,
     
     KEYBOARD_CAPSLOCK_ON_EVENT, 
     KEYBOARD_CAPSLOCK_OFF_EVENT, 
     KEYBOARD_MUTEDISABLE_ON_EVENT,   
     KEYBOARD_MUTEDISABLE_OFF_EVENT,
     
     KEYBOARD_MICDISABLE_ON_EVENT,
     KEYBOARD_MICDISABLE_OFF_EVENT,
     KEYBOARD_HOST_RX_GPIO_EVENT,
     KEYBOARD_HOST_RX_UART_EVENT,

     KEYBOARD_HOST_CHECK_EVENT,
	 KEYBOARD_TEST_EVENT,
     KEYBOARD_WAKEUP_IRQ_EVENT,
     KEYBOARD_PAD_HALL_CLOSE,
};

enum {
	KEYBOARD_UART_OPEN_START_TYPE = 0x01,
	KEYBOARD_UART_OPEN_END_TYPE,	
	KEYBOARD_UART_WRITE_START_TYPE,
	KEYBOARD_UART_WRITE_END_TYPE,
};

enum {
	KEYBOARD_UART_RX_CLEAR_TYPE = 0x00,
	KEYBOARD_UART_RX_SET_TYPE,	
	KEYBOARD_UART_RX_GPIO_TYPE,	
};

enum {
	KEYBOARD_UART_RECV_DEFAULT = 0x00,
	KEYBOARD_UART_RECV_START,	
	KEYBOARD_UART_RECV_END,	
};

enum {
	PAD_HALL_STATU_OPEN = 0x00,
	PAD_HALL_STATU_CLOSE
};

struct touch_event{
    unsigned int x;
    unsigned int y;
    unsigned char id;
    unsigned char area;
    unsigned char is_down;
    unsigned char is_left;
    unsigned char is_right;     
};

typedef int (*tn_keyboard_callback_t)(unsigned char event);






struct tn_keyboard_data{
    struct input_dev *input_tn_keyboard;
    unsigned char old[8];
    unsigned char new[8];

   /* struct input_dev *input_mm_tn_keyboard;*/
    unsigned char mm_old[4];
    unsigned char mm_new[4];
    
    struct input_dev *input_touchpad;     
    unsigned char data[22];   //len(1Byte)+key(1Byte)+fingers(20Byte)
    unsigned char  touch_down;
    unsigned char  touch_temp;
    int pre_fingers; 
    struct touch_event event;

	struct input_dev *input_wakeup; 

    struct notifier_block tn_keyboard_notify;
    unsigned char write_buf[UART_BUFFER_SIZE];
    unsigned char read_buf[UART_BUFFER_SIZE];  
    unsigned char write_check_buf[UART_BUFFER_SIZE];
    unsigned char recv_dma_buf[UART_BUFFER_SIZE];
    int recv_len;
    int recv_status;
    int write_len;
    int read_len;

    struct task_struct *tn_keyboard_task;
    int flag;

    int power_status;
    struct platform_device *plat_dev;
    struct pinctrl *pinctrl;
    struct pinctrl_state *uart_tx_set;
    struct pinctrl_state *uart_tx_clear;
	struct pinctrl_state *uart_rx_set;
    struct pinctrl_state *uart_rx_clear;
	struct pinctrl_state *uart_rx_gpio_pin;
    struct pinctrl_state *uart_wake_gpio_pin;
    struct pinctrl_state *uart_wake_clear;
    int status;
    int vcc_on;

    int power_en_gpio;
    int lcda_en_gpio;
    int plug_gpio;
    int plug_irq;

	int tx_gpio;
    int rx_irq;
    
    int tx_en_gpio;

	int uart_rx_gpio;
    int uart_rx_gpio_irq;
	int uart_rx_mode;
    
    int uart_wake_gpio;
    int uart_wake_gpio_irq;
    int uart_wake_gpio_irq_count;

    int uart_tx_gpio;
    tn_keyboard_callback_t call_back;
    struct mutex mutex;
    unsigned char tn_keyboard_status;
    unsigned char tn_keyboard_events;
    unsigned char new_event;

    struct file *file_client;
	struct uart_8250_port *port;
	struct hrtimer plug_timer;
    struct hrtimer check_timer;
    struct alarm tp_down_loss_check_timer;
    struct alarm pad_hall_delay_close_vcc_timer;
    struct iio_channel *adc_check_ch;
    int read_flag;    
    int tp_down_loss_timer_enable;
    int pad_hall_delay_close_vcc_enable;
};


extern struct tn_keyboard_data *tn_keyboard_client;
extern char TAG[60];

void tn_keyboard_show_buf(void *buf, int count);

int tn_keyboard_input_init(void);
int tn_keyboard_input_report(char *buf);
int tn_keyboard_mm_input_report(char *buf);
int tn_keyboard_input_power_key_report(void);
int tn_keyboard_input_wakeup_init(void);

void tn_keyboard_led_process(int code, int value);
void tn_keyboard_led_report(int key_value);

int touchpad_input_init(void);
int touchpad_input_report(char *buf);
void tn_keyboard_power_enable(int value);
//extern tn_keyboard_lcd_event_call_back_t lcd_event_call_back;
//extern tn_keyboard_uart_call_back_t uart_call_back;


//extern tn_keyboard_uart_dma_call_back_t uart_dma_call_back;
//extern tn_keyboard_uart_dma_write_call_back_t uart_dma_write_call_back;
//extern tn_keyboard_uart_dma_set_call_back_t uart_dma_set_call_back;
extern void tn_keyboard_start_tp_down_loss_check_timer(int enable);
extern int tn_keyboard_analyze(char * buf, int len);
extern int tn_keyboard_enable_uart_tx(int value);
extern int tn_keyboard_analyze_byte(char          ch);
void send_event(int event);

/*extern ssize_t tty_write(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos);*/

//extern ssize_t file_tty_write(struct file *file, struct kiocb *iocb, struct iov_iter *from);	
#endif
