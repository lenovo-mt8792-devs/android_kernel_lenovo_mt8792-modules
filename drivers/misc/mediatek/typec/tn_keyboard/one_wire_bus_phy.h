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
#ifndef __ONE_WIRE_BUS_PHY_H__
#define __ONE_WIRE_BUS_PHY_H__
//#include "ht32.h"
#include <linux/kernel.h>
#include <linux/random.h>
#include "one_wire_bus_def.h"

///**
// * @brief 一帧数据的启始标志
// */
#define OWB_XM_PACKET_START_FLAG 0x21 //小米头
#define OWB_LX_PACKET_START_FLAG 0x25 //联想头

/**
 * @brief 重传一帧数据的启始标志
 */
#define OWB_XM_PACKET_REPEAT_FLAG 0x22 //小米重传头
#define OWB_LX_PACKET_REPEAT_FLAG 0x26 //联想重传头

#define OWB_LX

//当为非调试模式不允许使用小米键盘
#ifdef OWB_LX
#define ONE_WIRE_BUS_PACKET_START_FLAG OWB_LX_PACKET_START_FLAG
#define ONE_WIRE_BUS_PACKET_REPEAT_FLAG OWB_LX_PACKET_REPEAT_FLAG
#else
#define ONE_WIRE_BUS_PACKET_START_FLAG OWB_LX_PACKET_START_FLAG
#define ONE_WIRE_BUS_PACKET_REPEAT_FLAG OWB_LX_PACKET_REPEAT_FLAG
#endif

/**
 * @brief 一帧数据的结束标志
 */
#define ONE_WIRE_BUS_PACKET_END_FLAG 0x2C

/**
 * @brief Command word of data upload
 * 
 */
#define ONE_WIRE_BUS_PACKET_CMD_START 0x60
#define ONE_WIRE_BUS_PACKET_CMD_MOUSE 0x60
#define ONE_WIRE_BUS_PACKET_CMD_KEYBOARD 0x61
#define ONE_WIRE_BUS_PACKET_CMD_MEDIA 0x62
#define ONE_WIRE_BUS_PACKET_CMD_TOUCHPAD 0x63
#define ONE_WIRE_BUS_PACKET_CMD_G_SENSOR 0x64
#define ONE_WIRE_BUS_PACKET_CMD_CUSTOM 0x69
#define ONE_WIRE_BUS_PACKET_CMD_END 0x69

#define ONE_WIRE_BUS_PACKET_CMD_CUSTOM_GSENSOR 0xA0
#define ONE_WIRE_BUS_PACKET_CMD_CUSTOM_HALL 0xA1
#define ONE_WIRE_BUS_PACKET_CMD_PEN_INSET 0xA2
#define ONE_WIRE_BUS_PACKET_CMD_PEN_HALL 0xA3
/**
 * @brief Command word of parameter settings
 * 
 */
#define ONE_WIRE_BUS_PACKET_CMD_SETPARA 0x6A
#define ONE_WIRE_BUS_PACKET_CMD_SETPARA_ACK 0x6B

/**
 * @brief Command word of status read
 * 
 */
#define ONE_WIRE_BUS_PACKET_CMD_STATUS_READ 0x6C
#define ONE_WIRE_BUS_PACKET_CMD_STATUS_RETURN 0x6D

/**
 * @brief Command word of encrypted authentication
 * 
 */
#define ONE_WIRE_BUS_PACKET_CMD_ENCRYPT 0x6E
#define ONE_WIRE_BUS_PACKET_CMD_ENCRYPT_ACK 0x6F

/**
 * @brief Command word of upgrade
 * 
 */
#define ONE_WIRE_BUS_PACKET_CMD_UPG_START 0x70
#define ONE_WIRE_BUS_PACKET_CMD_UPG_START_ACK 0x71
#define ONE_WIRE_BUS_PACKET_CMD_UPG_DATA 0x72
#define ONE_WIRE_BUS_PACKET_CMD_UPG_DATA_ACK 0x73
#define ONE_WIRE_BUS_PACKET_CMD_UPG_END 0x74
#define ONE_WIRE_BUS_PACKET_CMD_UPG_END_ACK 0x75

/**
 * @brief Command word of software reset
 * 
 */
#define ONE_WIRE_BUS_PACKET_CMD_S_RST 0x76
#define ONE_WIRE_BUS_PACKET_CMD_S_RST_ACK 0x77

/**
 * @brief Command word of lowpower
 * 
 */
#define ONE_WIRE_BUS_PACKET_CMD_LOWP 0x78
#define ONE_WIRE_BUS_PACKET_CMD_LOWP_ACK 0x79

/**
 * @brief Command word of syn
 * 
 */
#define ONE_WIRE_BUS_PACKET_CMD_SYN_DOWN 0x7A
#define ONE_WIRE_BUS_PACKET_CMD_SYN_UP 0x7B

/**
 * @brief debug sofware cmd
 * 
 */
#define ONE_WIRE_BUS_PACKET_CMD_DEBUG_SF 0x7C
#define ONE_WIRE_BUS_PACKET_CMD_DEBUG_SF_ACK 0x7D
#define ONE_WIRE_USB_CMD_RW_DEBUG_VALUE 0xF3
//-------------------------------------------------------------------------
// 读取键盘端调试信息子命令
// 读取键盘端的动行状态，主要是各变量值
#define ONE_WIRE_BUS_DEBUG_SUB_CMD_READ_KEYBOARD_VALUE 0x11
// 读取Keyboard硬件相关信息，主要是芯片寄存器相关
#define ONE_WIRE_BUS_DEBUG_SUB_CMD_READ_KEYBOARD_HW_INFO 0x13
// 读取Keyboard硬件相关信息，主要是字符串打印
#define ONE_WIRE_BUS_DEBUG_SUB_CMD_READ_KEYBOARD_STRING 0x15
// 复位Keyboard调试信息
#define USB_SUB_CMD_DEBUG_RESET_KEYBOARD_INFO 0x22

// 从RTX上获取Dongle信息，用于厂测
#define USB_SUB_CMD_DEBUG_RTX_GET_DONGLE_INFO 0x31
// WN8031F应答
#define USB_SUB_CMD_DEBUG_RTX_DONGLE_ACK_INFO 0x32
//-------------------------------------------------------------------------

/**
 * @brief Production test
 * 
 */
#define ONE_WIRE_BUS_PACKET_CMD_MANUFACTURE 0x7E
#define ONE_WIRE_BUS_PACKET_CMD_MANUFACTURE_ACK 0x7F
#define ONE_WIRE_USB_CMD_RW_MANUFACTURE_VALUE 0xF1

/**
 * @brief Command word of host read keyboard state
 * 
 */
#define ONE_WIRE_BUS_PACKET_EXT_CMD_READ_KEY_STATE 0x52

/**
 * @brief Command word of host write keyboard state
 * 
 */
#define ONE_WIRE_BUS_EXT_CMD_KEY_STATE_START 0x20
#define ONE_WIRE_BUS_EXT_CMD_WRITE_FIRM_RECOVER 0x20
#define ONE_WIRE_BUS_EXT_CMD_WRITE_TOUCH_ENABLE 0x21
#define ONE_WIRE_BUS_EXT_CMD_WRITE_KEY_ENABLE 0x22
#define ONE_WIRE_BUS_EXT_CMD_WRITE_BACK_LIGHT_EN 0x23
#define ONE_WIRE_BUS_EXT_CMD_WRITE_TOUCH_SENSITI 0x24
#define ONE_WIRE_BUS_EXT_CMD_WRITE_POWER_STATE 0x25
#define ONE_WIRE_BUS_EXT_CMD_WRITE_GSENSOR_EN 0x26
#define ONE_WIRE_BUS_EXT_CMD_WRITE_CAPS_LOCK 0x26
#define ONE_WIRE_BUS_EXT_CMD_WRITE_CAPS_LOCK_FTM 0x27
#define ONE_WIRE_BUS_EXT_CMD_READ_KEY_STATE 0x2F
#define ONE_WIRE_BUS_EXT_CMD_KEY_STATE_END 0x2F

/**
 * @brief Command word of host read keyboard ack
 * 
 */
#define ONE_WIRE_BUS_EXT_CMD_KEY_READ_STATE_ACK 0x30

/**
 * @brief Command word of host write encryption data
 * 
 */
#define ONE_WIRE_BUS_EXT_CMD_KEY_ENCRYPT_START 0x31
#define ONE_WIRE_BUS_EXT_CMD_KEY_MI_AUTH_START 0x31
#define ONE_WIRE_BUS_EXT_CMD_KEY_MI_STEP3 0x32
#define ONE_WIRE_BUS_EXT_CMD_KEY_MI_TYPE1 0x33
#define ONE_WIRE_BUS_EXT_CMD_KEY_MI_TYPE2 0x34
#define ONE_WIRE_BUS_EXT_CMD_KEY_MI_STEP7 0x35
#define ONE_WIRE_BUS_EXT_CMD_KEY_ENCRYPT_END 0x35

/**
 * @brief WN8031收到来自PAD MCU的命令后立即作出USB响应
 * 
 */
#define ONE_WIRE_BUS_EXT_CMD_WN8031_ACK_USB 0xF0

/**
 * @brief PAD MCU set keyboard led status
 * 
 */
#define ONE_WIRE_BUS_PACKET_CMD_SET_LED_STATUS 0xA3

/**
 * @brief :单总线物理层协议初始化
 * 
 * @param 
 * @return true : 成功
 * @return false : 失败
 */
bool one_wire_bus_phy_init(void);

/**
 * @brief :对一段数据根据序号进行转码。转码后的数据长度是转码前两倍，要注意缓冲溢出
 * 
 * @param serial :转码序号， 0 ~ 7
 * @param p_in :需要转码的数据缓冲
 * @param p_out :转换后的输出数据缓冲
 * @param in_len :转码前输入数据长度
 * @return true :成功
 * @return false ::失败
 */
bool one_wire_bus_data_transcode(unsigned char serial, unsigned char *p_in,
				 unsigned char *p_out, unsigned short in_len);

/**
 * @brief :根据转码数据搜索对应的转码序号
 * 
 * @param val :转码数据
 * @return  搜索到的序号
 */
unsigned char one_wire_bus_search_serial_num(unsigned char val);

/**
 * @brief 计算一段数据的累加和
 * 
 * @param p_buf 输入数据缓冲
 * @param len 输入数据长度
 * @return unsigned char 累加和低8 Bit
 */
unsigned char one_wire_bus_get_buf_sum(unsigned char *p_buf, unsigned char len);

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
bool one_wire_bus_decode_one_byte(unsigned char val_h, unsigned char val_l,
				  unsigned char dec_ser, unsigned char *p_out);

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
bool one_wire_bus_decode_frame(unsigned char *p_in, unsigned char *p_out,
			       unsigned short in_len, unsigned char dec_ser);

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
unsigned short one_wire_bus_phy_packet(unsigned char cmd, unsigned char *p_data,
				       unsigned char len,
				       unsigned char *p_packet_buf,
				       unsigned short packet_max_len);

/**
 * @brief 获取随机序号
 * 
 * @param 
 * @return unsigned char 实际返回的随机序号
 */
unsigned char one_wire_bus_gen_random_serial(void);
#endif

/* [] END OF FILE */
