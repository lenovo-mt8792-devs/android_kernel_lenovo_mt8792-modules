#
# Makefile for the tn tn_keyboard device drivers.
#
#
#obj-$(CONFIG_TN_KEYBOARD)		+= tn_keyboard.o touchpad.o tn_keyboard.o one_wire_bus_phy.o
ccflags-y += -Wno-excess-initializers -Wno-format-extra-args -Wno-format -Wno-unused-variable -Wno-misleading-indentation
obj-$(CONFIG_TN_KEYBOARD)		+= tn_keyboard.o
tn_keyboard-objs := tn_keyboard_core.o
tn_keyboard-objs += keyboard.o touchpad.o one_wire_bus_phy.o tn_tty_io.o
