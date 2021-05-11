#include "gui.h"
#include "app_ble.h"
#include "gui.h"
#include <device.h>
#include <devicetree.h>
#include <drivers/display.h>
#include <drivers/gpio.h>
#include <inttypes.h>
#include <logging/log.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <sys_clock.h>
#include <zephyr.h>

#define WHEEL_PERIMETER 2325 //in mm

/* defining leds */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

#define LED0 DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN0 DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS0 DT_GPIO_FLAGS(LED0_NODE, gpios)

#define LED1 DT_GPIO_LABEL(LED1_NODE, gpios)
#define PIN1 DT_GPIO_PIN(LED1_NODE, gpios)
#define FLAGS1 DT_GPIO_FLAGS(LED1_NODE, gpios)

#define LED2 DT_GPIO_LABEL(LED2_NODE, gpios)
#define PIN2 DT_GPIO_PIN(LED2_NODE, gpios)
#define FLAGS2 DT_GPIO_FLAGS(LED2_NODE, gpios)

#define LED3 DT_GPIO_LABEL(LED3_NODE, gpios)
#define PIN3 DT_GPIO_PIN(LED3_NODE, gpios)
#define FLAGS3 DT_GPIO_FLAGS(LED3_NODE, gpios)
#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL

/* deffining the input for the reed switch*/
#define SW0_NODE DT_ALIAS(sw0)

#define SW0_GPIO_LABEL DT_GPIO_LABEL(SW0_NODE, gpios)
#define SW0_GPIO_PIN DT_GPIO_PIN(SW0_NODE, gpios)
#define SW0_GPIO_FLAGS (GPIO_INPUT | DT_GPIO_FLAGS(SW0_NODE, gpios))

LOG_MODULE_REGISTER(gui);

char count[5];
int counter = 0, speed_now = -1;
char speed[15], timer[20], minc[5], secc[5], msecc[5], distancec[10], foo[10], traveled_distancec[40], metersc[20], kmetersc[20],lap_msecc[5],lap_secc[5],lap_minc[5],lap[20];
int min = 0, sec = 0, msec = 0,lap_msec,lap_sec,lap_min;
int i = 0, ret;
int traveled_distance = 0 /* i would like to save this but idk*/, meters = 0, kmeters = 0;
bool run_timmer = false, reset_timmer = false;

char count_str[11] = {0};
const struct device *button;
const struct device *display_dev;

static gui_event_t m_gui_event;
static gui_callback_t m_gui_callback = 0;

// Create a message queue for handling external GUI commands
K_MSGQ_DEFINE(m_gui_cmd_queue, sizeof(gui_message_t), 8, 4);

// Define a timer to update the GUI periodically

char *on_off_strings[2] = {"On", "Off"};
char *bluetooth_statuses[6] = {"--","Ititialised","Connected","Sending data","Reciving data","Disconected"};

// GUI objects
lv_obj_t *background;
lv_obj_t *data_text;
lv_obj_t *data;
lv_obj_t *lap_text;
lv_obj_t *lap_data;
lv_obj_t *distance_text;
lv_obj_t *distance_container;
lv_obj_t *bt_status_text,*bt_status;
lv_obj_t *timer_block;
lv_obj_t *text_timer_block;
lv_obj_t *speed_block;
lv_obj_t *top_header;
lv_obj_t *current_speed_block;
lv_obj_t *label_button, *label_led, *label_bt_state_hdr, *label_bt_state;
lv_obj_t *btn1, *btn1_label, *btn2, *btn2_label, *btn3, *btn3_label, *btn4, *btn4_label;

// Styles

lv_style_t style_btn, style_label, style_label_value, style_checkbox;
lv_style_t style_header, style_con_bg;
lv_style_t style_bkg;

// Fonts
LV_FONT_DECLARE(arial_20bold);
LV_FONT_DECLARE(calibri_20b);
LV_FONT_DECLARE(calibri_20);
LV_FONT_DECLARE(calibri_24b);
LV_FONT_DECLARE(calibri_32b);

// Images

LV_IMG_DECLARE(led_on);
LV_IMG_DECLARE(led_off);

const struct device *dev0, *dev1, *dev2, *dev3, *ext_led;
bool led_is_on = true;
int ret0, ret1, ret2, ret3;
int f = 0;

void status_led_handler(struct k_work *work) {
  switch (f) {
  case 0:
    gpio_pin_set(dev0, PIN0, (int)led_is_on);
    gpio_pin_set(dev1, PIN1, (int)!led_is_on);
    gpio_pin_set(dev2, PIN2, (int)!led_is_on);
    gpio_pin_set(dev3, PIN3, (int)!led_is_on);
    f++;
    break;
  case 1:
    gpio_pin_set(dev0, PIN0, (int)!led_is_on);
    gpio_pin_set(dev1, PIN1, (int)led_is_on);
    gpio_pin_set(dev2, PIN2, (int)!led_is_on);
    gpio_pin_set(dev3, PIN3, (int)!led_is_on);
    f++;
    break;
  case 2:
    gpio_pin_set(dev0, PIN0, (int)!led_is_on);
    gpio_pin_set(dev1, PIN1, (int)!led_is_on);
    gpio_pin_set(dev2, PIN2, (int)!led_is_on);
    gpio_pin_set(dev3, PIN3, (int)led_is_on);
    f++;
    break;
  case 3:
    gpio_pin_set(dev0, PIN0, (int)!led_is_on);
    gpio_pin_set(dev1, PIN1, (int)!led_is_on);
    gpio_pin_set(dev2, PIN2, (int)led_is_on);
    gpio_pin_set(dev3, PIN3, (int)!led_is_on);
    f = 0;
    break;
  }
}

K_WORK_DEFINE(status_led, status_led_handler);

void my_led_timer_handler(struct k_timer *dummy) {
  k_work_submit(&status_led);
}

K_TIMER_DEFINE(my_led_timer, my_led_timer_handler, NULL);

static void on_button1(lv_obj_t *btn, lv_event_t event) {
  if (btn == btn1) {
    if (event == LV_EVENT_PRESSED) {
      run_timmer = !run_timmer;
      if (m_gui_callback) {
        m_gui_event.evt_type = GUI_EVT_BUTTON_PRESSED;
        m_gui_event.button_checked = true;
        m_gui_callback(&m_gui_event);
      }
    } else if (event == LV_EVENT_RELEASED) {

      if (m_gui_callback) {
        m_gui_event.evt_type = GUI_EVT_BUTTON_PRESSED;
        m_gui_event.button_checked = false;
        m_gui_callback(&m_gui_event);
      }
    }
  }
  if (btn == btn2) {
    if (event == LV_EVENT_PRESSED) {
      reset_timmer = !reset_timmer;
      if (m_gui_callback) {
        m_gui_event.evt_type = GUI_EVT_BUTTON_PRESSED;
        m_gui_event.button_checked = true;
        m_gui_callback(&m_gui_event);
      }
    }
  }
  if (btn == btn3) {
    if (event == LV_EVENT_PRESSED) {
      traveled_distance = 0;
      if (m_gui_callback) {
        m_gui_event.evt_type = GUI_EVT_BUTTON_PRESSED;
        m_gui_event.button_checked = true;
        m_gui_callback(&m_gui_event);
      }
    }
  }
  if (btn == btn4) {
    if (event == LV_EVENT_PRESSED) {
      lap_msec=msec;
      lap_sec=sec;
      lap_min=min;
      reset_timmer = !reset_timmer;

      if (m_gui_callback) {
        m_gui_event.evt_type = GUI_EVT_BUTTON_PRESSED;
        m_gui_event.button_checked = true;
        m_gui_callback(&m_gui_event);
      }
    }
  }
}

static void init_styles(void) {
  /*Create background style*/
  static lv_style_t style_screen;
  lv_style_set_bg_color(&style_screen, LV_STATE_DEFAULT, LV_COLOR_MAKE(0xff, 0xff, 0xff));
  lv_obj_add_style(lv_scr_act(), LV_BTN_PART_MAIN, &style_screen);

  /*Create the screen header label style*/
  lv_style_init(&style_header);
  lv_style_set_bg_opa(&style_header, LV_STATE_DEFAULT, LV_OPA_COVER);
  lv_style_set_bg_color(&style_header, LV_STATE_DEFAULT, LV_COLOR_MAKE(0, 4, 138));
  lv_style_set_radius(&style_header, LV_STATE_DEFAULT, 5);
  //lv_style_set_bg_grad_color(&style_header, LV_STATE_DEFAULT, LV_COLOR_TEAL);
  //lv_style_set_bg_grad_dir(&style_header, LV_STATE_DEFAULT, LV_GRAD_DIR_VER);
  lv_style_set_pad_left(&style_header, LV_STATE_DEFAULT, 0);
  lv_style_set_pad_top(&style_header, LV_STATE_DEFAULT, 5);

  /*Screen header text style*/
  lv_style_set_text_color(&style_header, LV_STATE_DEFAULT, LV_COLOR_MAKE(0xff, 0xff, 0xff));
  lv_style_set_text_font(&style_header, LV_STATE_DEFAULT, &calibri_20);

  lv_style_init(&style_con_bg);
  lv_style_copy(&style_con_bg, &style_header);
  lv_style_set_bg_color(&style_con_bg, LV_STATE_DEFAULT, LV_COLOR_MAKE(0xff, 0x3f, 0x34));
  
  lv_style_set_bg_opa(&style_con_bg, LV_STATE_DEFAULT, LV_OPA_50);
  lv_style_set_radius(&style_header, LV_STATE_DEFAULT, 4);

  /*Create a label style*/
  lv_style_init(&style_label);
  lv_style_set_bg_opa(&style_label, LV_STATE_DEFAULT, LV_OPA_COVER);
  lv_style_set_bg_color(&style_label, LV_STATE_DEFAULT, LV_COLOR_SILVER);
  lv_style_set_bg_grad_color(&style_label, LV_STATE_DEFAULT, LV_COLOR_GRAY);
  lv_style_set_bg_grad_dir(&style_label, LV_STATE_DEFAULT, LV_GRAD_DIR_VER);
  lv_style_set_pad_left(&style_label, LV_STATE_DEFAULT, 5);
  lv_style_set_pad_top(&style_label, LV_STATE_DEFAULT, 10);

  /*Add a border*/
  lv_style_set_border_color(&style_label, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_style_set_border_opa(&style_label, LV_STATE_DEFAULT, LV_OPA_70);
  lv_style_set_border_width(&style_label, LV_STATE_DEFAULT, 3);

  /*Set the text style*/
  lv_style_set_text_color(&style_label, LV_STATE_DEFAULT, LV_COLOR_MAKE(0x00, 0x00, 0x30));
  lv_style_set_text_font(&style_label, LV_STATE_DEFAULT, &calibri_20);

  /*Create a label value style*/
  lv_style_init(&style_label_value);
  lv_style_set_bg_opa(&style_label_value, LV_STATE_DEFAULT, LV_OPA_20);
  lv_style_set_bg_color(&style_label_value, LV_STATE_DEFAULT, LV_COLOR_SILVER);
  lv_style_set_bg_grad_color(&style_label_value, LV_STATE_DEFAULT, LV_COLOR_TEAL);
  lv_style_set_bg_grad_dir(&style_label_value, LV_STATE_DEFAULT, LV_GRAD_DIR_VER);
  lv_style_set_pad_left(&style_label_value, LV_STATE_DEFAULT, 0);
  lv_style_set_pad_top(&style_label_value, LV_STATE_DEFAULT, 3);

  /*Set the text style*/
  lv_style_set_text_color(&style_label_value, LV_STATE_DEFAULT, LV_COLOR_MAKE(0x00, 0x00, 0x30));
  lv_style_set_text_font(&style_label_value, LV_STATE_DEFAULT, &calibri_20);

  lv_style_init(&style_bkg);
  lv_style_set_bg_color(&style_bkg, LV_STATE_DEFAULT, LV_COLOR_MAKE(0x12, 0xff, 0x38));

  /*Create a simple button style*/
  lv_style_init(&style_btn);
  lv_style_set_radius(&style_btn, LV_STATE_DEFAULT, 5);
  lv_style_set_bg_opa(&style_btn, LV_STATE_DEFAULT, LV_OPA_COVER);
  lv_style_set_bg_color(&style_btn, LV_STATE_DEFAULT, LV_COLOR_MAKE(0, 4, 138));
  lv_style_set_bg_grad_color(&style_btn, LV_STATE_DEFAULT, LV_COLOR_GRAY);
  lv_style_set_bg_grad_dir(&style_btn, LV_STATE_DEFAULT, LV_GRAD_DIR_VER);

  /*Swap the colors in pressed state*/
  lv_style_set_bg_color(&style_btn, LV_STATE_PRESSED, LV_COLOR_MAKE(0, 4, 138));
  lv_style_set_bg_grad_color(&style_btn, LV_STATE_PRESSED, LV_COLOR_SILVER);

  /*Add a border*/
  lv_style_set_border_color(&style_btn, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  lv_style_set_border_opa(&style_btn, LV_STATE_DEFAULT, LV_OPA_70);
  lv_style_set_border_width(&style_btn, LV_STATE_DEFAULT, 2);

  /*Different border color in focused state*/
  lv_style_set_border_color(&style_btn, LV_STATE_FOCUSED, LV_COLOR_BLACK);
  lv_style_set_border_color(&style_btn, LV_STATE_FOCUSED | LV_STATE_PRESSED, LV_COLOR_NAVY);

  /*Set the text style*/
  lv_style_set_text_color(&style_btn, LV_STATE_DEFAULT, LV_COLOR_MAKE(255,255,255));
  lv_style_set_text_font(&style_btn, LV_STATE_DEFAULT, &calibri_20);

  /*Make the button smaller when pressed*/
  lv_style_set_transform_height(&style_btn, LV_STATE_PRESSED, -4);
  lv_style_set_transform_width(&style_btn, LV_STATE_PRESSED, -8);
#if LV_USE_ANIMATION
  /*Add a transition to the size change*/
  static lv_anim_path_t path;
  lv_anim_path_init(&path);
  lv_anim_path_set_cb(&path, lv_anim_path_overshoot);

  lv_style_set_transition_prop_1(&style_btn, LV_STATE_DEFAULT, LV_STYLE_TRANSFORM_HEIGHT);
  lv_style_set_transition_prop_2(&style_btn, LV_STATE_DEFAULT, LV_STYLE_TRANSFORM_WIDTH);
  lv_style_set_transition_time(&style_btn, LV_STATE_DEFAULT, 200);
  lv_style_set_transition_path(&style_btn, LV_STATE_DEFAULT, &path);
#endif
}

void init_blinky_gui(char speed[8], char time[8]) {

  // The connected header needs to be created before the top_header, to appear behind

  top_header = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(top_header, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(top_header, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(top_header, 1, 1);
  lv_obj_set_size(top_header, 318, 30);
  lv_label_set_text(top_header, "Bike Mate");
  lv_label_set_align(top_header, LV_LABEL_ALIGN_CENTER);

  current_speed_block = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(current_speed_block, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(current_speed_block, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(current_speed_block, 0, 32);
  lv_obj_set_size(current_speed_block, 159, 60);
  lv_label_set_text(current_speed_block, " Current speed:");
  lv_label_set_align(current_speed_block, LV_LABEL_ALIGN_LEFT);

  distance_text = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(distance_text, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(distance_text, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(distance_text, 161, 32);
  lv_obj_set_size(distance_text, 159, 60);
  lv_label_set_text(distance_text, " Traveled distance:");
  lv_label_set_align(distance_text, LV_LABEL_ALIGN_LEFT);

  distance_container = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(distance_container, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(distance_container, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(distance_container, 161, 60);
  lv_obj_set_size(distance_container, 159, 30);
  lv_label_set_text(distance_container, "-- Km");
  lv_label_set_align(distance_container, LV_LABEL_ALIGN_CENTER);

  speed_block = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(speed_block, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(speed_block, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(speed_block, 0, 60);
  lv_obj_set_size(speed_block, 159, 30);
  lv_label_set_text(speed_block, speed);
  lv_label_set_align(speed_block, LV_LABEL_ALIGN_CENTER);

  text_timer_block = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(text_timer_block, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(text_timer_block, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(text_timer_block, 0, 93);
  lv_obj_set_size(text_timer_block, 159, 30);
  lv_label_set_text(text_timer_block, " Timer:");
  lv_label_set_align(text_timer_block, LV_LABEL_ALIGN_LEFT);

  timer_block = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(timer_block, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(timer_block, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(timer_block, 0, 116);
  lv_obj_set_size(timer_block, 159, 30);
  lv_label_set_text(timer_block, time);
  lv_label_set_align(timer_block, LV_LABEL_ALIGN_CENTER);

  lap_text = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(lap_text, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(lap_text, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(lap_text, 161, 93);
  lv_obj_set_size(lap_text, 159, 30);
  lv_label_set_text(lap_text, " Last lap time:");
  lv_label_set_align(lap_text, LV_LABEL_ALIGN_LEFT);

  lap_data = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(lap_data, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(lap_data, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(lap_data, 161, 116);
  lv_obj_set_size(lap_data, 159, 30);
  lv_label_set_text(lap_data, time);
  lv_label_set_align(lap_data, LV_LABEL_ALIGN_CENTER);

  data = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(data, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(data, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(data, 0, 147);
  lv_obj_set_size(data, 159, 30);
  lv_label_set_text(data, " Angle ( X, Z ):");
  lv_label_set_align(data, LV_LABEL_ALIGN_LEFT);

  data_text = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(data_text, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(data_text, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(data_text, 0, 170);
  lv_obj_set_size(data_text, 159, 30);
  lv_label_set_text(data_text, "--.--* --.--*");
  lv_label_set_align(data_text, LV_LABEL_ALIGN_CENTER);

    data = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(data, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(data, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(data, 161, 147);
  lv_obj_set_size(data, 159, 30);
  lv_label_set_text(data, " Bluetooth status:");
  lv_label_set_align(data, LV_LABEL_ALIGN_LEFT);

  data_text = lv_label_create(lv_scr_act(), NULL);
  lv_obj_add_style(data_text, LV_LABEL_PART_MAIN, &style_header);
  lv_label_set_long_mode(data_text, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(data_text, 161, 170);
  lv_obj_set_size(data_text, 159, 30);
  lv_label_set_text(data_text, bluetooth_statuses[0]);
  lv_label_set_align(data_text, LV_LABEL_ALIGN_CENTER);
  /* the angle feauture and anything related to comunicating with the phone is not added because i didnt figured out how to 
        implement this for the current dev board, but i am avable to finish this if someone helps me
        */

  btn1 = lv_btn_create(lv_scr_act(), NULL); /*Add a button the current screen*/
  lv_obj_set_pos(btn1, 160, 200);             /*Set its position*/
  lv_obj_set_size(btn1, 80, 40);            /*Set its size*/
  //lv_obj_reset_style_list(btn1, LV_BTN_PART_MAIN);         /*Remove the styles coming from the theme*/
  lv_obj_add_style(btn1, LV_BTN_PART_MAIN, &style_btn);
  lv_btn_set_checkable(btn1, true);

  btn1_label = lv_label_create(btn1, NULL); /*Add a label to the button*/
  lv_label_set_text(btn1_label, "Start");   /*Set the labels text*/
  lv_obj_set_event_cb(btn1, on_button1);

  btn2 = lv_btn_create(lv_scr_act(), NULL);        /*Add a button the current screen*/
  lv_obj_set_pos(btn2, 80, 200);                   /*Set its position*/
  lv_obj_set_size(btn2, 80, 40);                   /*Set its size*/
  lv_obj_reset_style_list(btn2, LV_BTN_PART_MAIN); /*Remove the styles coming from the theme*/
  lv_obj_add_style(btn2, LV_BTN_PART_MAIN, &style_btn);
  lv_btn_set_checkable(btn2, true);

  btn2_label = lv_label_create(btn2, NULL); /*Add a label to the button*/
  lv_label_set_text(btn2_label, "Reset T"); /*Set the labels text*/
  lv_obj_set_event_cb(btn2, on_button1);

  btn3 = lv_btn_create(lv_scr_act(), NULL);        /*Add a button the current screen*/
  lv_obj_set_pos(btn3, 0, 200);                  /*Set its position*/
  lv_obj_set_size(btn3, 80, 40);                   /*Set its size*/
  lv_obj_reset_style_list(btn3, LV_BTN_PART_MAIN); /*Remove the styles coming from the theme*/
  lv_obj_add_style(btn3, LV_BTN_PART_MAIN, &style_btn);
  lv_btn_set_checkable(btn3, true);

  btn3_label = lv_label_create(btn3, NULL); /*Add a label to the button*/
  lv_label_set_text(btn3_label, "Reset D"); /*Set the labels text*/
  lv_obj_set_event_cb(btn3, on_button1);

  btn4 = lv_btn_create(lv_scr_act(), NULL);        /*Add a button the current screen*/
  lv_obj_set_pos(btn4, 240, 200);                  /*Set its position*/
  lv_obj_set_size(btn4, 80, 40);                   /*Set its size*/
  lv_obj_reset_style_list(btn4, LV_BTN_PART_MAIN); /*Remove the styles coming from the theme*/
  lv_obj_add_style(btn4, LV_BTN_PART_MAIN, &style_btn);
  lv_btn_set_checkable(btn4, true);

  btn4_label = lv_label_create(btn4, NULL); /*Add a label to the button*/
  lv_label_set_text(btn4_label, "Lap");     /*Set the labels text*/
  lv_obj_set_event_cb(btn4, on_button1);
}

void gui_init(gui_config_t *config) {
  m_gui_callback = config->event_callback;
}

void gui_set_bt_state(gui_bt_state_t state) {
  static gui_message_t set_bt_state_msg;
  set_bt_state_msg.type = GUI_MSG_SET_BT_STATE;
  set_bt_state_msg.params.bt_state = state;
  k_msgq_put(&m_gui_cmd_queue, &set_bt_state_msg, K_NO_WAIT);
}

void update_speed_value() {

  itoa(speed_now, speed, 10);

  strcat(speed, " Km/H");
  lv_label_set_text(speed_block, speed);
}

void update_distance() {

  meters = traveled_distance % 1000;
  kmeters = traveled_distance / 1000;

  itoa(meters, metersc, 10);
  itoa(kmeters, kmetersc, 10);

  strcat(traveled_distancec, kmetersc);
  strcat(traveled_distancec, ".");
  if (meters < 100)
    strcat(traveled_distancec, "0");
  if (meters < 10)
    strcat(traveled_distancec, "0");
  strcat(traveled_distancec, metersc);
  strcat(traveled_distancec, " Km");
  lv_label_set_text(distance_container, traveled_distancec);
  memset(traveled_distancec, 0, 40);
}

void update_lap_time(){
  itoa(lap_msec/10,lap_msecc,10);
  itoa(lap_sec,lap_secc,10);
  itoa(lap_min,lap_minc,10);
  strcat(lap, lap_minc);
  strcat(lap, ":");
  if (lap_sec < 10)
    strcat(lap, "0");
  strcat(lap, lap_secc);
  strcat(lap, ":");
  if (lap_msec < 10)
    strcat(lap, "0");
  strcat(lap, lap_msecc);
  lv_label_set_text(lap_data, lap);
  memset(lap, 0, 20);
  memset(lap_secc, 0, 5);
  memset(lap_minc, 0, 5);
  memset(lap_msecc, 0, 5);
}


void update_screen_values() {
  update_speed_value();
  update_timer_values();
  update_distance();
  update_lap_time();
}

void update_timer_values() {
  if (msec >= 1000) {
    sec += msec / 1000;
    msec = msec % 1000;
  }
  if (sec >= 60) {
    sec = sec - 60;
    min++;
  }
  itoa(sec, secc, 10);
  itoa(min, minc, 10);
  itoa(msec / 10, msecc, 10);

  strcat(timer, minc);
  strcat(timer, ":");
  if (sec < 10)
    strcat(timer, "0");
  strcat(timer, secc);
  strcat(timer, ":");
  if (msec < 10)
    strcat(timer, "0");
  strcat(timer, msecc);
  lv_label_set_text(timer_block, timer);
  memset(timer, 0, 20);
  memset(secc, 0, 5);
  memset(minc, 0, 5);
  memset(msecc, 0, 5);
}

void my_work_handler(struct k_work *work) {

  update_screen_values();
}

K_WORK_DEFINE(my_work, my_work_handler);

void my_timer_handler(struct k_timer *dummy) {
  k_work_submit(&my_work);
}

K_TIMER_DEFINE(my_timer, my_timer_handler, NULL);

void my_work_handler1(struct k_work *work) {
  if (run_timmer) {
    msec = msec + 10;
  }
  if (reset_timmer) {

    msec = 0;
    sec = 0;
    min = 0;
    reset_timmer = false;
  }
}

K_WORK_DEFINE(my_work1, my_work_handler1);

void my_timer_handler1(struct k_timer *dummy) {
  k_work_submit(&my_work1);
}

K_TIMER_DEFINE(my_timer1, my_timer_handler1, NULL);

void my_speed_work_handler(struct k_work *work) {

  speed_now = counter * WHEEL_PERIMETER / 1000 * 3, 6;
  counter = 0;
}

K_WORK_DEFINE(my_speed_work, my_speed_work_handler);

void my_speed_timer_handler(struct k_timer *dummy) {
  k_work_submit(&my_speed_work);
}

K_TIMER_DEFINE(my_speed_timer, my_speed_timer_handler, NULL);

static struct gpio_callback button_cb_data;

void button_pressed(const struct device *dev, struct gpio_callback *cb,
    uint32_t pins) {
  traveled_distance = traveled_distance + WHEEL_PERIMETER / 1000;
  counter++;
}

void gui_run(void) {

  button = device_get_binding(SW0_GPIO_LABEL);
  if (button == NULL) {
    printk("Error: didn't find %s device\n", SW0_GPIO_LABEL);
    return;
  }

  ret = gpio_pin_configure(button, SW0_GPIO_PIN, SW0_GPIO_FLAGS);
  if (ret != 0) {
    printk("Error %d: failed to configure %s pin %d\n",
        ret, SW0_GPIO_LABEL, SW0_GPIO_PIN);
    return;
  }

  ret = gpio_pin_interrupt_configure(button,
      SW0_GPIO_PIN,
      GPIO_INT_EDGE_TO_ACTIVE);
  if (ret != 0) {
    printk("Error %d: failed to configure interrupt on %s pin %d\n",
        ret, SW0_GPIO_LABEL, SW0_GPIO_PIN);
    return;
  }

  gpio_init_callback(&button_cb_data, button_pressed, BIT(SW0_GPIO_PIN));
  gpio_add_callback(button, &button_cb_data);
  printk("Set up button at %s pin %d\n", SW0_GPIO_LABEL, SW0_GPIO_PIN);

  dev0 = device_get_binding(LED0);
  dev1 = device_get_binding(LED1);
  dev2 = device_get_binding(LED2);
  dev3 = device_get_binding(LED3);

  ret0 = gpio_pin_configure(dev0, PIN0, GPIO_OUTPUT_ACTIVE | FLAGS0);
  ret1 = gpio_pin_configure(dev1, PIN1, GPIO_OUTPUT_ACTIVE | FLAGS1);
  ret2 = gpio_pin_configure(dev2, PIN2, GPIO_OUTPUT_ACTIVE | FLAGS2);
  ret3 = gpio_pin_configure(dev3, PIN3, GPIO_OUTPUT_ACTIVE | FLAGS3);

  display_dev = device_get_binding(CONFIG_LVGL_DISPLAY_DEV_NAME);

  if (display_dev == NULL) {
    LOG_ERR("Display device not found!");
    return;
  }

  init_styles();

  init_blinky_gui("-- Km/H", "-:--:--");
  k_sleep(K_MSEC(1000));
  display_blanking_off(display_dev);

  k_timer_start(&my_led_timer, K_MSEC(175), K_MSEC(175));
  k_timer_start(&my_timer, K_MSEC(31), K_MSEC(31));
  k_timer_start(&my_speed_timer, K_SECONDS(1), K_SECONDS(1));
  k_timer_start(&my_timer1, K_MSEC(10), K_MSEC(10));

  while (true) {

    lv_task_handler();

    k_sleep(K_MSEC(10));
  }
}

K_THREAD_DEFINE(gui_thread, 4096, gui_run, NULL, NULL, NULL, 7, 0, 0);