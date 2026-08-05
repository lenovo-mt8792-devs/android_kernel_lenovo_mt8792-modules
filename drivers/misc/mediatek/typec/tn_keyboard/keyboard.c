#include"tn_keyboard.h"


#define KEYBOARD_NUM_KEYS   256
#define KEYBOARD_MM_NUM_KEYS 16
#define unk	KEY_UNKNOWN



static const unsigned char tn_keyboard[KEYBOARD_NUM_KEYS] = {
      0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
     50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
      4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
     27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
     65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
    105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
     72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
    191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
    115,114,unk,unk,unk,121,unk, 89, 93,124, 92, 94, 95,unk,unk,unk,
    122,123, 90, 91, 85,unk,unk,unk,unk,unk,unk,unk,111,unk,unk,unk,
    unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
    unk,unk,unk,unk,unk,unk,179,180,unk,unk,unk,unk,unk,unk,unk,unk,
    unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,unk,
    unk,unk,unk,unk,unk,unk,unk,unk,111,unk,unk,unk,unk,unk,unk,unk,
     29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
    150,158,159,128,136,177,178,176,142,152,173,140,unk,unk,unk,unk
};



/*0x224,0xe2,0xea,0xe9,0x391,0x70,0x6f,0x392,0x394,0x395,0x397,0x38e,0x398,0x399,0x390*/
static const unsigned short mm_tn_keyboard[KEYBOARD_MM_NUM_KEYS][2] = {
    {0x70,   KEY_BRIGHTNESSDOWN},
    {0x6f,   KEY_BRIGHTNESSUP},
    {0xe2,   KEY_MUTE},
    {0xea,   KEY_VOLUMEDOWN},
    {0xe9,   KEY_VOLUMEUP},
    {0x224,   KEY_BACK},
    {0x38e,   KEY_LOCKSCREEN},
    {0x390,   KEY_SWITCHLANUAGE}, 
    {0x391,   KEY_MICDISABLE},
    {0x392,   KEY_TOUCHPANELMUTE},
    {0x393,   KEY_GLOBALSEARCH},    
    {0x394,   KEY_FULLSCREEN},
    {0x395,   KEY_SPLITSCREEN},
    {0x397,   KEY_SUPERINTCON}, 
    {0x398,   KEY_CUSTOMERAPP1}, 
    {0x399,   KEY_CUSTOMERAPP2},   
    {unk,   unk},    
};


int tn_keyboard_input_wakeup_init(void){
	int ret = 0;
    struct input_dev *input_dev  = input_allocate_device();
    
    if(!input_dev){
        kb_err("%s %d input_allocate_device err \n",__func__,__LINE__);
        return -ENOMEM;
    }
    input_dev->name = WAKEUP_NAME;
    input_dev->id.bustype = BUS_HOST;
    input_dev->id.vendor = VERDOR_ID;
    input_dev->id.product = PRODUCT_ID;
    input_dev->id.version = 0x0010;
    __set_bit(EV_KEY, input_dev->evbit);
   
    __set_bit(KEY_POWER, input_dev->keybit);
    __set_bit(KEY_KB_ENABLE, input_dev->keybit);
    __set_bit(KEY_KB_DISABLE, input_dev->keybit);
    ret = input_register_device(input_dev);
    if(ret){
        input_free_device(input_dev);
        kb_err("%s %d input_register_device err \n",__func__,__LINE__);
        return ret;
    }
    tn_keyboard_client->input_wakeup = input_dev;

    kb_debug("%s %d ok \n",__func__,__LINE__);
    return 0;
}

static int tn_keyboard_event_hander(struct input_dev *dev,
			unsigned int type, unsigned int code, int value)
{	
    int ret = 0;  
    switch(type){
        case EV_LED:
            tn_keyboard_led_process(code, value);
            break;
        case EV_MSC:
            break;
        default:
            kb_err("%s %d no type  err \n",__func__,__LINE__);
            ret = -1;
    }   
    return ret;
}

int tn_keyboard_input_init(void)
{
    int i = 0, ret = 0;
    struct input_dev *tn_keyboard_input  = input_allocate_device();
    
    if(!tn_keyboard_input){
        kb_err("%s %d input_allocate_device err \n",__func__,__LINE__);
        return -ENOMEM;
    }
    tn_keyboard_input->name = KEYBOARD_NAME;
    tn_keyboard_input->id.bustype = BUS_HOST;
    tn_keyboard_input->id.vendor = VERDOR_ID;
    tn_keyboard_input->id.product = PRODUCT_ID;
    tn_keyboard_input->id.version = 0x0010;
    __set_bit(EV_KEY, tn_keyboard_input->evbit);
    __set_bit(EV_MSC, tn_keyboard_input->evbit);
    __set_bit(MSC_SCAN, tn_keyboard_input->mscbit);

    __set_bit(EV_LED, tn_keyboard_input->evbit);
    __set_bit(LED_CAPSL, tn_keyboard_input->ledbit);
    //__set_bit(LED_MUTE, tn_keyboard_input->ledbit);
    //__set_bit(LED_MIC_MUTE, tn_keyboard_input->ledbit);
     tn_keyboard_input->event = tn_keyboard_event_hander;
    for(i = 0; i < KEYBOARD_NUM_KEYS; i++){
        if(tn_keyboard[i] != 0 && tn_keyboard[i] != unk){
            __set_bit(tn_keyboard[i], tn_keyboard_input->keybit);
        }
    }
    /*input mm key set*/
    for(i = 0; i < KEYBOARD_MM_NUM_KEYS; i++){
        if(mm_tn_keyboard[i][1] != 0 && mm_tn_keyboard[i][1] != unk){
            __set_bit(mm_tn_keyboard[i][1], tn_keyboard_input->keybit);
            __set_bit(mm_tn_keyboard[i][0], tn_keyboard_input->mscbit);
        }
    }
    //__set_bit(KEY_POWER, tn_keyboard_input->keybit);
    
    ret = input_register_device(tn_keyboard_input);
    if(ret){
        input_free_device(tn_keyboard_input);
        kb_err("%s %d input_register_device err \n",__func__,__LINE__);
        return ret;
    }
    tn_keyboard_client->input_tn_keyboard = tn_keyboard_input;

    
    kb_debug("%s %d ok \n",__func__,__LINE__);
    return 0;
    
}


int tn_keyboard_input_power_key_report(void){
    struct tn_keyboard_data *kbd = tn_keyboard_client;
    struct input_dev  *input_dev = kbd->input_wakeup;
    if(!input_dev)
        return -EINVAL;
    if((kbd->tn_keyboard_status & KEYBOARD_POWER_ON_STATUS) == 0){    
        tn_keyboard_client->tn_keyboard_status |= KEYBOARD_POWER_ON_STATUS;
        input_report_key(input_dev,KEY_POWER, 1);
        input_sync(input_dev);
        mdelay(15);
        input_report_key(input_dev,KEY_POWER, 0);
        input_sync(input_dev);
        kb_info("%s %d  \n",__func__,__LINE__);
    }
    return 0;
}

int tn_keyboard_input_report(char *buf){
    int i=  0;    
    struct tn_keyboard_data *kbd = tn_keyboard_client;
    struct input_dev  *input_dev = kbd->input_tn_keyboard;
    if(!buf) 
        return -EINVAL;   
    if(!input_dev)
        return -EINVAL;
    //sprintf(TAG,"%s  %d",__func__,__LINE__); 
   // tn_keyboard_show_buf(tn_keyboard_client->new,8);
    memcpy(tn_keyboard_client->new,buf,8);
   
    for (i = 0; i < 8; i++){
		input_report_key(input_dev, tn_keyboard[i + 224], (kbd->new[1] >> i) & 1); 
        //tn_keyboard_input_power_key_report();
    }

	for (i = 2; i < 8; i++) {

		if (kbd->old[i] > 3 && memscan(kbd->new + 2, kbd->old[i], 6) == kbd->new + 8) {
			if (tn_keyboard[kbd->old[i]]){
				input_report_key(input_dev, tn_keyboard[kbd->old[i]], 0);				
				//tn_keyboard_led_report(tn_keyboard[kbd->old[i]]);
                kb_debug("%s %d key int:%d hex:0x%x \n",__func__,__LINE__,tn_keyboard[kbd->old[i]],tn_keyboard[kbd->old[i]]);
            }
			else
				kb_debug("Unknown key (scancode %#x) released.\n",
					 kbd->old[i]);
		}

		if (kbd->new[i] > 3 && memscan(kbd->old + 2, kbd->new[i], 6) == kbd->old + 8) {
			if (tn_keyboard[kbd->new[i]]){
				input_report_key(input_dev, tn_keyboard[kbd->new[i]], 1);
				//tn_keyboard_input_power_key_report();

                kb_debug("%s %d key int:%d hex:0x%x \n",__func__,__LINE__,tn_keyboard[kbd->new[i]],tn_keyboard[kbd->new[i]]);
            }
			else
				kb_debug("Unknown key (scancode %#x) pressed.\n",
					 kbd->new[i]);
		}
	}

	input_sync(input_dev);
	memcpy(kbd->old, kbd->new, 8);		
    return 0;
    
}

void *memscan_ex(void *addr, int c, size_t size)
{
	unsigned char *p = addr;
    unsigned short key = 0; 
	while (size) {
	    key = *p | ((*(p + 1)) << 8);
		if (key == c)
			return (void *)p;
		p = p + 2;
		size = size - 2;
	}
  	return (void *)p;
}
int tn_keyboard_mm_input_report(char *buf){
    int i=  0, j = 0;  
    int keywords = 0;
    struct tn_keyboard_data *kbd = tn_keyboard_client;
    struct input_dev  *input_dev = kbd->input_tn_keyboard;
    if(!buf) 
        return -EINVAL; 
    if(!input_dev)
        return -EINVAL;
       
    //sprintf(TAG,"%s  %d",__func__,__LINE__);
    //tn_keyboard_show_buf(tn_keyboard_client->mm_old,4);
    //tn_keyboard_show_buf(tn_keyboard_client->mm_new,4);
    memcpy(tn_keyboard_client->mm_new,&buf[1],4); 
    //keywords = kbd->mm_new[2*i] | kbd->mm_new[2*i+1] << 8;
	//kb_debug("%s %d key:0x%x \n",__func__,__LINE__,keywords);

	
    for(i = 0; i < 2; i++){
        keywords = kbd->mm_old[2*i] | kbd->mm_old[2*i+1] << 8;
    	if ( memscan_ex(kbd->mm_new, keywords, 4) == kbd->mm_new + 4) {      	    
    		if (keywords){    		    
    		    for(j = 0; j < KEYBOARD_MM_NUM_KEYS; j++){
                    if(keywords == mm_tn_keyboard[j][0])
                        break;
    		    }
    		    kb_debug("%s %d i:%d j:%d %p!=%p keywords:0x%x\n",__func__,__LINE__,i,j,memscan_ex(kbd->mm_new, keywords, 4),kbd->mm_new + 4,keywords);
    		    if(j < KEYBOARD_MM_NUM_KEYS){
					input_event(input_dev, EV_MSC, MSC_SCAN, (0x0c << 16)|keywords);
					kb_debug("%s %d input_event MSC_SCAN:0x%x \n",__func__,__LINE__,(0x0c << 16)|keywords);
        			input_report_key(input_dev, mm_tn_keyboard[j][1], 0);        			
                    kb_debug("%s %d key:0x%x scancode:0x%x\n",__func__,__LINE__,keywords,mm_tn_keyboard[j][1]);
					//tn_keyboard_led_report(mm_tn_keyboard[j][1]);
                }else{
                    kb_debug("Unknown key (scancode %#x) %d released.\n",kbd->mm_old[0],__LINE__);
                }
            }
    		else
    			kb_debug("Unknown key (scancode %#x) %d released.\n",kbd->mm_old[0],__LINE__);
    	}
        keywords = kbd->mm_new[2*i] | kbd->mm_new[2*i+1] << 8;
    	if (memscan_ex(kbd->mm_old, keywords, 4) == kbd->mm_old + 4) {     	    
    	    if (keywords){
    		    for(j = 0; j < KEYBOARD_MM_NUM_KEYS; j++){
                    if(keywords == mm_tn_keyboard[j][0])
                        break;
    		    }
    		    kb_debug("%s %d i:%d j:%d %p!=%p keywords:0x%x\n",__func__,__LINE__,i,j,memscan_ex(kbd->mm_old, keywords, 4),kbd->mm_new + 4,keywords);
    		    if(j < KEYBOARD_MM_NUM_KEYS){
					input_event(input_dev, EV_MSC, MSC_SCAN, (0x0c << 16)|keywords);
					kb_debug("%s %d input_event MSC_SCAN:0x%x \n",__func__,__LINE__,(0x0c << 16)|keywords);
                    input_report_key(input_dev, mm_tn_keyboard[j][1], 1);
					if(keywords != 0x38e){ //lock key
				        tn_keyboard_input_power_key_report();
				    }

                    kb_debug("%s %d key int:%x hex:0x%x\n",__func__,__LINE__,keywords,mm_tn_keyboard[j][1]);
                }else{
                    kb_debug("Unknown key (scancode %#x) %d  pressed.\n",kbd->mm_new[0],__LINE__);
                }
            }
    		else
    			kb_debug("Unknown key (scancode %#x) pressed.\n",kbd->mm_new[0]);
    	}
	}	

	input_sync(input_dev);
	memcpy(kbd->mm_old, kbd->mm_new, 4);	    
    return 0;
}

