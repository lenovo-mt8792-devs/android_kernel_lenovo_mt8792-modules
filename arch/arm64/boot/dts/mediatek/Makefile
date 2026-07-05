# SPDX-License-Identifier: GPL-2.0

# for the include dt-bindings header files in the kernel_device_modules-X.Y
# common, for include/dt-bindings
DTC_INCLUDE += $(DEVICE_MODULES_PATH)/include
# arm64
DTC_INCLUDE += $(DEVICE_MODULES_PATH)/arch/$(SRCARCH)/boot/dts $(DEVICE_MODULES_PATH)/arch/$(SRCARCH)/boot/dts/mediatek

# for cust.dtsi
#DTC_INCLUDE += $(DEVICE_MODULES_REL_PATH)/arch/$(SRCARCH)/boot/dts
DTC_INCLUDE += $(dir $(src))

# for devicetree overlay
DTC_FLAGS += -@

dtb-$(CONFIG_ARCH_MEDIATEK) += mt6897.dtb

# OSC: 临时添加项目 DTB
# TODO: build P98300AA1.dtbo, but missing
# cust.dtsi for tb8792p1_64 project.
dtb-$(CONFIG_ARCH_MEDIATEK) += \
	P98300DA1.dtbo \
	P98300DA2.dtbo \
	P98300GA1.dtbo \
	P98300JA1.dtbo
