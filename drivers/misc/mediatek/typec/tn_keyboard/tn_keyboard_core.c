#include "tn_keyboard.h"
//#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/fb.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <uapi/linux/sched/types.h> 
#include <linux/pm.h>
#include <linux/pm_wakeirq.h>
#include <linux/serial_8250.h>

#include "one_wire_bus_phy.h"

#include "mtk_disp_notify.h"

struct tn_keyboard_data *tn_keyboard_client = NULL;
char TAG[60] = {0};
static int test_type = 0;
static int wake_irq_switch = 1;
static struct class *sec_class;
static struct device *sec_device;

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DECLARE_WAIT_QUEUE_HEAD(read_waiter);


//static BLOCKING_NOTIFIER_HEAD(tn_keyboard_notifier_list);

static int tn_keyboard_event_process(unsigned char tn_keyboard_event);
#ifdef CONFIG_PLUG_SUPPORT
static irqreturn_t keybaord_core_plug_irq_handler(int irq, void *data);
#endif
struct file* tn_keyboard_port_to_file(struct uart_8250_port *uart_8250_port);
#ifdef CONFIG_BOARD_V4_SUPPORT
static irqreturn_t tn_keyboard_wake_gpio_irq_handler(int irq, void *data);
#else
static irqreturn_t tn_keyboard_rx_gpio_irq_handler(int irq, void *data);
#endif

void tn_keyboard_show_buf(void *buf, int count) {
    int i = 0;
    char temp[UART_BUFFER_SIZE*2+2] = {0};
    int len = 0;
    char *pbuf = (char *) buf;
	if (count > UART_BUFFER_SIZE)
		count = UART_BUFFER_SIZE;
    for (i = 0; i < count; i++) {
        len += sprintf(temp + len, "%02x", pbuf[i]);
    }
    kb_debug("%s:show_buf  len:[%d]  %s\n", TAG, count, temp);
}

EXPORT_SYMBOL(tn_keyboard_show_buf);


void tn_keyboard_info_buf(void *buf, int count) {
    int i = 0;
    char temp[UART_BUFFER_SIZE*2+2] = {0};
    int len = 0;
    char *pbuf = (char *) buf;
	if (count > UART_BUFFER_SIZE)
		count = UART_BUFFER_SIZE;
    for (i = 0; i < count; i++) {
        len += sprintf(temp + len, "%02x", pbuf[i]);
    }
    kb_info("%s:info_buf  len:[%d]  %s\n", TAG, count, temp);
}

void set_wakeup_irq_switch(int enable){
    kb_debug("set_wakeup_irq_switch wake_irq_switch:%d  enable:%d\n", wake_irq_switch, enable);
    if(wake_irq_switch == enable){
        kb_debug("set_wakeup_irq_switch status same!cancel!\n");
        return;
    }
    if(!tn_keyboard_client->uart_wake_gpio_irq){
		kb_debug("set_wakeup_irq_switch uart_wake_gpio_irq == NULL!cancel!\n");
        return;
	}
    wake_irq_switch = enable;
    if(enable == 1){
        tn_keyboard_client->uart_wake_gpio_irq_count = 0;
        enable_irq(tn_keyboard_client->uart_wake_gpio_irq);
        kb_debug("set_wakeup_irq_switch enable_irq uart_wake_gpio_irq!\n");
    }else if(enable == 0){
        tn_keyboard_client->uart_wake_gpio_irq_count = 3;
        disable_irq(tn_keyboard_client->uart_wake_gpio_irq);
        kb_debug("set_wakeup_irq_switch disable_irq uart_wake_gpio_irq!\n");
    }
}

void send_event(int event){
	tn_keyboard_client->new_event = event;
    tn_keyboard_client->call_back = tn_keyboard_event_process;
    tn_keyboard_client->flag = 1;
    wake_up_interruptible(&waiter);
}

#ifdef CONFIG_ADC_SUPPORT
static int tn_keyboard_adc_init(void){
    int ret = 0;
    int val = 0;
    kb_debug("%s %d start\n",__func__,__LINE__);
    tn_keyboard_client->adc_check_ch = devm_kzalloc(&tn_keyboard_client->plat_dev->dev,sizeof(struct iio_channel),GFP_KERNEL);
    if(!tn_keyboard_client->adc_check_ch)
        return -ENOMEM;
    tn_keyboard_client->adc_check_ch = iio_channel_get(&tn_keyboard_client->plat_dev->dev, "tn_keyboard-ch3");
    ret = IS_ERR(tn_keyboard_client->adc_check_ch);
    if(ret){
        kb_err("%s %d fail to get auxadc iio adc:%d\n", __func__,__LINE__,ret);
        return ret;
    }
    ret = iio_read_channel_processed(tn_keyboard_client->adc_check_ch, &val);
    kb_debug("%s %d ok val=%d\n", __func__,__LINE__,val);
    printk("%s %d ok val=%d\n", __func__,__LINE__,val);
    return 0;
}
static int tn_keyboard_adc_get(void){
    int ret = 0;
    int val = 0;
    if(!tn_keyboard_client->adc_check_ch)
        return -1;
    ret = iio_read_channel_processed(tn_keyboard_client->adc_check_ch, &val);
    kb_debug("%s %d ok val=%d\n", __func__,__LINE__,val);
    printk("%s %d ok val=%d\n", __func__,__LINE__,val);
    return val;
}
#endif
/*
static void tn_keyboard_start_check_timer(void){   
    ktime_t ktime = ktime_set(0, 1000*1000*1200); //1200ms   
    hrtimer_start(&tn_keyboard_client->check_timer, ktime, HRTIMER_MODE_REL);      
}
*/

void tn_keyboard_start_tp_down_loss_check_timer(int enable){
	if(tn_keyboard_client->tp_down_loss_timer_enable == 0 && enable){
		//ktime_t ktime = ktime_set(0, 1000*1000*850); //850ms
		alarm_start_relative(&tn_keyboard_client->tp_down_loss_check_timer, ms_to_ktime(850));
		tn_keyboard_client->tp_down_loss_timer_enable = enable;
		kb_err("%s %d start check!enable:%d\n",__func__,__LINE__, enable);
	}else if(enable == 0){
		alarm_cancel(&tn_keyboard_client->tp_down_loss_check_timer);
		tn_keyboard_client->tp_down_loss_timer_enable = enable;
		kb_err("%s %d cancel check!enable:%d\n",__func__,__LINE__, enable);
	}
}

void enable_pad_hall_delay_close_vcc_timer(int enable){
	if(tn_keyboard_client->pad_hall_delay_close_vcc_enable == 0 && enable){
		alarm_start_relative(&tn_keyboard_client->pad_hall_delay_close_vcc_timer, ms_to_ktime(60*1000));
		tn_keyboard_client->pad_hall_delay_close_vcc_enable = enable;
		kb_err("%s %d start check!enable:%d\n",__func__,__LINE__, enable);
	}else if(enable == 0){
		alarm_cancel(&tn_keyboard_client->pad_hall_delay_close_vcc_timer);
		tn_keyboard_client->pad_hall_delay_close_vcc_enable = enable;
		kb_err("%s %d cancel check!enable:%d\n",__func__,__LINE__, enable);
	}
}

static int tn_keyboard_input_connect(void){
    int ret = 0;
    if(!tn_keyboard_client->input_touchpad){
        ret = touchpad_input_init();
        if(ret){
            kb_err("%s %d touchpad_input_init err:%d\n",__func__,__LINE__, ret);
            return ret;
        }
    }
    if(!tn_keyboard_client->input_tn_keyboard){
        ret = tn_keyboard_input_init();
        if(ret){
            kb_err("%s %d tn_keyboard_input_init err:%d\n",__func__,__LINE__, ret);
            return ret;
        }
    }
    kb_err("%s %d input is connect!\n",__func__,__LINE__);
    return ret;
}

static int tn_keyboard_mod_data_process(char *buf, int len){
    int   value = buf[0]; 
    int ret = 0; 
    struct input_dev  *input_dev = NULL;
    if(tn_keyboard_client->vcc_on == 1 && value != 0x7b && (tn_keyboard_client->tn_keyboard_status & KEYBOARD_CONNECT_STATUS) == 0){
        send_event(KEYBOARD_PLUG_IN_EVENT);
        kb_debug("%s %d KEYBOARD_PLUG_IN_EVENT \n",__func__,__LINE__);
    }
	switch(value){
		case 0x61:
			tn_keyboard_input_report(&buf[1]);
        	kb_debug("%s %d  \n",__func__,__LINE__);
			break;
		case 0x62:
	        tn_keyboard_mm_input_report(&buf[1]);
	        kb_debug("%s %d  \n",__func__,__LINE__);
			break;
		case 0x63:
			if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_INPUT_INIT_STATUS) != 0){
				kb_debug("%s %d	touchpad_input_report begin!\n",__func__,__LINE__);
				touchpad_input_report(&buf[1]);
			}else{
				kb_debug("%s %d	cancel touchpad_input_report!keyboard no INPUT INIT!status: %d vcc_on:%d!\n",__func__,__LINE__,tn_keyboard_client->tn_keyboard_status,tn_keyboard_client->vcc_on);
			}
			break;	
        case 0x7b:
            kb_debug("%s %d status:%2x\n",__func__,__LINE__,tn_keyboard_client->tn_keyboard_status);
            if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_CONNECT_STATUS) == 0){
                send_event(KEYBOARD_PLUG_IN_EVENT);
                kb_debug("%s %d plug in\n",__func__,__LINE__);
                pm_relax(&tn_keyboard_client->plat_dev->dev);
            }
            if(buf[2] == 0x02 && len > 2){
				kb_debug("%s %d start tn_keyboard_start_tp_down_loss_check_timer\n",__func__,__LINE__);
				tn_keyboard_start_tp_down_loss_check_timer(1);
			}
            break;
		case 0x6B:
		case 0x71:            
		case 0x83:	
		case 0x93:                
        case 0x70:
        case 0x6A:    
        case 0x92: 
           if(tn_keyboard_client->read_flag == 1){
                kb_debug("%s %d	value:0x%2x cmd:0x%2x \n",__func__,__LINE__,value,tn_keyboard_client->write_buf[1]);
                if(value == (int)tn_keyboard_client->write_buf[1] || value == (int)tn_keyboard_client->write_buf[1]+1){                    
                    tn_keyboard_client->read_flag = 0;
                    wake_up_interruptible(&read_waiter);
                }
			}	
            if(value == 0x93 && buf[2] == 0x03){
                input_dev = tn_keyboard_client->input_wakeup;
                if(buf[4] == 0x01){
                    input_report_key(input_dev,KEY_KB_ENABLE, 1);
                    input_sync(input_dev);               
                    input_report_key(input_dev,KEY_KB_ENABLE, 0);
                    input_sync(input_dev);
                    kb_debug("%s %d	report KEY_KB_ENABLE \n",__func__,__LINE__);                    
                }else if(buf[4] == 0x00){
                    input_report_key(input_dev,KEY_KB_DISABLE, 1);
                    input_sync(input_dev);               
                    input_report_key(input_dev,KEY_KB_DISABLE, 0);
                    input_sync(input_dev);
                    kb_debug("%s %d	report KEY_KB_DISABLE \n",__func__,__LINE__);
                }                
            }
			break;  
		default:
			return -EINVAL;   
	}
    return 0;
}

int uart_package_date(char *buf, int len, unsigned char *p_out, int *p_len) {
    unsigned char *p_buf, sun, serial;
    unsigned short p_length = 0, i = 0;
    //unsigned char send_buf[128] = {0};
    int ret = 0;
   // p_buf = send_buf;
   
    //sprintf(TAG,"%s text %d",__func__,__LINE__);
    //tn_keyboard_show_buf(buf, len);
    p_buf = p_out;
    p_buf[i++] = 0x55;
    p_buf[i++] = 0x55;
    p_buf[i++] = 0x55;
    p_buf[i++] = 0x55;
    p_buf[i++] = 0x55;
    p_buf[i++] = 0x55;
    p_buf[i++] = 0x55;
    p_buf[i++] = 0x55;
    p_buf[i++] = ONE_WIRE_BUS_PACKET_START_FLAG;
    p_buf[i++] = buf[0];
    serial = one_wire_bus_gen_random_serial();  
    ret = one_wire_bus_data_transcode(serial, &buf[1], &p_buf[i], len - 1);
    if(ret == false){
        kb_err("one_wire_bus_data_transcode1 err%d ret:%d\r\n", __LINE__,ret);
        return -EINVAL;
    }
    i = i + (len - 1) * 2;
    sun = one_wire_bus_get_buf_sum(&p_buf[8], i - 8);
    //rtt_color(RTT_H_BLUE,"serial=%d,sun=%x",serial,sun);
    ret = one_wire_bus_data_transcode(serial, &sun, &p_buf[i], 1);
    if(ret == false){
        kb_err("one_wire_bus_data_transcode2 err%d ret:%d\r\n", __LINE__,ret);
        return -EINVAL;
    }
    i += 2;
    p_buf[i++] = ONE_WIRE_BUS_PACKET_END_FLAG;
    p_buf[i++] = 0x55;
    p_buf[i++] = 0x55;
    p_buf[i++] = 0x55;
    p_buf[i++] = 0x55;
    p_length = i;
    *p_len = p_length;
    p_out = p_buf;
    //sprintf(TAG,"%s ciphertext %d",__func__,__LINE__);
    //tn_keyboard_show_buf(p_buf, p_length);
    return 0;
}



int uart_unpack_date(char *buf, int len, unsigned char *out_buf, int *out_len) {
    int recv_count = len;
    char *temp = buf;
    char recv_buf[128] = {0};
    bool f_dec;
    unsigned char temp_serial, check_sum, new_length;
    //sprintf(TAG,"%s ciphertext %d",__func__,__LINE__);
    //tn_keyboard_show_buf(temp, recv_count);
    temp_serial = one_wire_bus_search_serial_num(temp[2]);
    if (temp_serial == 0xff) {
        kb_err("not found serial%x\r\n", buf[2]);
        return -EINVAL;
    }
    recv_buf[0] = temp[1];
    //alogi("%s %d temp_serial:%d recv_count:%d\r\n", __func__, __LINE__, temp_serial, recv_count);
    f_dec = one_wire_bus_decode_frame(&temp[2], (unsigned char *) &recv_buf[1],
                                      (recv_count - 3) / 2, temp_serial);
    if (f_dec == false) {
        kb_err("decode error\r\n");
        return -EINVAL;
    }
    check_sum = one_wire_bus_get_buf_sum(temp, recv_count - 3);
    new_length = (recv_count - 3) / 2 + 1;
    //kb_debug("temp_serial=%d,f_dec=%d,%x,%x",temp_serial,f_dec,check_sun,temp_buf[length-1]);
    if (recv_buf[1] != (new_length - 3)) {
        kb_err("length error\r\n");
        return -EINVAL;
    }
    if (check_sum != recv_buf[new_length - 1]) {
        kb_err("check_sun error\r\n");
        return -EINVAL;
    }
   // sprintf(TAG,"%s text %d",__func__,__LINE__);
    //sprintf(TAG,"%s text %d",__func__,__LINE__);
    //tn_keyboard_show_buf(recv_buf, new_length);

    *out_len = new_length;
    //alogi("new_length:%d out_len:%d\r\n",new_length,*out_len);
    memcpy(out_buf, recv_buf, new_length);
    return 0;

}


/*int tn_keyboard_analyze_byte(char         ch){      
    char recv_buf[128] = {0};
    int out_len = 0;
    int ret = 0; 
    static unsigned char start = 1;
    static char uart_rxbuf[256] = {0};
    static int  recv_count = 0;
	if(tn_keyboard_client->file_client == NULL)
		return -1;
	
    //printk("K:%02x\n",ch);
    if(ch == 0x55)
        return -1;
    if(ch == ONE_WIRE_BUS_PACKET_START_FLAG){
        recv_count = 0;
        start = 1;
    }
    if(ch == ONE_WIRE_BUS_PACKET_REPEAT_FLAG)
        recv_count = 0;
    if(recv_count < UART_BUFFER_SIZE)
        uart_rxbuf[recv_count++] = ch;        
    if(ch == ONE_WIRE_BUS_PACKET_END_FLAG){  
         if(start == 0){
             recv_count = 0;
             return -1;
         }
        ret = uart_unpack_date(uart_rxbuf, recv_count, recv_buf, &out_len);
        if(ret){
            kb_err("%s %d uart_unpack_date fail:\r\n",__func__,__LINE__,ret);
            start = 0;
            recv_count = 0;
            return ret;
        }    
        ret = tn_keyboard_mod_data_process(recv_buf, out_len);
        if(ret){
            start = 0;                
            recv_count = 0;
            kb_err("%s %d tn_keyboard_mod_data_process fail:\r\n",__func__,__LINE__,ret);
            return ret;
        } 
        memset(uart_rxbuf, 0, 256);
        recv_count = 0;
        start = 0;
        return 0;
    }
    return -1;    
}*/

int tn_keyboard_analyze(char * buf, int len){ 
    char *pbuf = buf;
    int i = 0;
    int recv_count = 0;
    
    char *temp = tn_keyboard_client->recv_dma_buf;
     
    char recv_buf[128] = {0};
    int out_len = 0;
    int ret = 0;

	if(tn_keyboard_client->file_client == NULL ||  len <= 0){
        kb_err("%s %d revc platform len:%d <=0 or platfrom not open serial,not handl data!\r\n",__func__,__LINE__,len);
		return -1;
    }
    
    for(i = 0; i < len; i++){
        if(pbuf[i] == 0x55)
            continue;
        if(pbuf[i] == ONE_WIRE_BUS_PACKET_START_FLAG){
            tn_keyboard_client->recv_len = 0;
            if(tn_keyboard_client->recv_status == KEYBOARD_UART_RECV_DEFAULT)
                tn_keyboard_client->recv_status = KEYBOARD_UART_RECV_START;
        }
        if(pbuf[i] == ONE_WIRE_BUS_PACKET_REPEAT_FLAG){
            tn_keyboard_client->recv_len = 0;
            if(tn_keyboard_client->recv_status == KEYBOARD_UART_RECV_DEFAULT)
                tn_keyboard_client->recv_status = KEYBOARD_UART_RECV_START;
        }    
        if(tn_keyboard_client->recv_len < UART_BUFFER_SIZE && tn_keyboard_client->recv_status == KEYBOARD_UART_RECV_START)
            temp[tn_keyboard_client->recv_len++] = pbuf[i];
        if(pbuf[i] == ONE_WIRE_BUS_PACKET_END_FLAG){
            if(tn_keyboard_client->recv_status == KEYBOARD_UART_RECV_START)
                tn_keyboard_client->recv_status = KEYBOARD_UART_RECV_END;
            break;
        }
    }
    if(tn_keyboard_client->recv_status != KEYBOARD_UART_RECV_END){        
        if(tn_keyboard_client->recv_status == KEYBOARD_UART_RECV_START)
            kb_info("%s %d revc data is not KEYBOARD_UART_RECV_END!recv data: ",__func__,__LINE__);
        else{
            kb_err("%s %d recv exception data fail!len:%d recv_status:%d recv data: ",__func__,__LINE__, len, tn_keyboard_client->recv_status);
        }
        tn_keyboard_info_buf(buf, len);
        tn_keyboard_client->recv_status = KEYBOARD_UART_RECV_DEFAULT;
        return -1;
    }
    tn_keyboard_client->recv_status = KEYBOARD_UART_RECV_DEFAULT;
    recv_count = tn_keyboard_client->recv_len;
    ret = uart_unpack_date(temp, recv_count, recv_buf, &out_len);
    if(ret){
        sprintf(TAG,"%s  %d uart_unpack_date fail!recv_count:%d ",__func__,__LINE__,recv_count);
        tn_keyboard_info_buf(temp, recv_count);
        tn_keyboard_client->recv_status = KEYBOARD_UART_RECV_DEFAULT;
        return ret;
    }
    if(temp[1] == tn_keyboard_client->write_buf[1]){
        memcpy(tn_keyboard_client->write_check_buf, temp, recv_count);
        tn_keyboard_client->write_len = recv_count;
    }else if(temp[1] == (int)tn_keyboard_client->write_buf[1]+1){
        memcpy(tn_keyboard_client->read_buf, temp, recv_count);
        tn_keyboard_client->read_len = recv_count;
    }
    sprintf(TAG,"%s  %d recv data:",__func__,__LINE__);
    tn_keyboard_info_buf(recv_buf, out_len);
    ret = tn_keyboard_mod_data_process(recv_buf, out_len);
    if(ret){
        kb_err("%s %d tn_keyboard_mod_data_process fail:\r\n",__func__,__LINE__,ret);       
    }
    return ret;
   
}



ssize_t tty_write_ex(const char  *buf,    size_t count){
    ssize_t ret = 0;
    struct file *filp;
    
    if(IS_ERR_OR_NULL(tn_keyboard_client->port)){
        kb_err("%s %d err: port is null!\n",__func__, __LINE__);
        return -1;
    }
    if(IS_ERR_OR_NULL(tn_keyboard_client->file_client)){
        kb_err("%s %d err: file_client is null!\n",__func__, __LINE__);
        return -1;
    }
    filp = tn_keyboard_port_to_file(tn_keyboard_client->port);
    if(IS_ERR_OR_NULL(filp)){
        kb_err("%s %d err:tn_keyboard_port_to_file filp is null!\n",__func__, __LINE__);
        return -1;
    }
    if(filp != tn_keyboard_client->file_client){
        kb_err("%s %d replace file_client %p to %p!\n",__func__, __LINE__,tn_keyboard_client->file_client,filp);
        tn_keyboard_client->file_client = filp;
    }
    if(IS_ERR_OR_NULL(tn_keyboard_client->file_client->private_data)){
        kb_err("%s %d err: private_data is null!\n",__func__, __LINE__);
        return -1;
    }
    ret = tn_tty_write(tn_keyboard_client->file_client, buf, count, NULL);
    if(ret < 0){
        kb_err("TN %s %d err:%d!\n",__func__, __LINE__,ret);
        return -1;
    }
    return ret;        
}


int tn_keyboard_get_valid_data(char *buf, int len, unsigned char *out_buf, int *out_len){
    char *pbuf = buf;
    int i = 0;
    int count = 0;
    if(tn_keyboard_client->file_client == NULL){
            //kb_err("TN %s %d err: file_client is null!\n",__func__, __LINE__);
            return -1;
    }
    for(i = 0; i < len; i++){
        if(pbuf[i] == 0x55)
            continue;
        if(pbuf[i] == ONE_WIRE_BUS_PACKET_START_FLAG)
            count = 0;
        if(pbuf[i] == ONE_WIRE_BUS_PACKET_REPEAT_FLAG)
            count = 0;
        if(count < UART_BUFFER_SIZE)
            out_buf[count++] = pbuf[i];
        if(pbuf[i] == ONE_WIRE_BUS_PACKET_END_FLAG)
            break;
    }
    *out_len = count;
    return count;
}



static int tn_keyboard_write(void *buf, int len){
    int ret = 0;
    int out_len = 0;
    int count = 3;
    void *pbuf = buf;
    char write_buf[255] = {0};
    char read_buf[255] = {0};
    int write_len = 0;
    int i = 0;
    int j = 0;
    sprintf(TAG,"%s text %d",__func__,__LINE__);    
    tn_keyboard_info_buf(pbuf, len);
    memset(tn_keyboard_client->write_buf, 0, sizeof(tn_keyboard_client->write_buf));
    //memcpy(tn_keyboard_client->write_buf, buf, len);
    ret = uart_package_date(pbuf, len, write_buf, &write_len);  
    if(ret){
        kb_err("%s %d err\r\n",__func__,__LINE__);
        return ret;
    }
    tn_keyboard_get_valid_data(write_buf, write_len, tn_keyboard_client->write_buf, &out_len);
    for(j = 0;j <= count;j++){
        for(i = 0; i < count; i++){
            tn_keyboard_client->read_flag = 1;       
            ret  = tty_write_ex(write_buf, write_len);
            if(ret > 0){
               break;
            }
            mdelay(30);
        }
        if(i >= count){
            kb_err("%s %d ret:%d i:%d tty_write_ex err!\r\n",__func__,__LINE__,ret,i);
            return ret;
        }

        wait_event_interruptible_timeout(read_waiter, tn_keyboard_client->read_flag != 1, HZ/20);//timeout 50ms
        
        if(tn_keyboard_client->read_flag == 1){		
            kb_debug("%s %d read_flag:%d  \r\n",__func__,__LINE__,tn_keyboard_client->read_flag);  
        }
        if(tn_keyboard_client->write_len <= 0){
            kb_err("%s %d read_flag:%d  write_len:%d\r\n",__func__,__LINE__,tn_keyboard_client->read_flag, tn_keyboard_client->write_len);  
            tn_keyboard_client->read_flag = 0;        
            return -1;
        }
        if(tn_keyboard_client->write_buf[1] == tn_keyboard_client->write_check_buf[1] && 
            tn_keyboard_client->write_buf[out_len-2] == tn_keyboard_client->write_check_buf[out_len-2]){ //compare cmd and crc                
            ret = 0;
            kb_debug("%s %d is same,write %d data ok ret:%d i:%d!\n", __func__, __LINE__,out_len, ret, i);         
            break;
        }else{
            ret = -1;
            kb_err("%s %d is not same %d data err!\n", __func__, __LINE__,out_len);
            continue;
        }
    }
    tn_keyboard_client->write_len = 0;
    memset(tn_keyboard_client->write_check_buf, 0, UART_BUFFER_SIZE);    
    return ret;
}

static int tn_keyboard_read(void *buf, int *r_len){	
    int ret = 0;
	kb_debug("%s %d start\r\n",__func__,__LINE__);
    tn_keyboard_client->read_flag = 1; 
	wait_event_interruptible_timeout(read_waiter, tn_keyboard_client->read_flag != 1, HZ/10);//timeout 100ms
	
	if(tn_keyboard_client->read_flag == 1){		
		kb_debug("%s %d read_flag:%d  \r\n",__func__,__LINE__,tn_keyboard_client->read_flag);  		
	}
    if(tn_keyboard_client->read_len <= 0){
        kb_err("%s %d read_len:%d  err\r\n",__func__,__LINE__,tn_keyboard_client->read_len);
        tn_keyboard_client->read_flag = 0;
        return -1;
    }
	kb_debug("%s %d read_flag:%d len:%d ok\r\n",__func__,__LINE__,tn_keyboard_client->read_flag,tn_keyboard_client->read_len);
    ret = uart_unpack_date(tn_keyboard_client->read_buf,tn_keyboard_client->read_len, buf, r_len);	
    memset(tn_keyboard_client->read_buf, 0, sizeof(tn_keyboard_client->read_buf));  
    sprintf(TAG,"%s text %d",__func__,__LINE__);    
    tn_keyboard_show_buf(buf, *r_len);	
    tn_keyboard_client->read_len = 0;
    memset(tn_keyboard_client->read_buf, 0, UART_BUFFER_SIZE);
	return 0;
}

static int tn_keyboard_write_and_read(void *w_buf, int w_len, void *r_buf, int *r_len){
    int ret = 0;
	int i = 0;
    int count = 3;
    pm_stay_awake(&tn_keyboard_client->plat_dev->dev);
    
	for(i = 0; i < count; i++){ 
        kb_debug("%s %d read_flag:%d i:%d start\r\n",__func__,__LINE__,tn_keyboard_client->read_flag,i);
	    ret = tn_keyboard_write(w_buf, w_len);
	    if(ret){	       
	        mdelay(100);
			continue;	       
	    }  
       
        if(r_buf == NULL)
            break;
	    ret = tn_keyboard_read(r_buf, r_len);
		if(ret == 0){
			break;
		}	
	}
    pm_relax(&tn_keyboard_client->plat_dev->dev);
	if(i >= count){
		kb_err("%s %d  ret:%d i:%d err\r\n",__func__,__LINE__,ret,i);
		return ret;
	}
    kb_debug("%s %d ret:%d i:%d ok\r\n",__func__,__LINE__,ret,i);
    return 0;
}

int  tn_keyboard_tp_ver(void) {
    char ver_reg[] = {0x82, 0x01, 0x07};
    char temp[100] = {0};
    int read_len = 0;
    int ret = 0;   
   
    ret = tn_keyboard_write_and_read(ver_reg, sizeof(ver_reg),temp,&read_len);
    if (ret < 0){
        kb_err("%s %d err:ret:%d \n",__func__,__LINE__,ret); 
		return ret;
    }
    //kb_debug("%s %d write:ret:%d \n",__func__,__LINE__,ret);  
   
    tn_keyboard_show_buf(temp, read_len); 
    return 0;
}
int  tn_keyboard_ver(void) {
    char ver_reg[] = {0x70, 0x03, 0x01,0x01, 0x01};
    char buf[] = {0x71,0x06,0x01,0x04};
    char temp[100] = {0};
    int read_len = 0;
    int count = 3;
    int ret = 0;   
    int i = 0;
    for(i = 0; i < count; i++){
        ret = tn_keyboard_write_and_read(ver_reg, sizeof(ver_reg),temp,&read_len);
        if (ret < 0){            
            kb_err("%s %d err:ret:%d \n",__func__,__LINE__,ret); 
    		continue;
        }    
        //kb_debug("%s %d write:ret:%d \n",__func__,__LINE__,ret); 
        if(memcmp(temp, buf, sizeof(buf)) == 0){
           break;
        }
    }
    tn_keyboard_show_buf(temp, read_len); 
    if(i >= count){
        kb_err("%s %d err:ret:%d \n",__func__,__LINE__,ret); 
        return -1;
    }
    return 0;
    
}

void tn_keyboard_reliability_test(void){
	int i = 0;
	int ret = 0;
    int sum = 0;
	int count = 2000;
	kb_debug("%s %d start i:%d\n",__func__,__LINE__,i); 
	for(i = 0; i < count; i++){
		if(test_type == 1){
			ret = tn_keyboard_ver();
		}else if(test_type == 2){
			ret = tn_keyboard_tp_ver();
		}else{
			kb_err("%s %d no test mode\r\n", __func__, __LINE__, test_type);
			return;
		}
		if(ret != 0){
		 	kb_err("%s %d i:%d err\r\n", __func__, __LINE__, i);
			sum++;
		}
	}
	if(sum != 0){
		kb_err("%s %d fail! sum:%d\r\n", __func__, __LINE__, sum);
	}
	kb_debug("%s %d end ok i:%d sum:%d \n",__func__,__LINE__,i,sum);  
}
void tn_keyboard_test(void){
	tn_keyboard_reliability_test();
}

static int tn_keyboard_set_led(char event)
{
    int ret=  0;          
    
    //char led_buf[] = {0x6A, 0x06, 0xA3, 0x04, 0x00, 0x00, 0x00, 0x00};//3 led cmd
    char led_buf[] = {0x6A, 0x03, 0x27, 0x01, 0x00};//caps led cmd
    kb_debug("%s %d tn_keyboard_event:%d  tn_keyboard_status:0x%02x\n",__func__,__LINE__,event,tn_keyboard_client->tn_keyboard_status);    

    
    if(tn_keyboard_client->tn_keyboard_status & KEYBOARD_CAPSLOCK_ON_STATUS){
        led_buf[4] = 0x01;
    }
    /*if(tn_keyboard_client->tn_keyboard_status & KEYBOARD_MUTEDISABLE_ON_STATUS){
        led_buf[6] = 0x01; 
    }
    if(tn_keyboard_client->tn_keyboard_status & KEYBOARD_MICDISABLE_ON_STATUS){
        led_buf[7] = 0x01; 
    } */ 
   
    kb_debug("%s %d tn_keyboard_event:%d  tn_keyboard_status:0x%02x\n",__func__,__LINE__,event,tn_keyboard_client->tn_keyboard_status); 
  
   // if(event >= KEYBOARD_CAPSLOCK_ON_EVENT)
    mdelay(15);
    switch(event){ 
        case  KEYBOARD_HOST_LCD_ON_EVENT:
			kb_debug("%s %d %d\r\n",__func__,__LINE__,event);
            ret = tn_keyboard_write(led_buf, sizeof(led_buf));
            if(ret){
                kb_err("%s %d err\r\n",__func__,__LINE__);
                return ret;
            } 
            break;
        /*case KEYBOARD_HOST_LCD_OFF_EVENT:
			kb_debug("%s %d %d\r\n",__func__,__LINE__,event);
            memset(&led_buf[4], 0, 4);
            ret = tn_keyboard_write(led_buf, sizeof(led_buf));
            if(ret){
                kb_err("%s %d err\r\n",__func__,__LINE__);
                //return ret;
            } 
            break;*/
        case KEYBOARD_PLUG_IN_EVENT:
			kb_debug("%s %d %d\r\n",__func__,__LINE__,event);;
            ret = tn_keyboard_write(led_buf, sizeof(led_buf));           
            if(ret){
                kb_err("%s %d err\r\n",__func__,__LINE__);
                //return ret;
            } 
            break;
        case KEYBOARD_PLUG_OUT_EVENT:  /* for pre development test*/
            /*kb_debug("%s %d %d\r\n",__func__,__LINE__,event);
            memset(&led_buf[4], 0, 4);
            ret = tn_keyboard_write(led_buf, sizeof(led_buf));           
            if(ret){
                kb_err("%s %d err\r\n",__func__,__LINE__);
                //return ret;
            } */
            break;
     
        case KEYBOARD_CAPSLOCK_ON_EVENT:
			kb_debug("%s %d %d\r\n",__func__,__LINE__,event);
      
            led_buf[4] = 0x01;
            ret = tn_keyboard_write(led_buf, sizeof(led_buf));           
            if(ret){
                kb_err("%s %d err\r\n",__func__,__LINE__);
                //return ret;
            } 

            break;
        case KEYBOARD_CAPSLOCK_OFF_EVENT:
            kb_debug("%s %d %d\r\n",__func__,__LINE__,event);              

            led_buf[4] = 0x00;
            ret = tn_keyboard_write(led_buf, sizeof(led_buf));           
            if(ret){
                kb_err("%s %d err\r\n",__func__,__LINE__);
                //return ret;
            } 

            break;
            

        /*case KEYBOARD_MUTEDISABLE_ON_EVENT:
			kb_debug("%s %d %d\r\n",__func__,__LINE__,event);

            led_buf[6] = 0x01;
            ret = tn_keyboard_write(led_buf, sizeof(led_buf));           
            if(ret){
                kb_err("%s %d err\r\n",__func__,__LINE__);
                //return ret;
            } 

            break;
        case KEYBOARD_MUTEDISABLE_OFF_EVENT:
			kb_debug("%s %d %d\r\n",__func__,__LINE__,event);

            led_buf[6] = 0x00;
            ret = tn_keyboard_write(led_buf, sizeof(led_buf));           
            if(ret){
                kb_err("%s %d err\r\n",__func__,__LINE__);
                //return ret;
            }
				

            break;
         
        case KEYBOARD_MICDISABLE_ON_EVENT:
			kb_debug("%s %d %d\r\n",__func__,__LINE__,event); 

            led_buf[7] = 0x01;
            ret = tn_keyboard_write(led_buf, sizeof(led_buf));           
            if(ret){
                kb_err("%s %d err\r\n",__func__,__LINE__);
                return ret;
            } 

            break;
        case KEYBOARD_MICDISABLE_OFF_EVENT:
			kb_debug("%s %d %d\r\n",__func__,__LINE__,event);  

            led_buf[7] = 0x00;
            ret = tn_keyboard_write(led_buf, sizeof(led_buf));           
            if(ret){
                kb_err("%s %d err\r\n",__func__,__LINE__);
                //return ret;
            } 

            break;*/
		default:
			kb_err("%s %d no event to do err\r\n",__func__,__LINE__);
			break;
     }
     sprintf(TAG,"%s %d",__func__,__LINE__);
     tn_keyboard_show_buf(led_buf, sizeof(led_buf)); 
	 kb_debug("%s %d tn_keyboard_event:%d  tn_keyboard_status:0x%02x\n",__func__,__LINE__,event,tn_keyboard_client->tn_keyboard_status);
     return ret;
}
/*
static int tn_keyboard_get_ver(void){
    int ret=  0;       
    char write_buf[] = {0x70, 0x03, 0x01, 0x01, 0x01};  
    char read_buf[255] = {0};
    int read_len = 0;
    kb_debug("%s %d \r\n",__func__,__LINE__); 
   
    //ret = tn_keyboard_write(write_buf, sizeof(write_buf));
    ret = tn_keyboard_write_and_read(write_buf, sizeof(write_buf),read_buf,&read_len);         
    if(ret){
        kb_err("%s %d err\r\n",__func__,__LINE__);
        return ret;
    }
    sprintf(TAG,"%s %d",__func__,__LINE__);
    tn_keyboard_show_buf(read_buf, read_len); 
    kb_debug("%s %d ok\r\n",__func__,__LINE__);
    return 0;
}*/

static int tn_keyboard_set_power(int power_status){
    int ret=  0;       
    char power_buf[] = {0x92, 0x03, 0x02, 0x01, 0x01}; 
    char buf_off[] = {0x93, 0x04, 0x02, 0x02, 0x01, 0x01};
    char buf_on[] = {0x93, 0x04, 0x02, 0x02, 0x00, 0x01};  
    char read_buf[255] = {0};
    int read_len = 0;
    int i = 0;
    kb_debug("%s %d power_status:%d\r\n",__func__,__LINE__,power_status); 
    if(power_status){        
        power_buf[4] = 0x00;          
    } else{  
    	//tn_keyboard_get_ver();
        power_buf[4] = 0x01;   
    }
    for(i = 0; i < 3; i++){    
        ret = tn_keyboard_write_and_read(power_buf, sizeof(power_buf),read_buf,&read_len);         
        if(ret){
            continue;        
        } 
        
        if(power_status){        
			if(memcmp(read_buf, buf_on, sizeof(buf_on)) == 0){
				kb_debug("%s %d set on ok\n",__func__,__LINE__); 
			    break;
			}           
		} else{  
			if(memcmp(read_buf, buf_off, sizeof(buf_off)) == 0){
				kb_debug("%s %d set off ok\n",__func__,__LINE__);
			    break;
			}
		}
  	
    }
    if(i >= 3){
        kb_err("%s %d err ret:0x%02x status:%d\r\n",__func__,__LINE__,ret,read_buf[4]);
        return ret;
    }
    sprintf(TAG,"%s %d",__func__,__LINE__);
    tn_keyboard_show_buf(read_buf, read_len); 
    kb_debug("%s %d ok ret:0x%02x status:%d\r\n",__func__,__LINE__,ret,read_buf[4]);
    return 0;
}



void tn_keyboard_led_report(int key_value){ 
    char value = 0;
    if(!( key_value == KEY_CAPSLOCK || key_value == KEY_MUTE || key_value == KEY_MICDISABLE))
        return;
    kb_debug("%s %d key_value:%d\r\n",__func__,__LINE__,key_value);    
    if(key_value == KEY_CAPSLOCK){
        if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_CAPSLOCK_ON_STATUS) == 0){
             tn_keyboard_client->tn_keyboard_status |= KEYBOARD_CAPSLOCK_ON_STATUS;             
        }else{
             tn_keyboard_client->tn_keyboard_status &= (~KEYBOARD_CAPSLOCK_ON_STATUS);
        }
  
        kb_debug("%s %d KEY_CAPSLOCK key_value:%d value:%d\r\n",__func__,__LINE__,key_value,value);  

    }
    else if(key_value == KEY_MUTE){
        if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_MUTEDISABLE_ON_STATUS) == 0){
             tn_keyboard_client->tn_keyboard_status |= KEYBOARD_MUTEDISABLE_ON_STATUS;             
        }else{
            tn_keyboard_client->tn_keyboard_status &= (~KEYBOARD_MUTEDISABLE_ON_STATUS);
        }
        value = tn_keyboard_client->tn_keyboard_status & KEYBOARD_MUTEDISABLE_ON_STATUS;
        //input_event(tn_keyboard_client->input_tn_keyboard, EV_LED, LED_MUTE, !!value);
        kb_debug("%s %d KEY_MUTE key_value:%d value:%d\r\n",__func__,__LINE__,key_value,value);  

    }else if(key_value == KEY_MICDISABLE){
        if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_MICDISABLE_ON_STATUS) == 0){
             tn_keyboard_client->tn_keyboard_status |= KEYBOARD_MICDISABLE_ON_STATUS;             
        }else{
            tn_keyboard_client->tn_keyboard_status &= (~KEYBOARD_MICDISABLE_ON_STATUS);
        }
        value = tn_keyboard_client->tn_keyboard_status & KEYBOARD_MICDISABLE_ON_STATUS;
        //input_event(tn_keyboard_client->input_tn_keyboard, EV_LED, LED_MIC_MUTE, !!value);   
        kb_debug("%s %d KEY_MICDISABLE key_value:%d value:%d\r\n",__func__,__LINE__,key_value,value);  
    }

}

void tn_keyboard_led_process(int code, int value){
     //kb_info("%s %d  type:led code:0x%2x value:0x%2x \n",__func__,__LINE__, code,  value);
     switch(code){
        case LED_CAPSL:
            if(value == 1){
                tn_keyboard_client->tn_keyboard_status |= KEYBOARD_CAPSLOCK_ON_STATUS;
                if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_POWER_ON_STATUS) != 0){
                    send_event(KEYBOARD_CAPSLOCK_ON_EVENT);
                }
            }
            else{
                tn_keyboard_client->tn_keyboard_status &= (~KEYBOARD_CAPSLOCK_ON_STATUS);
                if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_POWER_ON_STATUS) != 0){
                    send_event(KEYBOARD_CAPSLOCK_OFF_EVENT);
                }
            }
            break;
        /*case LED_MUTE:
            if(value == 1){
                send_event(KEYBOARD_MUTEDISABLE_ON_EVENT);
                tn_keyboard_client->tn_keyboard_status |= KEYBOARD_MUTEDISABLE_ON_STATUS;  
            }
            else{
                send_event(KEYBOARD_MUTEDISABLE_OFF_EVENT);
                tn_keyboard_client->tn_keyboard_status &= (~KEYBOARD_MUTEDISABLE_ON_STATUS);
            }    
            break;
        case LED_MIC_MUTE:
            if(value == 1){
                send_event(KEYBOARD_MICDISABLE_ON_EVENT);
                tn_keyboard_client->tn_keyboard_status |= KEYBOARD_MICDISABLE_ON_STATUS;
            }    
            else{
                send_event(KEYBOARD_MICDISABLE_OFF_EVENT);
                tn_keyboard_client->tn_keyboard_status &= (~KEYBOARD_MICDISABLE_ON_STATUS);
            }
            break; */     
        default:
            kb_debug("%s %d  type:led code:0x%2x value:0x%2x no support err!\n",__func__,__LINE__, code,  value); 
            return;
    }
    kb_info("%s %d  type:led code:0x%2x value:0x%2x tn_keyboard_status:%d\n",__func__,__LINE__, code,  value,tn_keyboard_client->tn_keyboard_status);
}

void tn_keyboard_power_process(int is_lcd_off_event){
    kb_debug("%s %d is_lcd_off:%d\r\n",__func__,__LINE__,is_lcd_off_event);   
    if(is_lcd_off_event == 0){        
        kb_info("%s %d tn_keyboard goto wakeup\r\n",__func__,__LINE__);  
        send_event(KEYBOARD_HOST_LCD_ON_EVENT);

    } else if(is_lcd_off_event == 1){
        kb_info("%s %d tn_keyboard goto sleep\r\n", __func__, __LINE__);
        send_event(KEYBOARD_HOST_LCD_OFF_EVENT);
    }
    kb_debug("%s %d ok\r\n",__func__,__LINE__);
    return;
}
EXPORT_SYMBOL(tn_keyboard_power_process);

static int tn_keyboard_notifier_callback(struct notifier_block *nb,unsigned long value, void *v)
{
    int data = 0;
    if (!v)
        return 0;

    data = *(int *)v;

    if (value == MTK_DISP_EVENT_BLANK) {
        if (data == MTK_DISP_BLANK_UNBLANK) {
            kb_debug("[%s] pogo keyboard enter UNBLANK\n", __func__);
            tn_keyboard_power_process(0);
        } else if (data == MTK_DISP_BLANK_POWERDOWN) {
            kb_debug("[%s] pogo keyboard enter early POWERDOWN\n", __func__);
            tn_keyboard_power_process(1);
        } else {
            kb_debug("[%s] data(%d) is not UNBLANK or POWERDOWN\n", __func__, data);
        }
    }

    return 0;
} 
/*static int tn_keyboard_notifier_callback(struct notifier_block *nb, unsigned long action, void *data){
    struct fb_event *event = NULL;
    int blank;
    kb_debug("%s %d tn_keyboard goto wakeup\r\n",__func__,__LINE__); 
    //return 0;
    if(action != FB_EVENT_BLANK)
        return 0;      
    event = data;
    blank = *(int*)event->data;
    switch(blank){
        case FB_BLANK_UNBLANK:
            kb_debug("%s %d tn_keyboard goto wakeup\r\n",__func__,__LINE__); 
            tn_keyboard_power_process(0);
            break;
        case FB_BLANK_POWERDOWN:
            kb_debug("%s %d tn_keyboard goto sleep\r\n", __func__, __LINE__);      
            tn_keyboard_power_process(1);           
            break;
        default:
            kb_debug("%s %d tn_keyboard no do\r\n", __func__, __LINE__);
            break;        
    }
    
    return 0;
}*/
int tn_keyboard_enable_uart_tx(int value){ 
	if(tn_keyboard_client->file_client == NULL)
		return -1;
	kb_debug("%s %d  value:%d gpiovalue:%d\n",__func__,__LINE__,value,gpio_get_value_cansleep(tn_keyboard_client->tx_en_gpio));	
    if(value == 1){
#ifdef CONFIG_BOARD_V4_SUPPORT  
        //pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_rx_clear);
        //if (gpio_get_value_cansleep(tn_keyboard_client->tx_en_gpio) == 0) {
			gpio_set_value_cansleep(tn_keyboard_client->tx_en_gpio,1); //bygx
			kb_debug("%s %d  set 1\n",__func__,__LINE__);	
		//}

        udelay(450);//for set mode valid	

#else
        pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_rx_clear);		
        pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_tx_set);
        udelay(450); //for set mode valid	
#endif
    }
    else if(value == 0){
#ifdef CONFIG_BOARD_V4_SUPPORT  
        udelay(300); //for set mode valid
        //if (gpio_get_value_cansleep(tn_keyboard_client->tx_en_gpio) == 1) {
			gpio_set_value_cansleep(tn_keyboard_client->tx_en_gpio,0); //bygx
			kb_debug("%s %d  set 0\n",__func__,__LINE__);	
		//}
        //pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_rx_set);

#else        
        udelay(300); //for set mode valid
        pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_tx_clear);
        pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_rx_set);	
#endif
    }
    return 0;
}

int tn_keyboard_uart_rx_mode_set(int value){
	int ret = 0;
	struct platform_device  *device = tn_keyboard_client->plat_dev;
	if(tn_keyboard_client->uart_rx_mode == value){
		kb_debug("%s %d  is same,no do uart_rx_mode:%d\n",__func__,__LINE__,value);	
		return -1;
	}
	switch(value){
		case KEYBOARD_UART_RX_CLEAR_TYPE:
			kb_debug("%s %d  uart_rx_clear\n",__func__,__LINE__);
			pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_rx_clear); 		
			tn_keyboard_client->uart_rx_mode = value;
			break;
		case KEYBOARD_UART_RX_SET_TYPE:
			disable_irq_wake(tn_keyboard_client->uart_rx_gpio_irq);
			free_irq(tn_keyboard_client->uart_rx_gpio_irq,NULL);
			pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_rx_set);	
			kb_debug("%s %d  uart_rx_set \n",__func__,__LINE__);
			tn_keyboard_client->uart_rx_mode = value;
			break;
		/*case KEYBOARD_UART_RX_GPIO_TYPE:
			kb_debug("%s %d  uart_rx_gpio\n",__func__,__LINE__);
			pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_rx_gpio_pin); 
			ret = devm_request_threaded_irq(&device->dev, tn_keyboard_client->uart_rx_gpio_irq, tn_keyboard_rx_gpio_irq_handler,
	                                    NULL, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "keybaord_rx_irq", NULL);	
			if (ret < 0) {
		        kb_err("request irq failed : %d\n", tn_keyboard_client->uart_rx_gpio_irq);
		        return -EINVAL;
		    }
			enable_irq_wake(tn_keyboard_client->uart_rx_gpio_irq);
			kb_debug("%s %d  uart_rx_gpio ok\n",__func__,__LINE__);
			tn_keyboard_client->uart_rx_mode = value;
			break;*/
		default:
			kb_debug("%s %d  no do\n",__func__,__LINE__);
			break;
	}
	kb_debug("%s %d  ok\n",__func__,__LINE__);
    return 0;
}
int tn_keyboard_get_dts_info(void){
	struct device_node *node = NULL;
	struct platform_device  *device = tn_keyboard_client->plat_dev;
	int ret = 0;
	kb_debug("%s %d start\n",__func__,__LINE__);
	node = of_find_compatible_node(NULL, NULL,KEYBOARD_CORE_NAME);
    if (!node) {
        kb_err("of node is not null\n");
        return -EINVAL;
    }
#ifdef CONFIG_PLUG_SUPPORT	 //bygxiong
    tn_keyboard_client->plug_gpio = of_get_named_gpio(node,"plug-gpios", 0);
    if (!gpio_is_valid(tn_keyboard_client->plug_gpio)) {
        kb_err("plug_gpio is not valid %d\n", tn_keyboard_client->plug_gpio);
        return -EINVAL;
    }
    kb_debug("%s %d plug_gpio:%d\n",__func__,__LINE__, tn_keyboard_client->plug_gpio);
  
    ret = gpio_request_one(tn_keyboard_client->plug_gpio, GPIOF_DIR_IN, "keybaord_plug_gpio");
    if(ret){
        kb_err("%s %d gpio_request_one keybaord_plug_gpio err:%d\n",__func__,__LINE__, ret);
        return ret;
    }
	
    tn_keyboard_client->plug_irq = gpio_to_irq(tn_keyboard_client->plug_gpio);
    ret = devm_request_threaded_irq(&device->dev, tn_keyboard_client->plug_irq, keybaord_core_plug_irq_handler,
                                    NULL, IRQF_TRIGGER_FALLING, "keybaord_plug_irq", NULL);
    if (ret < 0) {
        kb_err("request plug_irq failed : %d\n", tn_keyboard_client->plug_irq);
        return -EINVAL;
    }

    enable_irq_wake(tn_keyboard_client->plug_irq);

    
    tn_keyboard_client->power_en_gpio = of_get_named_gpio(node,"power-en-gpios", 0);
    if (!gpio_is_valid(tn_keyboard_client->power_en_gpio)) {
        kb_err("power_en_gpio is not valid %d\n", tn_keyboard_client->power_en_gpio);
        return -EINVAL;
    }
    kb_debug("%s %d power_en_gpio:%d\n",__func__,__LINE__,tn_keyboard_client->power_en_gpio);
    gpio_direction_output(tn_keyboard_client->power_en_gpio,0);
   
    /*tn_keyboard_client->lcda_en_gpio = of_get_named_gpio(node,"lcda-en-gpios", 0);
    if (!gpio_is_valid(tn_keyboard_client->lcda_en_gpio)) {
        kb_err("lcda-en-gpios is not valid %d\n", tn_keyboard_client->lcda_en_gpio);
        return -EINVAL;
    }*/

#endif
    tn_keyboard_client->uart_tx_gpio = of_get_named_gpio(node,"uart-tx-gpios", 0);
	if (!gpio_is_valid(tn_keyboard_client->uart_tx_gpio)) {
        kb_err("uart_tx_gpio is not valid %d\n", tn_keyboard_client->uart_tx_gpio);
        return -EINVAL;
    }
    tn_keyboard_client->uart_rx_gpio = of_get_named_gpio(node,"uart-rx-gpios", 0);
    if (!gpio_is_valid(tn_keyboard_client->uart_rx_gpio)) {
        kb_err("uart_rx_gpio is not valid %d\n", tn_keyboard_client->uart_rx_gpio);
        return -EINVAL;
    }
    kb_debug("%s %d uart_tx_gpio:%d uart_rx_gpio:%d\n",__func__,__LINE__, tn_keyboard_client->uart_tx_gpio,
		tn_keyboard_client->uart_rx_gpio);


	tn_keyboard_client->pinctrl = devm_pinctrl_get(&tn_keyboard_client->plat_dev->dev);
	if(IS_ERR(tn_keyboard_client->pinctrl)){
	    kb_err("%s %d devm_pinctrl_get  err\n",__func__,__LINE__);
	    return -1;
	}
	tn_keyboard_client->uart_tx_set = pinctrl_lookup_state(tn_keyboard_client->pinctrl, "uart_tx_set");
	if(IS_ERR(tn_keyboard_client->uart_tx_set)) {
	kb_debug("%s %d pinctrl_lookup_state uart_tx_set err\n",__func__,__LINE__);
	return -1;
	}
	tn_keyboard_client->uart_tx_clear = pinctrl_lookup_state(tn_keyboard_client->pinctrl,"uart_tx_clear");
	if(IS_ERR(tn_keyboard_client->uart_tx_clear)){
		kb_err("%s %d pinctrl_lookup_state uart_tx_clear err\n",__func__,__LINE__);
		return -1;
	}

	tn_keyboard_client->uart_rx_set = pinctrl_lookup_state(tn_keyboard_client->pinctrl, "uart_rx_set");
	if(IS_ERR(tn_keyboard_client->uart_rx_set)) {
		kb_err("%s %d pinctrl_lookup_state uart_rx_set err\n",__func__,__LINE__);
		return -1;
	}
	tn_keyboard_client->uart_rx_clear = pinctrl_lookup_state(tn_keyboard_client->pinctrl,"uart_rx_clear");
	if(IS_ERR(tn_keyboard_client->uart_rx_clear)){
		kb_err("%s %d pinctrl_lookup_state uart_rx_clear err\n",__func__,__LINE__);
		return -1;
	}
	tn_keyboard_client->uart_rx_gpio_pin = pinctrl_lookup_state(tn_keyboard_client->pinctrl,"uart_rx_gpio");
	if(IS_ERR(tn_keyboard_client->uart_rx_gpio_pin)){
		kb_err("%s %d pinctrl_lookup_state uart_rx_gpio err\n",__func__,__LINE__);
		return -1;
	}
	pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_rx_gpio_pin);  
	ret = gpio_request_one(tn_keyboard_client->uart_rx_gpio, GPIOF_DIR_IN, "tn_keyboard_rx_gpio");
    if(ret){
        kb_err("%s %d gpio_request_one uart_rx_gpio err:%d\n",__func__,__LINE__, ret);
        return ret;
    }
	tn_keyboard_client->uart_rx_gpio_irq = gpio_to_irq(tn_keyboard_client->uart_rx_gpio);
	
	pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_rx_clear);
	pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_rx_set); 
	pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_tx_clear); 
#ifdef CONFIG_BOARD_V4_SUPPORT
	/*tn_keyboard_client->uart_wake_gpio_pin = pinctrl_lookup_state(tn_keyboard_client->pinctrl,"uart_wake_gpio");
	if(IS_ERR(tn_keyboard_client->uart_wake_gpio_pin)){
		kb_err("%s %d pinctrl_lookup_state uart_wake_gpio_pin err\n",__func__,__LINE__);
		return -1;
	}
	pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_wake_gpio_pin);      

    tn_keyboard_client->uart_wake_clear = pinctrl_lookup_state(tn_keyboard_client->pinctrl,"uart_wake_clear");
	if(IS_ERR(tn_keyboard_client->uart_wake_clear)){
		kb_err("%s %d pinctrl_lookup_state uart_wake_clear err\n",__func__,__LINE__);
		return -1;
	}*/	
	

    tn_keyboard_client->uart_wake_gpio = of_get_named_gpio(node,"uart-wake-gpios", 0);
    if (!gpio_is_valid(tn_keyboard_client->uart_wake_gpio)) {
        kb_err("uart_wake_gpio is not valid %d\n", tn_keyboard_client->uart_wake_gpio);
        return -EINVAL;
    }
    /*ret = gpio_request_one(tn_keyboard_client->uart_wake_gpio, GPIOF_DIR_IN, "uart_wake_gpio");
    if(ret){
        kb_err("%s %d gpio_request_one uart_wake_gpio err:%d\n",__func__,__LINE__, ret);
        return ret;
    }
    tn_keyboard_client->uart_wake_gpio_irq = gpio_to_irq(tn_keyboard_client->uart_wake_gpio);

    
    ret = devm_request_threaded_irq(&device->dev, tn_keyboard_client->uart_wake_gpio_irq, tn_keyboard_wake_gpio_irq_handler,
                                NULL, IRQF_TRIGGER_RISING | IRQF_ONESHOT, "keybaord_wake_irq", NULL);	
    if (ret < 0) {
        kb_err("request irq failed : %d\n", tn_keyboard_client->uart_wake_gpio_irq);
        return -EINVAL;
    }
    enable_irq_wake(tn_keyboard_client->uart_wake_gpio_irq);
    set_wakeup_irq_switch(0);*/

    kb_debug("%s %d disable_irq_wake uart_wake_gpio_irq \n",__func__,__LINE__);


    tn_keyboard_client->tx_en_gpio = of_get_named_gpio(node,"tx-en-gpios", 0);
    if (!gpio_is_valid(tn_keyboard_client->tx_en_gpio)) {
        kb_err("tx_en_gpio is not valid %d\n", tn_keyboard_client->tx_en_gpio);
        //return -EINVAL;
    }
    kb_debug("%s %d uart_wake_gpio:%d tx_en_gpio:%d uart_wake_gpio_irq:%d\n",__func__,__LINE__, tn_keyboard_client->uart_wake_gpio,
		tn_keyboard_client->tx_en_gpio,tn_keyboard_client->uart_wake_gpio_irq);
	if (gpio_is_valid(tn_keyboard_client->tx_en_gpio)) {
        gpio_direction_output(tn_keyboard_client->tx_en_gpio,0); 
    }

	pinctrl_select_state(tn_keyboard_client->pinctrl, tn_keyboard_client->uart_tx_set);
    
#endif
    
	kb_debug("%s %d ok\n",__func__,__LINE__);
	return 0;
}
void tn_keyboard_power_enable(int value){
    if(!tn_keyboard_client || tn_keyboard_client->power_en_gpio == 0){
		kb_debug("%s %d cancel power_en_gpio == 0 value=%d\n",__func__,__LINE__,value);
		return;
	}
        
    if(value == 1){        
        gpio_direction_output(tn_keyboard_client->power_en_gpio,1);
        mdelay(30);
        kb_debug("%s %d enable value:%d\n",__func__,__LINE__,value);
    }else if(value == 0){  
        gpio_direction_output(tn_keyboard_client->power_en_gpio,0);        
        kb_debug("%s %d enable value:%d\n",__func__,__LINE__,value);
    }   
}

#ifdef CONFIG_BOARD_V4_SUPPORT
static irqreturn_t tn_keyboard_wake_gpio_irq_handler(int irq, void *data)
{
    if(((tn_keyboard_client->tn_keyboard_status & KEYBOARD_POWER_ON_STATUS) == 0) &&
        ((tn_keyboard_client->tn_keyboard_status & KEYBOARD_CONNECT_STATUS) == 1) &&
           tn_keyboard_client->uart_wake_gpio_irq_count == 3){
        kb_debug("%s %d send_event KEYBOARD_WAKEUP_IRQ_EVENT \n",__func__,__LINE__);
        send_event(KEYBOARD_WAKEUP_IRQ_EVENT);
    }
    tn_keyboard_client->uart_wake_gpio_irq_count++;
    return IRQ_HANDLED;
}

#else
static irqreturn_t tn_keyboard_rx_gpio_irq_handler(int irq, void *data)
{
	kb_debug("%s %d   \n",__func__,__LINE__);
    disable_irq_wake(tn_keyboard_client->uart_rx_gpio_irq);
    send_event(KEYBOARD_HOST_RX_UART_EVENT);
    return IRQ_HANDLED;
}
#endif

static ssize_t tx_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char *after;
	unsigned long value = simple_strtoul(buf, &after, 10);
	kb_debug("%s %d   %s  %d\n",__func__,__LINE__, buf,value);
	if(value == 1){
        tn_keyboard_enable_uart_tx(1);
	}else if(value == 0){
        tn_keyboard_enable_uart_tx(0);
	}
	return count;
}

static ssize_t tx_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{   
	return 0;
}
static ssize_t test_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char *after;
	unsigned long value = simple_strtoul(buf, &after, 10);
	kb_debug("%s %d   %s  %d\n",__func__,__LINE__, buf,value);	
	switch(value){
		case 1:
		case 2:
			test_type = value;
			kb_debug("%s %d   %s  test_type:%d\n",__func__,__LINE__, buf,test_type);
			send_event(KEYBOARD_TEST_EVENT);
			break;
		default:
			break;
	    
	}	
	return count;
}

static ssize_t test_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{   
	return 0;
}

static ssize_t keyboard_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{   
	return sprintf(buf, "status:0x%2x\n", tn_keyboard_client->tn_keyboard_status);
}

static ssize_t hall_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{   
    int value = gpio_get_value(tn_keyboard_client->plug_gpio);
	return sprintf(buf, "status:0x%2x\n", tn_keyboard_client->tn_keyboard_status);
}

static DEVICE_ATTR(tx_mode, S_IRUGO | S_IWUSR, tx_mode_show, tx_mode_store);
static DEVICE_ATTR(test_mode, S_IRUGO | S_IWUSR, test_mode_show, test_mode_store);
static DEVICE_ATTR(keyboard_status, S_IRUGO | S_IWUSR, keyboard_status_show, NULL);
static DEVICE_ATTR(hall_status, S_IRUGO | S_IWUSR, hall_status_show, NULL);


/* add your attr in here*/
static struct attribute *tn_keyboard_attributes[] = {
	&dev_attr_tx_mode.attr,
	&dev_attr_test_mode.attr,
	&dev_attr_keyboard_status.attr,
	&dev_attr_hall_status.attr,
	NULL
};

static struct attribute_group tn_keyboard_attribute_group = {
	.attrs = tn_keyboard_attributes
};

static void tn_keyboard_input_disconnect(void){
    if(tn_keyboard_client->input_touchpad){
        
        input_unregister_device(tn_keyboard_client->input_touchpad);
		//input_free_device(tn_keyboard_client->input_touchpad);
        kb_debug("%s %d input_unregister_device \n",__func__,__LINE__);
        tn_keyboard_client->input_touchpad = NULL;
    }
    if(tn_keyboard_client->input_tn_keyboard){
        input_unregister_device(tn_keyboard_client->input_tn_keyboard);
		//input_free_device(tn_keyboard_client->input_tn_keyboard);
        kb_debug("%s %d input_unregister_device \n",__func__,__LINE__);
        tn_keyboard_client->input_tn_keyboard = NULL; 
    }
    return;
}


static void tn_keyboard_hander_call_back(void){
    kb_info("%s %d  event:0x%2x\n",__func__,__LINE__,tn_keyboard_client->new_event);
    mutex_lock(&tn_keyboard_client->mutex);
    if(tn_keyboard_client->call_back){
        tn_keyboard_client->call_back(tn_keyboard_client->new_event);
        //tn_keyboard_client->call_back = NULL;
    }
    mutex_unlock(&tn_keyboard_client->mutex);
}



static int tn_keyboard_event_process(unsigned char tn_keyboard_event){
    int value = 0;
    int ret = 0; 
    
    kb_debug("%s %d BEGIN tn_keyboard_event:%d  tn_keyboard_status:0x%02x\n",__func__,__LINE__,tn_keyboard_event,tn_keyboard_client->tn_keyboard_status);
    switch(tn_keyboard_event){
        case KEYBOARD_PLUG_IN_EVENT:
			hrtimer_cancel(&tn_keyboard_client->plug_timer);
			tn_keyboard_client->tn_keyboard_status |= KEYBOARD_CONNECT_STATUS;
            ret = tn_keyboard_input_connect();
			if(ret){
				kb_err("%s %d tn_keyboard_input_connect err!\n",__func__,__LINE__);
			}
			mdelay(40);
			tn_keyboard_client->tn_keyboard_status |= KEYBOARD_INPUT_INIT_STATUS;
			kb_err("%s %d KEYBOARD_PLUG_IN_EVENT END!\n",__func__,__LINE__);
            break;
        case KEYBOARD_WAKEUP_IRQ_EVENT:
            if (!IS_ERR(sec_device)) {
                //kb_debug("%s %d KEYBOARD_WAKEUP_IRQ_EVENT KOBJ_ADD!\n",__func__,__LINE__);
                //kobject_uevent(&sec_device->kobj, KOBJ_ADD);
            }else{
                kb_debug("%s %d KEYBOARD_WAKEUP_IRQ_EVENT KOBJ_ADD ERROR!\n",__func__,__LINE__);
            }
            kb_err("%s %d KEYBOARD_WAKEUP_IRQ_EVENT ok!\n",__func__,__LINE__);
            break;
        case KEYBOARD_PAD_HALL_CLOSE:
            tn_keyboard_power_enable(0);
            kb_err("%s %d KEYBOARD_PAD_HALL_CLOSE ok!\n",__func__,__LINE__);
            break;
        case KEYBOARD_PLUG_VCC_EVENT: 	
				value = gpio_get_value(tn_keyboard_client->plug_gpio);
				kb_info("KEYBOARD_PLUG_VCC_EVENT BEGIN plug irq gpio = %d\r\n",value);
				if(value == 0){
					pm_stay_awake(&tn_keyboard_client->plat_dev->dev);
                    if (!IS_ERR(sec_device)) {
                        kb_debug("%s %d KEYBOARD_PLUG_VCC_EVENT KOBJ_ADD!\n",__func__,__LINE__);
                        kobject_uevent(&sec_device->kobj, KOBJ_ADD);
                    }else{
                        kb_debug("%s %d KEYBOARD_PLUG_VCC_EVENT KOBJ_ADD ERROR!\n",__func__,__LINE__);
                    }
                    if(tn_keyboard_client->vcc_on == 1){
						kb_debug("%s %d KEYBOARD_PLUG_VCC_EVENT END!tn_keyboard_client->vcc_on == 1\n",__func__,__LINE__);
						goto vcc_end;
					}
					tn_keyboard_client->vcc_on = 1;
                    irq_set_irq_type(tn_keyboard_client->plug_irq, IRQF_TRIGGER_RISING);
#ifdef CONFIG_ADC_SUPPORT
					tn_keyboard_adc_get();
					kb_debug("%s %d adc_ch:%d\n",__func__,__LINE__,tn_keyboard_adc_get());               
#endif
					tn_keyboard_input_power_key_report();
					tn_keyboard_power_enable(1);
                    tn_keyboard_uart_rx_mode_set(KEYBOARD_UART_RX_SET_TYPE);
                    if(!tn_keyboard_client->uart_wake_gpio_irq && tn_keyboard_client->plat_dev != NULL){
						kb_debug("%s %d first init wake gpio\n",__func__,__LINE__);
						//first init wake gpio
						ret = gpio_request_one(tn_keyboard_client->uart_wake_gpio, GPIOF_DIR_IN, "uart_wake_gpio");
						if(ret){
							kb_err("%s %d gpio_request_one uart_wake_gpio err:%d\n",__func__,__LINE__, ret);
							//return ret;
						}else{
							tn_keyboard_client->uart_wake_gpio_irq = gpio_to_irq(tn_keyboard_client->uart_wake_gpio);
							ret = devm_request_threaded_irq(&tn_keyboard_client->plat_dev->dev, tn_keyboard_client->uart_wake_gpio_irq, tn_keyboard_wake_gpio_irq_handler,
														NULL, IRQF_TRIGGER_RISING | IRQF_ONESHOT, "keybaord_wake_irq", NULL);	
							if (ret < 0) {
								kb_err("request irq failed : %d\n", tn_keyboard_client->uart_wake_gpio_irq);
								//return -EINVAL;
							}else{
								enable_irq_wake(tn_keyboard_client->uart_wake_gpio_irq);
								set_wakeup_irq_switch(0);
							}
						}
					}
				vcc_end:
					pm_relax(&tn_keyboard_client->plat_dev->dev);
                    kb_debug("%s %d KEYBOARD_PLUG_VCC_EVENT END!vcc_on:%d\n",__func__,__LINE__,tn_keyboard_client->vcc_on);
				}
				else{
					irq_set_irq_type(tn_keyboard_client->plug_irq, IRQF_TRIGGER_FALLING);
					send_event(KEYBOARD_PLUG_OUT_EVENT);
				}
            break;
        case KEYBOARD_PLUG_OUT_EVENT:
                tn_keyboard_client->vcc_on = 0;
#ifdef CONFIG_ADC_SUPPORT
                tn_keyboard_adc_get();
                kb_debug("%s %d adc_ch:%d\n",__func__,__LINE__,tn_keyboard_adc_get());
#endif
                kb_debug("%s %d KEYBOARD_PLUG_OUT_EVENT BEGIN!\n",__func__,__LINE__);
                tn_keyboard_client->tn_keyboard_status &= ~KEYBOARD_CONNECT_STATUS;
                set_wakeup_irq_switch(0);
                tn_keyboard_uart_rx_mode_set(KEYBOARD_UART_RX_CLEAR_TYPE);
#ifdef CONFIG_POWER_CTRL_SUPPORT
                tn_keyboard_power_enable(0);
#endif
                if (!IS_ERR(sec_device)) {
                    kb_debug("%s %d KEYBOARD_PLUG_OUT_EVENT KOBJ_REMOVE!\n",__func__,__LINE__);
                    kobject_uevent(&sec_device->kobj, KOBJ_REMOVE);
                }else{
                    kb_debug("%s %d KEYBOARD_PLUG_OUT_EVENT KOBJ_REMOVE ERROR!\n",__func__,__LINE__);
                }
                
				tn_keyboard_input_disconnect();
				tn_keyboard_client->tn_keyboard_status &= ~KEYBOARD_INPUT_INIT_STATUS;
				kb_err("%s %d KEYBOARD_PLUG_OUT_EVENT END!\n",__func__,__LINE__);
            break;

        case KEYBOARD_HOST_LCD_ON_EVENT:
                if(tn_keyboard_client->vcc_on == 1){
                    if(gpio_get_value(tn_keyboard_client->power_en_gpio) == 0){
                        kb_err("%s %d KEYBOARD_HOST_LCD_ON_EVENT enable vcc!\n",__func__,__LINE__);
                        tn_keyboard_power_enable(1);
                    }
                    if (!IS_ERR(sec_device)) {
                        kb_debug("%s %d KEYBOARD_HOST_LCD_ON_EVENT KOBJ_ADD!\n",__func__,__LINE__);
                        kobject_uevent(&sec_device->kobj, KOBJ_ADD);
                    }
                }
				tn_keyboard_start_tp_down_loss_check_timer(0);
                if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_CONNECT_STATUS) != 0){                
                    set_wakeup_irq_switch(0);
                    if (!IS_ERR(sec_device)) {
                        //kb_debug("%s %d KEYBOARD_HOST_LCD_ON_EVENT KOBJ_ADD!\n",__func__,__LINE__);
                        //kobject_uevent(&sec_device->kobj, KOBJ_ADD);
                        //mdelay(10);
                    }else{
                        kb_debug("%s %d KEYBOARD_HOST_LCD_ON_EVENT KOBJ_ADD ERROR!\n",__func__,__LINE__);
                    }
                    ret = tn_keyboard_set_power(1);
                    if(ret){
                        kb_err("%s %d tn_keyboard_set_power err!\n",__func__,__LINE__);
                        //break;
                    }
                    tn_keyboard_set_led(tn_keyboard_event);
                    //tn_keyboard_input_power_key_report();
                }
            tn_keyboard_client->tn_keyboard_status |= KEYBOARD_POWER_ON_STATUS;
            kb_err("%s %d KEYBOARD_HOST_LCD_ON_EVENT disable_wake_irq ok!\n",__func__,__LINE__);
            break;
        case KEYBOARD_HOST_LCD_OFF_EVENT:   
          //  if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_POWER_ON_STATUS) != 0){
                if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_CONNECT_STATUS) != 0){
                    //tn_keyboard_set_led(tn_keyboard_event);             
                    mdelay(10);
                    ret = tn_keyboard_set_power(0);
                    if(ret){
                        kb_err("%s %d tn_keyboard_set_power err!\n",__func__,__LINE__);
                        //break;
                    }
                    if (!IS_ERR(sec_device)) {
                        //kb_debug("%s %d KEYBOARD_HOST_LCD_OFF_EVENT KOBJ_REMOVE!\n",__func__,__LINE__);
                        //kobject_uevent(&sec_device->kobj, KOBJ_REMOVE);
                        //mdelay(10);
                    }else{
                        kb_debug("%s %d KEYBOARD_HOST_LCD_OFF_EVENT KOBJ_REMOVE ERROR!\n",__func__,__LINE__);
                    }
                    set_wakeup_irq_switch(1);
                }
            //}
            
            tn_keyboard_client->tn_keyboard_status &= ~KEYBOARD_POWER_ON_STATUS;
            kb_err("%s %d KEYBOARD_HOST_LCD_OFF_EVENT enable_wake_irq ok!\n",__func__,__LINE__);
            break;

        case KEYBOARD_CAPSLOCK_ON_EVENT:          
        case KEYBOARD_CAPSLOCK_OFF_EVENT: 
        case KEYBOARD_MUTEDISABLE_ON_EVENT:           
        case KEYBOARD_MUTEDISABLE_OFF_EVENT:  
        case KEYBOARD_MICDISABLE_ON_EVENT:         
        case KEYBOARD_MICDISABLE_OFF_EVENT:
            if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_CONNECT_STATUS) != 0){
                tn_keyboard_set_led(tn_keyboard_event); 
            }
            break;
       
		case KEYBOARD_HOST_RX_GPIO_EVENT:
#ifdef CONFIG_BOARD_V4_SUPPORT  
            if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_CONNECT_STATUS) != 0){
                enable_irq_wake(tn_keyboard_client->uart_wake_gpio_irq);
            }
#else
            tn_keyboard_uart_rx_mode_set(2);
#endif
			break;
		case KEYBOARD_HOST_RX_UART_EVENT:
#ifdef CONFIG_BOARD_V4_SUPPORT   
            disable_irq_wake(tn_keyboard_client->uart_wake_gpio_irq);  
#else
            tn_keyboard_uart_rx_mode_set(1);
#endif
			break;
        case KEYBOARD_HOST_CHECK_EVENT: 
            ret = tn_keyboard_ver();            
            if(ret != 0){
                tn_keyboard_input_disconnect();
                tn_keyboard_client->tn_keyboard_status &= ~KEYBOARD_CONNECT_STATUS;   
#ifdef CONFIG_POWER_CTRL_SUPPORT
                tn_keyboard_power_enable(0);                
#endif
            }
            break;
		case KEYBOARD_TEST_EVENT:
			tn_keyboard_test(); //for test
			break;
        default:
            kb_err("%s %d no event do!\n",__func__,__LINE__);
            break;
    }
    kb_debug("%s %d END tn_keyboard_event:%d  tn_keyboard_status:0x%02x\n",__func__,__LINE__,tn_keyboard_event,tn_keyboard_client->tn_keyboard_status);
    return 0;
}

#ifdef CONFIG_PLUG_SUPPORT //bygxiong
static irqreturn_t keybaord_core_plug_irq_handler(int irq, void *data)
{
    send_event(KEYBOARD_PLUG_VCC_EVENT);
    return IRQ_HANDLED;
}
#endif
   
static int tn_keyboard_event_handler(void *unused){

	struct sched_param param = { .sched_priority = 4 };
	sched_setscheduler(current, SCHED_RR, &param);  
	
	do {		
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tn_keyboard_client->flag != 0);
		tn_keyboard_client->flag = 0;
		set_current_state(TASK_RUNNING); 
        tn_keyboard_hander_call_back();  
	} while (!kthread_should_stop());

	kb_debug("touch_event_handler exit\n");

	return 0;
}

int tn_keyboard_dma_write_call_set(int enable){ 
    if(enable == 1){
        tn_keyboard_enable_uart_tx(1);			
		kb_debug("%s %d write start\n",__func__,__LINE__);
    }else if(enable == 0){
        tn_keyboard_enable_uart_tx(0);			
		kb_debug("%s %d write end\n",__func__,__LINE__);
    }else{
        kb_err("%s %d  param err!\n",__func__,__LINE__);
        return -1;
    }   
    return 0;
}

struct file* tn_keyboard_port_to_file(struct uart_8250_port *uart_8250_port){
    struct uart_port  *uart_port = &uart_8250_port->port;
	struct uart_state   *state = uart_port->state;
	struct tty_struct *tty = state->port.itty;
	struct tty_file_private *priv;
	struct file *filp = NULL;
	spin_lock(&tty->files_lock);
    list_for_each_entry(priv, &tty->tty_files, list){
		if(priv != NULL){
            kb_debug("%s %d  major:%d minor:%d found!ttyS majodr=4 minor=65 priv->file:%p", __func__,__LINE__,imajor(priv->file->f_inode),iminor(priv->file->f_inode),priv->file);
			if(filp == NULL && (imajor(priv->file->f_inode) == 4 || iminor(priv->file->f_inode) == 65)){
				filp = priv->file;
				kb_debug("%s %d  %p found!file_client:%p", __func__,__LINE__,priv->file,tn_keyboard_client->file_client);
				//break;
			}
            if(tn_keyboard_client->file_client != NULL && tn_keyboard_client->file_client == priv->file){
                filp = tn_keyboard_client->file_client;
                kb_debug("%s %d priv->file:%p == file_client:%p", __func__,__LINE__,priv->file,tn_keyboard_client->file_client);
            }
		}
	}
	spin_unlock(&tty->files_lock);
	return filp;
}

int tn_keyboard_dma_set_call_back(void *uart_8250_port, int type){ 
    struct file *filp;
    if(type == 1){
        tn_keyboard_client->port = (struct uart_8250_port *)uart_8250_port;
		filp = tn_keyboard_port_to_file(tn_keyboard_client->port);
	    if(tn_keyboard_client->file_client == NULL && filp != NULL){
			tn_keyboard_client->file_client = filp;
			hrtimer_cancel(&tn_keyboard_client->plug_timer);
	    }else{
            kb_debug("%s %d  open error! file_client:%p filp:%p\n",__func__,__LINE__,tn_keyboard_client->file_client,filp);
        }
		kb_debug("%s %d  open\n",__func__,__LINE__);
    }else if(type == 0){
        tn_keyboard_client->file_client = NULL;
		tn_keyboard_client->port = NULL;
		kb_debug("%s %d  close\n",__func__,__LINE__);
    }else{
        kb_err("%s %d  param err!\n",__func__,__LINE__);
        return -1;
    }
    return 0;
}

/**
 * 监听pad皮套hall状态
 * status 1:合盖皮套    0:打开皮套
 * */
static void tn_keyboard_pad_hall_status_call_back(int status){
	kb_err("%s %d  pad_hall_status:%d!\n",__func__,__LINE__,status);
	if(status == PAD_HALL_STATU_CLOSE){
		enable_pad_hall_delay_close_vcc_timer(1);
	}else{
		enable_pad_hall_delay_close_vcc_timer(0);
		if(tn_keyboard_client->vcc_on == 1){
            if(gpio_get_value(tn_keyboard_client->power_en_gpio) == 0){
                kb_err("%s %d pad hall open enable vcc!\n",__func__,__LINE__);
                tn_keyboard_power_enable(1);
                return;
            }
		}
        kb_err("%s %d pad hall open not handle vcc!vcc_hall:%d vcc_gpio_value:%d:!\n",__func__,__LINE__,tn_keyboard_client->vcc_on,gpio_get_value(tn_keyboard_client->power_en_gpio));
	}
}

static void tn_keyboard_register_call_back(void){
    kb_debug("%s %d start\n",__func__,__LINE__);

    if(uart_dma_call_back == NULL){
        uart_dma_call_back = tn_keyboard_analyze;
    }

    if(uart_dma_write_call_back == NULL){
        uart_dma_write_call_back = tn_keyboard_dma_write_call_set;
    }

	if(uart_dma_set_call_back == NULL){
        uart_dma_set_call_back = tn_keyboard_dma_set_call_back;		 
    }
    
    if(pad_hall_status_call_back == NULL){
        pad_hall_status_call_back = tn_keyboard_pad_hall_status_call_back;		 
    }

}
static int tn_keyboard_start_up_init(void){
	ktime_t ktime = ktime_set(5, 1000*1000*10);
    tn_keyboard_client->tn_keyboard_status |= KEYBOARD_POWER_ON_STATUS; 
    kb_debug("%s %d tn_keyboard_status:0x%02x\n",__func__,__LINE__,tn_keyboard_client->tn_keyboard_status);
	tn_keyboard_client->uart_rx_mode = 1;

    kb_debug("%s %d hrtimer start plug_timer 5s",__func__,__LINE__);
    hrtimer_start(&tn_keyboard_client->plug_timer, ktime, HRTIMER_MODE_REL);


    return 0;
}

static enum hrtimer_restart keybaord_core_plug_hrtimer(struct hrtimer *timer)
{	
    int value = 0;  
    int ret = -1; 
    ktime_t ktime;
    value = 0; //bygxiong default connect	    

#ifdef CONFIG_PLUG_SUPPORT  //bygxiong
    value = gpio_get_value(tn_keyboard_client->plug_gpio);  
#endif	
    kb_debug("gpio read value = %d\r\n",value);
    if(value == 0){
        send_event(KEYBOARD_PLUG_VCC_EVENT);           
        kb_debug("%s %d %d\n",__func__,__LINE__,value);
        ktime = ktime_set(1, 0);
		hrtimer_start(&tn_keyboard_client->plug_timer, ktime, HRTIMER_MODE_REL);
		kb_debug("%s %d hrtimer restart timer!",__func__,__LINE__);      
    }
    else{       
        send_event(KEYBOARD_PLUG_OUT_EVENT);
        kb_debug("%s %d %d\n",__func__,__LINE__,value);       
    }
	return HRTIMER_NORESTART;
}

static enum alarmtimer_restart keybaord_core_tp_down_loss_hrtimer(struct alarm *alarm,ktime_t now)
{
	kb_debug("%s %d tp down loss check timer report power!\n",__func__,__LINE__);
	tn_keyboard_client->tp_down_loss_timer_enable = 0;
    tn_keyboard_input_power_key_report();
	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart pad_hall_delay_close_vcc_timer(struct alarm *alarm,ktime_t now)
{
	kb_debug("%s %d disenable kb vcc!\n",__func__,__LINE__);
	send_event(KEYBOARD_PAD_HALL_CLOSE);
	return ALARMTIMER_NORESTART;
}

static int tn_keyboard_plat_probe(struct platform_device  *device){
    int ret = 0;   
    struct device *dev = &device->dev;
    kb_debug("%s %d start\n",__func__,__LINE__);
    tn_keyboard_client = kzalloc(sizeof(*tn_keyboard_client), GFP_KERNEL);    
	 
    if(!tn_keyboard_client){
         kb_err("%s %d kzalloc err\n",__func__,__LINE__);
         return -ENOMEM;
    }    
    mutex_init(&tn_keyboard_client->mutex);    
    tn_keyboard_client->tn_keyboard_notify.notifier_call = tn_keyboard_notifier_callback;
    ret = mtk_disp_notifier_register("tn_keyboard",&tn_keyboard_client->tn_keyboard_notify);
    kb_debug("%s %d fb_register_client ret:%d\n",__func__,__LINE__, ret);
    if(ret){
        kb_err("%s %d fb_register_client err:%d\n",__func__,__LINE__, ret);
        return ret;
    }
    tn_keyboard_client->tn_keyboard_task = kthread_run(tn_keyboard_event_handler, 0, "tn_keyboard_task");
    if (IS_ERR_OR_NULL(tn_keyboard_client->tn_keyboard_task)) {
        kb_err("%s %d kthread_run err\n",__func__,__LINE__);
        tn_keyboard_client->tn_keyboard_task = NULL;        
    }
    tn_keyboard_client->plat_dev = device;
    
    ret = sysfs_create_group(&device->dev.kobj, &tn_keyboard_attribute_group);
    if (ret != 0) {
		kb_err("%s %d sysfs_create_group err\n",__func__,__LINE__);
		sysfs_remove_group(&device->dev.kobj, &tn_keyboard_attribute_group);
		return -EIO;
	}
	hrtimer_init(&tn_keyboard_client->plug_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	tn_keyboard_client->plug_timer.function = keybaord_core_plug_hrtimer;   

    /*hrtimer_init(&tn_keyboard_client->check_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	tn_keyboard_client->check_timer.function = keybaord_core_check_hrtimer; */
	alarm_init(&tn_keyboard_client->tp_down_loss_check_timer, ALARM_BOOTTIME, keybaord_core_tp_down_loss_hrtimer);
	alarm_init(&tn_keyboard_client->pad_hall_delay_close_vcc_timer, ALARM_BOOTTIME, pad_hall_delay_close_vcc_timer);
   
	tn_keyboard_start_up_init();
	ret = tn_keyboard_get_dts_info();
	if(ret != 0){
		return ret;
	}
	ret = tn_keyboard_input_wakeup_init();
	if(ret != 0){
		return ret;
	}
#ifdef CONFIG_ADC_SUPPORT    
    tn_keyboard_adc_init();
#endif
    
    tn_keyboard_register_call_back();	
    device_init_wakeup(dev, 1);   
    kb_info("%s %d ok\n",__func__,__LINE__);
    return 0;
}

static int tn_keyboard_plat_remove(struct platform_device *device){
    fb_unregister_client(&tn_keyboard_client->tn_keyboard_notify);
    input_unregister_device(tn_keyboard_client->input_touchpad);
    input_unregister_device(tn_keyboard_client->input_tn_keyboard);
    kfree(tn_keyboard_client);    
    kb_debug("%s %d \n",__func__,__LINE__);
    return 0;
}

static int tn_keyboard_plat_suspend(struct platform_device *device, pm_message_t state){     
    //send_event(KEYBOARD_HOST_RX_GPIO_EVENT);
    kb_info("%s %d \n",__func__,__LINE__);
    return 0;
}

static int tn_keyboard_plat_resume(struct platform_device *device){   
    //send_event(KEYBOARD_HOST_RX_UART_EVENT);
    kb_info("%s %d \n",__func__,__LINE__);
    return 0;
}
static void tn_keyboard_plat_shutdown(struct platform_device *device){
    kb_debug("%s %d \n",__func__,__LINE__);    
}
static const struct of_device_id tn_keyboard_plat_of_match[] = {
    {.compatible = KEYBOARD_CORE_NAME,},
    {},
};
static struct platform_driver tn_keyboard_plat_driver ={
    .probe = tn_keyboard_plat_probe,
    .remove = tn_keyboard_plat_remove,
    .suspend = tn_keyboard_plat_suspend,
    .resume = tn_keyboard_plat_resume,
    .shutdown = tn_keyboard_plat_shutdown,
    .driver = {
        .name = KEYBOARD_CORE_NAME,
        .owner = THIS_MODULE,
        .of_match_table = tn_keyboard_plat_of_match,
    }
};


static int __init tn_keyboard_mod_init(void){
    int ret = 0; 	
	kb_debug("%s %d start\n",__func__,__LINE__);	


    ret = platform_driver_register(&tn_keyboard_plat_driver);
    if(ret){
        kb_err("%s %d platform_driver_register err\n",__func__,__LINE__);
        return ret;
    }

    sec_class = class_create(THIS_MODULE, "pogopin");
    if (IS_ERR(sec_class)){
        kb_info("%s %d class_create fail!\n",__func__,__LINE__);
    }else{
        sec_device = device_create(sec_class, NULL, 0, NULL, "keyboard");
        if (IS_ERR(sec_device)) {
            kb_info("%s %d device_create fail!\n",__func__,__LINE__);
            class_destroy(sec_class);
        }else{
            kb_info("%s %d class device ok\n",__func__,__LINE__);
        }
    }
    
    kb_info("%s %d ok\n",__func__,__LINE__);
    return 0;
}

static void __exit tn_keyboard_mod_exit(void){
    platform_driver_unregister(&tn_keyboard_plat_driver);
    device_destroy(sec_class, 0);
    class_destroy(sec_class);
    kb_info("%s %d ok\n",__func__,__LINE__);
}

late_initcall(tn_keyboard_mod_init);
module_exit(tn_keyboard_mod_exit);

MODULE_AUTHOR("Tinno Team Inc");
MODULE_DESCRIPTION("Tinno Keyboard Driver v1.0");
MODULE_LICENSE("GPL");





