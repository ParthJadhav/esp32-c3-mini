#include "ui.h"
#include <Arduino.h>
#include <lvgl.h>

static void btn_event_cb(lv_event_t *e) {
  const char *msg = (const char *)lv_event_get_user_data(e);
  // Serial.println(msg);
}

void ui_init(void) {
  lv_disp_t *dispp = lv_disp_get_default();
  lv_obj_t *container = lv_obj_create(lv_scr_act());
  lv_obj_set_size(container, 300, 200);
  lv_obj_align(container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_layout(container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW_WRAP);

  // Create three buttons
  lv_obj_t *btn1 = lv_btn_create(container);
  lv_obj_t *btn2 = lv_btn_create(container);
  lv_obj_t *btn3 = lv_btn_create(container);

  // Set button labels
  lv_obj_t *label1 = lv_label_create(btn1);
  lv_label_set_text(label1, "Button 1");
  lv_obj_t *label2 = lv_label_create(btn2);
  lv_label_set_text(label2, "Button 2");
  lv_obj_t *label3 = lv_label_create(btn3);
  lv_label_set_text(label3, "Button 3");

  // Set button event callbacks
  lv_obj_add_event_cb(btn1, btn_event_cb, LV_EVENT_CLICKED,
                      (void *)"Button 1 pressed");
  lv_obj_add_event_cb(btn2, btn_event_cb, LV_EVENT_CLICKED,
                      (void *)"Button 2 pressed");
  lv_obj_add_event_cb(btn3, btn_event_cb, LV_EVENT_CLICKED,
                      (void *)"Button 3 pressed");
}
