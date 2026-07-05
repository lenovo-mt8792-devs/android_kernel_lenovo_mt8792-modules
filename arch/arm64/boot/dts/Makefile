# SPDX-License-Identifier: GPL-2.0

# The DTBs are built in a seperate build step with dtstree
# overridden to this directory. The top level Makefile that
# sets DEVICE_MODULES_PATH is therefore not included, so
# we need to figure it out ourselves.
ifeq ($(DEVICE_MODULES_PATH),)
export DEVICE_MODULES_PATH := $(srctree)/$(dtstree)/../../../..
endif

subdir-y += mediatek
