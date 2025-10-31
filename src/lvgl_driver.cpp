// src/lvgl_driver.cpp
#include <Arduino.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;  // Reference to your existing display object

// Display buffers - double buffering for smooth updates
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[480 * 20];  // Buffer for 20 lines
static lv_color_t buf2[480 * 20];  // Second buffer for double buffering

// Display flushing callback - this is called by LVGL to render
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Use your existing Arduino_GFX object
    gfx->startWrite();
    gfx->writeAddrWindow(area->x1, area->y1, w, h);
    gfx->writePixels((uint16_t *)&color_p->full, w * h);
    gfx->endWrite();

    // Tell LVGL we're done flushing
    lv_disp_flush_ready(disp);
}

// Initialize LVGL with your display driver
void lvgl_driver_init() {
    // Initialize LVGL
    lv_init();

    // Initialize the display buffer
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 480 * 20);

    // Initialize and register the display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 320;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    Serial.println("LVGL driver initialized");
}