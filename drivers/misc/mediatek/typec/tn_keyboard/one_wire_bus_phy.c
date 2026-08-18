/* ========================================
 *
 * Copyright Nano IC, 2021-02-25
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF Nano IC.
 *
 * ========================================
*/

//#include "ht32.h"
//#include "one_wire_bus_ext.h"
#include "one_wire_bus_phy.h"


const static unsigned  char  uart_map_table[8][16] = 
{
    {0xE8,0x39,0x10,0xBD,0x3A,0xA5,0xC0,0xA9,0xD3,0x35,0xE0,0xEB,0xB0,0x19,0xED,0x1F},
    {0xD1,0xD5,0xE6,0xBC,0xD4,0xCC,0xF9,0xCE,0xD0,0xFB,0xE5,0xA1,0xB1,0xFD,0x13,0x1E},
    {0xC6,0xCA,0xA4,0x11,0xF6,0xBF,0xEE,0xDC,0xD2,0xDA,0xF3,0xA2,0xCB,0x1C,0xC8,0x18},
    {0x33,0x3F,0xE3,0xAB,0x38,0xBB,0xC3,0xD8,0xD6,0x12,0xB3,0x30,0xE2,0x32,0xA0,0x1D},
    {0xDE,0xAC,0xE9,0xAE,0xF1,0xBA,0xC4,0xE7,0xCF,0xEC,0xEA,0xB4,0xF4,0xFE,0x14,0xB2},
    {0x3D,0x3B,0xDD,0xB7,0xEF,0xB8,0xC5,0xCD,0xAA,0xDF,0xBE,0x37,0xF5,0xF8,0xAF,0x3C},
    {0x34,0x3E,0xA7,0xAD,0xB5,0x16,0x1A,0xFC,0xE4,0xB6,0xFF,0xC2,0x36,0xD7,0xC9,0x1B},
    {0x17,0xF0,0xFA,0xA8,0xA3,0xB9,0xC7,0xF2,0x31,0xD9,0xE1,0x15,0xF7,0xDB,0xA6,0xC1},
};


/**
 * @brief :单总线物理层协议初始化
 * 
 * @param 
 * @return true : 成功
 * @return false : 失败
 */
bool one_wire_bus_phy_init(void)
{
    return true;
}

/**
 * @brief :对一段数据根据序号进行转码。转码后的数据长度是转码前两倍，要注意缓冲溢出
 * 
 * @param serial :转码序号， 0 ~ 7
 * @param p_in :需要转码的数据缓冲
 * @param p_out :转换后的输出数据缓冲
 * @param in_len :转码前输入数据长度
 * @return true :成功
 * @return false :失败
 */
bool  one_wire_bus_data_transcode(unsigned char serial,unsigned char *p_in,unsigned char *p_out,unsigned short in_len)
{
    unsigned short i;
    unsigned char  value;

    if(serial < 8)
    {
        for(i=0;i<in_len;i++)
        {
            value = p_in[i];
            if(ONE_WIRE_BUS_PACKET_START_FLAG==OWB_LX_PACKET_START_FLAG)
            {
                p_out[i*2] = uart_map_table[serial][0x0f - ((value >> 4)&0x0f)];
                p_out[i*2+1] = uart_map_table[serial][0x0f - (value&0x0f)]; 
            }
            else
            {
                p_out[i*2] = uart_map_table[serial][(value >> 4)&0x0f];
                p_out[i*2+1] = uart_map_table[serial][value&0x0f];
            }
            
        }
        return true;
    }else{
        return false;
    }
}

/**
 * @brief :根据转码数据搜索对应的转码序号
 * 
 * @param val :转码数据
 * @return  搜索到的序号
 */
unsigned char one_wire_bus_search_serial_num(unsigned char val)
{
    unsigned char i,j,ser;

    ser = 0xff;
    for(i=0;i<8;i++)
    {
        for(j=0;j<16;j++)
        {
            if(uart_map_table[i][j] == val)
            {
                ser = i;
                return ser;
            }
        }
    }
    return ser;
}

/**
 * @brief 计算一段数据的累加和
 * 
 * @param p_buf 输入数据缓冲
 * @param len 输入数据长度
 * @return unsigned char 累加和低8 Bit
 */
unsigned char one_wire_bus_get_buf_sum(unsigned char * p_buf,unsigned char len)
{
    unsigned int sum;
	unsigned char k = 0;
    sum = 0;
    for(k=0;k<len;k++)
    {
        sum += p_buf[k];
    }
    return (sum & 0xff);
}

/**
 * @brief 根据两字节的转码的数据，解码出一个字节原始数据
 * 
 * @param val_h 需要解码的，转码数据高字节
 * @param val_l 需要解码的，转码数据低字节
 * @param dec_ser 解码用的系号
 * @param p_out 解码后的数据输出存储指针
 * @return true 成功
 * @return false 失败
 */
bool one_wire_bus_decode_one_byte(unsigned char val_h,unsigned char val_l,unsigned char dec_ser,unsigned char * p_out)
{
    unsigned char i,val,status;

    if(dec_ser > 7)
    {
        return false;
    }

    status = 0x00;
    val = 0x00;
    for(i=0;i<16;i++)
    {
        if(uart_map_table[dec_ser][i] == val_h)
        {
            val = ((ONE_WIRE_BUS_PACKET_START_FLAG==OWB_LX_PACKET_START_FLAG)?15-i:i);
            status = 0x01;
        }
    }
    if(status == 0x01)
    {
        for(i=0;i<16;i++)
        {
            if(uart_map_table[dec_ser][i] == val_l)
            {
                val <<= 4;
                val |= ((ONE_WIRE_BUS_PACKET_START_FLAG==OWB_LX_PACKET_START_FLAG)?15-i:i);
                status |= 0x02;
            }
        }
    }
    if(status == 0x03)
    {
        *p_out = val;
        return true;
    }else{
        return false; 
    }
}

/**
 * @brief 对一段转码后的数进行解码。解码后的数据长度是解码前的二分之一
 * 
 * @param p_in 需要解码的数据缓冲
 * @param p_out 解码后的输出数据缓冲
 * @param in_len 解码前输入数据长度
 * @param dec_ser 解码用的系号
 * @return true 成功
 * @return false失败
 */
bool one_wire_bus_decode_frame(unsigned char *p_in,unsigned char *p_out,unsigned short in_len,unsigned char dec_ser)
{
    unsigned short i;
    unsigned char out_data;
    bool f_dec,f_byte;

    f_dec = true;
    out_data = 0;
    if(dec_ser < 8)
    {
        for(i=0;i<in_len;i++)
        {
            f_byte = one_wire_bus_decode_one_byte(p_in[i*2],p_in[i*2+1],dec_ser,&out_data);
            if(f_byte == true)
            {
                p_out[i] = out_data;
            }else{  // 解码出错
                f_dec = false;
                break;
            }
        }
    }else{
        f_dec = false;
    }
    return f_dec;
}

/**
 * @brief 根据输入命令和数据，打包成满足单总线协议要求的数据包
 * 
 * @param cmd 命令字
 * @param p_data 输入数据指存
 * @param len 输入数据长度
 * @param p_packet_buf 输出数据指存
 * @param packet_max_len 输出数据缓冲最大长度
 * @return unsigned short 实际转换出来的长度
 */
unsigned short one_wire_bus_phy_packet(unsigned char cmd,
                                unsigned char *p_data,
                                unsigned char len, 
                                unsigned char *p_packet_buf,
                                unsigned short packet_max_len
                                )
{
    unsigned char sum,serial,temp;
    unsigned short i,need_len;

    need_len = 19 + (unsigned short)len*2;
    if(packet_max_len < need_len)
    {
        return 0;
    }      
    get_random_bytes(&temp, sizeof(temp));
    serial = ((temp >> 4) ^ (temp & 0x0f));
    serial &= 0x07;
    for (i = 0; i < 8; i++)
    {
        p_packet_buf[i] = 0x55;
    } 
    //数据包头
    p_packet_buf[8] = ONE_WIRE_BUS_PACKET_START_FLAG;
    //命令字
    p_packet_buf[9] = cmd;

    one_wire_bus_data_transcode(serial, &len, &p_packet_buf[10], 1);
    one_wire_bus_data_transcode(serial, p_data, &p_packet_buf[12], len);

    sum = one_wire_bus_get_buf_sum(&p_packet_buf[8], (len*2 + 4));
    one_wire_bus_data_transcode(serial, &sum, &p_packet_buf[len*2 + 12], 1);

    //数据包尾
    p_packet_buf[len*2 + 14] = ONE_WIRE_BUS_PACKET_END_FLAG;

    for (i = 1; i < 5; i++)
    {
        p_packet_buf[len*2 + 14 + i] = 0x55;
    }
    return need_len;
}

/**
 * @brief 获取随机序号
 * 
 * @param 
 * @return unsigned char 实际返回的随机序号
 */
unsigned char one_wire_bus_gen_random_serial(void){
    static unsigned char s_count = 0;
    unsigned char rand_num = 0;
    get_random_bytes(&rand_num, sizeof(rand_num));
    s_count++;   
    rand_num = (rand_num & s_count) % 8;
    return rand_num;
}