/*
 * Copyright (c) 2018 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_ble.h"
#include "gui.h"
#include <device.h>
#include <devicetree.h>
#include <drivers/display.h>
#include <drivers/gpio.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <sys/printk.h>
#include <zephyr.h>



;

void main(void) {

  gui_config_t gui_config = {};
  gui_init(&gui_config);
}