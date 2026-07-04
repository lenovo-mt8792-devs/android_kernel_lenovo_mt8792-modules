#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# Connectivity combo driver
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    subdir-ccflags-y += -I$(srctree)/
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/clkbuf/src
    subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
    subdir-ccflags-y += -I$(srctree)/drivers/mmc/core
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/pinctrl/mediatek/
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power_throttling/

    # Do Nothing, move to standalone repo
    MODULE_NAME := connadp
    obj-$(CONFIG_MTK_COMBO) += $(MODULE_NAME).o

    $(MODULE_NAME)-objs += common/connectivity_build_in_adapter.o
    $(MODULE_NAME)-objs += common/wmt_build_in_adapter.o
    $(MODULE_NAME)-objs += power_throttling/adapter.o
    $(MODULE_NAME)-objs += power_throttling/core.o
    ifeq ($(CONFIG_CONN_PWR_DEBUG),y)
        $(MODULE_NAME)-objs += power_throttling/test.o
    endif

    # KO specfied attributes
    export WLAN_CHIP_ID := 6897
    export BT_PLATFORM := 6897
    export MTK_COMBO_CHIP := CONNAC2X2_SOC7_0
    export WIFI_IP_SET := 1
    export CONNAC_VER := 2_0
    export CONFIG_MTK_COMBO_WIFI_HIF := axi
    export MTK_ANDROID_WMT := y
    export MTK_ANDROID_EMI := y

    obj-y += bt/mt66xx/6897/
    obj-y += connfem/
    obj-y += conninfra/
    obj-y += gps/data_link/
    obj-y += wlan/adaptor/build/connac2x/
    obj-y += wlan/core/gen4m/
else
# Otherwise we were called directly from the command line;
# invoke the kernel build system.
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
