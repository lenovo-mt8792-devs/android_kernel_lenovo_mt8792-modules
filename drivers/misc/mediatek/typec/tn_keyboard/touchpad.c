#include "tn_keyboard.h"
#include <linux/input/mt.h>


int touchpad_input_report(char *buf){     
    struct input_dev *input_dev = tn_keyboard_client->input_touchpad;
    struct touch_event *event =  &tn_keyboard_client->event;
    struct touch_event temp;      
    char *data = tn_keyboard_client->data;     
    int i = 0, j = 0;
    int fingers = 0;    
    int len = 0;
    int offset = 0;
    unsigned char temp_touch_down = 0;
    if(!buf){
        return -EINVAL;
    }    
    if(!input_dev)
        return -EINVAL;
    fingers = (buf[0] - 1)/5;
    len = fingers * 5 + 2;
    if(!memcmp(data, buf, len)){
        kb_debug("%s %d data repeat,no do finger:%d len:%d!\n",__func__,__LINE__,fingers,len);
        return 0;
    }

    memset(&temp, 0 , sizeof(temp));
    memcpy(data, buf, len);
    if(tn_keyboard_client->pre_fingers != fingers){
		kb_debug("%s %d finger change!flingers:%d -> %d!\n",__func__,__LINE__,tn_keyboard_client->pre_fingers,fingers);
        if(tn_keyboard_client->touch_down > 0){
            for(j = 0; j < fingers; j++){
                offset = 2 + 5 * j;        
                temp.id = data[offset + 0]>>4 & 0xf;
                temp_touch_down |= BIT(temp.id);
            }

            if(unlikely(tn_keyboard_client->touch_down ^ temp_touch_down)){
				kb_debug("%s %d auto report finger up!touch_down:%d temp_touch_down:%d\n",__func__,__LINE__,tn_keyboard_client->touch_down,temp_touch_down);
                for(i = 0; i < TOUCH_FINGER_MAX; i++){
                    if (BIT(i) & (tn_keyboard_client->touch_down ^ temp_touch_down)) {  //finger change
                        input_mt_slot(input_dev, i);
                        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
                        tn_keyboard_client->pre_fingers = fingers;
                        tn_keyboard_client->touch_down &= ~BIT(i);
                        tn_keyboard_client->touch_temp &= ~BIT(i);
                        kb_debug("%s %d auto report finger up!change id:%d finger change:%d -> %d touch_down:%d temp_touch_down:%d\n",__func__,__LINE__,i,fingers,tn_keyboard_client->pre_fingers,tn_keyboard_client->touch_down,temp_touch_down);
                    }
                }
            }
            kb_debug("%s %d auto report finger up ok!touch_down:%d temp_touch_down:%d\n",__func__,__LINE__,tn_keyboard_client->touch_down,temp_touch_down);
        }
    }
    tn_keyboard_client->pre_fingers = fingers;
    
    temp.is_left =  data[1] & 0x01;//(data[0] & 0x04) >> 2;
    temp.is_right =  (data[1]>>1) & 0x01;//(data[1] & 0x08) >> 3;
    temp.area = 0x09;
    for(j = 0; j < fingers; j++){
        offset = 2 + 5 * j;        
        temp.id = data[offset + 0]>>4 & 0xf;
        temp.is_down = data[offset+ 0] & 0x01;     
        temp.x = data[offset+ 1] | data[offset+ 2]<<8;
        temp.y = data[offset+ 3] | data[offset+ 4]<<8;
        //temp.x = temp.x * TOUCH_X_MAX / 2560;
        //temp.y = temp.y * TOUCH_Y_MAX / 1536;

        kb_debug("%s %d id:%d,down:%d,left:%d,right:%d,x:%d,y:%d,area:%d offset:%d fingers:%d\n",
        __func__,__LINE__,temp.id,temp.is_down,temp.is_left,temp.is_right,temp.x,temp.y,temp.area,offset,fingers);
        input_mt_slot(input_dev, temp.id);  
        if(temp.is_down){              
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);   
            tn_keyboard_client->touch_down |= BIT(temp.id);
            tn_keyboard_client->touch_temp |= BIT(temp.id);
            input_report_abs(input_dev, ABS_MT_POSITION_X, temp.x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, temp.y);        ; 
            //input_report_key(input_dev, BTN_TOUCH, 1);
            //tn_keyboard_input_power_key_report();
            kb_debug("%s %d id:%d finger down\n",__func__,__LINE__,temp.id);
        }else{
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);           
            tn_keyboard_client->touch_down &= ~BIT(temp.id);
            if(unlikely(tn_keyboard_client->touch_down ^ tn_keyboard_client->touch_temp)){
                for(i = 0; i < TOUCH_FINGER_MAX; i++){
                    if (BIT(i) & (tn_keyboard_client->touch_down ^ tn_keyboard_client->touch_temp)) {  //finger change
                        input_mt_slot(input_dev, i);
                        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
                        tn_keyboard_client->pre_fingers -=1;
                        kb_debug("%s %d check finger up!change id:%d finger change:%d -> %d touch_down:%d touch_temp:%d\n",__func__,__LINE__,i,fingers,tn_keyboard_client->pre_fingers,tn_keyboard_client->touch_down,tn_keyboard_client->touch_temp);
                    }
                }
            }
            tn_keyboard_client->touch_temp &= ~BIT(temp.id);

            kb_debug("%s %d finger up!id:%d touch_down:%d touch_temp:%d\n",__func__,__LINE__,temp.id,tn_keyboard_client->touch_down,tn_keyboard_client->touch_temp);      
            if(!tn_keyboard_client->touch_down){    //finger all up
                //input_report_key(input_dev, BTN_TOUCH, 0); 
                tn_keyboard_client->touch_temp = 0;    
                tn_keyboard_client->touch_down = 0;
                tn_keyboard_client->pre_fingers = 0;
                kb_debug("%s %d finger all up!clear pre_fingers:%d\n",__func__,__LINE__,tn_keyboard_client->pre_fingers);
            }

        }
        if(j == fingers - 1){
            if(temp.is_left){
                event->is_left = temp.is_left;
                input_report_key(input_dev, BTN_MOUSE, temp.is_left);
                kb_debug("%s %d report touch left BTN_MOUSE down!\n",__func__,__LINE__);
            }
            if(temp.is_right){
                event->is_right = temp.is_right;
                input_report_key(input_dev, BTN_RIGHT, temp.is_right);
                kb_debug("%s %d report touch right BTN_RIGHT down!\n",__func__,__LINE__);
            }
            if((tn_keyboard_client->tn_keyboard_status & KEYBOARD_POWER_ON_STATUS) == 0 && 
				event->is_left == 0 && event->is_right == 0 && 
					tn_keyboard_client->tp_down_loss_timer_enable == 0){
				kb_debug("%s %d tn_keyboard_start_tp_down_loss_check_timer!\n",__func__,__LINE__);
				tn_keyboard_start_tp_down_loss_check_timer(1);
			}
            if(event->is_left && temp.is_left == 0){
                event->is_left = temp.is_left;
                input_report_key(input_dev, BTN_MOUSE, temp.is_left);
                kb_debug("%s %d report touch left BTN_MOUSE up!\n",__func__,__LINE__);
            }
            if(event->is_right && temp.is_right == 0){
                event->is_right = temp.is_right;
                input_report_key(input_dev, BTN_RIGHT, temp.is_right);
                kb_debug("%s %d report touch right BTN_RIGHT up!\n",__func__,__LINE__);
            }
        }
        input_mt_sync_frame(input_dev);
    }
    input_sync(input_dev);
    kb_debug("%s %d handler once touch data ok!\n",__func__,__LINE__);
    return 0;
}
int touchpad_input_init(void)
{
    int ret = 0;
    struct input_dev *input_dev = input_allocate_device();
    if(!input_dev){
        kb_err("%s %d input_allocate_device err\n",__func__,__LINE__);
        return -ENOMEM;
    }
    input_dev->name = TOUCHPAD_NAME;
    input_dev->id.bustype = BUS_HOST;
    input_dev->id.product = PRODUCT_ID;
    input_dev->id.vendor = VERDOR_ID;
    input_dev->id.version = 0x0010;

    set_bit(EV_ABS, input_dev->evbit);
    set_bit(EV_KEY, input_dev->evbit);
    set_bit(ABS_X, input_dev->absbit);
    set_bit(ABS_Y, input_dev->absbit);    
    //set_bit(ABS_PRESSURE, input_dev->absbit);
    set_bit(BTN_TOUCH, input_dev->keybit);

    
    set_bit(ABS_MT_TRACKING_ID, input_dev->absbit);
    //set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
    //set_bit(ABS_MT_TOUCH_MINOR, input_dev->absbit);
    set_bit(ABS_MT_POSITION_X, input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y, input_dev->absbit);

    set_bit(BTN_MOUSE, input_dev->keybit);
    set_bit(BTN_RIGHT, input_dev->keybit);
    
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, TOUCH_X_MAX, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, TOUCH_Y_MAX, 0, 0);
    //input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 100, 0, 0);
    //input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, 100, 0, 0);
    input_set_abs_params(input_dev, ABS_X, 0, TOUCH_X_MAX, 0, 0);
    input_set_abs_params(input_dev, ABS_Y, 0, TOUCH_Y_MAX, 0, 0);
    input_abs_set_res(input_dev, ABS_X, TOUCH_X_MAX);
    input_abs_set_res(input_dev, ABS_Y, TOUCH_Y_MAX);
    //input_set_abs_params(input_dev, ABS_PRESSURE, 0, 255, 0, 0);
    //input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 65535, 0, 0, 0);//input_mt_init_slots已包含


    set_bit(INPUT_PROP_BUTTONPAD, input_dev->propbit);
    input_mt_init_slots(input_dev, 0x04,INPUT_MT_POINTER);


    ret = input_register_device(input_dev);
    if(ret){
        input_free_device(input_dev);
        kb_err("%s %d input_register_device err\n",__func__,__LINE__);
        return ret;
    }
    tn_keyboard_client->input_touchpad = input_dev;
    kb_debug("%s %d ok\n",__func__,__LINE__);
    return 0;
}
