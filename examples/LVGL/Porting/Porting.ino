#include <Arduino.h>
#include <lvgl.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <ui.h>
#include <ESP_Panel_Library.h>
#include <ESP_IOExpander_Library.h>
#include "../../ui/src/ui_events.h"


#define LV_BUF_SIZE     (ESP_PANEL_LCD_H_RES * 20)

ESP_Panel *panel = NULL;
SemaphoreHandle_t lvgl_mux = NULL;                  // LVGL mutex

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp6.aliyun.com", 28800, 60000);

LV_FONT_DECLARE(ui_font_FontNumber28bp4);
LV_FONT_DECLARE(ui_font_FontNumber48bp4);

lv_obj_t *wifiListView = NULL;

bool ShowWifiList_flag = false;
bool WifiConnected_flag = false;
bool ShowClock_flag = false;

static int num_wifi = 0;
static int connected_count = 0;
static char dateString[20];

static const char *test_Wifiname = "TP-LINK_Liu";
static const char *selectedWifiName = NULL;
static const char *Wifipassword = NULL;

void wifiListClicked_cb(lv_event_t * e);

#if ESP_PANEL_LCD_BUS_TYPE == ESP_PANEL_BUS_TYPE_RGB
/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    panel->getLcd()->drawBitmap(area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
    lv_disp_flush_ready(disp);
}
#else
/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    panel->getLcd()->drawBitmap(area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
}

bool notify_lvgl_flush_ready(void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}
#endif /* ESP_PANEL_LCD_BUS_TYPE */

#if ESP_PANEL_USE_LCD_TOUCH
/* Read the touchpad */
void my_touchpad_read(lv_indev_drv_t * indev, lv_indev_data_t * data)
{
    panel->getLcdTouch()->readData();

    bool touched = panel->getLcdTouch()->getTouchState();
    if(!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        TouchPoint point = panel->getLcdTouch()->getPoint();

        data->state = LV_INDEV_STATE_PR;
        /*Set the coordinates*/
        data->point.x = point.x;
        data->point.y = point.y;

        // Serial.printf("Touch point: x %d, y %d\n", point.x, point.y);
    }
}
#endif

bool lv_port_lock(uint32_t timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lv_port_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

void lvgl_task(void *pvParameter)
{
    while (1) {
        lv_port_lock(0);
        lv_task_handler();
        lv_port_unlock();
        delay(5);
    }
}

void wifiListClicked_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    
    if (event_code == LV_EVENT_CLICKED) {
        ShowWifiList_flag = false;
        Serial.println("ShowWifiList_flag false"); 
        WiFi.scanDelete(); 

        lv_port_lock(0);

        selectedWifiName = lv_list_get_btn_text(wifiListView, target);
        if (selectedWifiName != NULL) {
            Serial.printf("%s\n", selectedWifiName);
        }

        _ui_screen_change(&ui_ScreenPassword, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_ScreenPassword_screen_init);
        
        lv_port_unlock();
    }
    
}

void Wifi_Scan_cb(lv_event_t * e)
{   
    ShowWifiList_flag = true;
    ShowClock_flag = false;
    Serial.println("ShowWifiList_flag true"); 
}

void KeyConfirm_cb(lv_event_t * e)
{
    Wifipassword = lv_textarea_get_text(ui_TextPassword);
    Serial.printf("%s\n", Wifipassword);

    WiFi.begin(selectedWifiName, Wifipassword);
    WifiConnected_flag = true;
}

static void clock_run_cb(lv_timer_t *timer)
{
    lv_port_lock(0);

    lv_obj_t *lab_time = (lv_obj_t *) timer->user_data;
    lv_label_set_text_fmt(lab_time, "%s", timeClient.getFormattedTime());

    lv_port_unlock();
}

static void calendar_run_cb(lv_timer_t *timer)
{
    lv_port_lock(0);

    lv_obj_t *lab_calendar = (lv_obj_t *) timer->user_data;
    lv_label_set_text_fmt(lab_calendar, "%s", dateString);

    lv_port_unlock();
}

static void week_run_cb(lv_timer_t *timer)
{
    String wk[7] = {"日","一","二","三","四","五","六"};
    String s = "星期" + wk[timeClient.getDay()];
    lv_port_lock(0);

    lv_obj_t *lab_calendar = (lv_obj_t *) timer->user_data;
    lv_label_set_text_fmt(lab_calendar, "%s", s);

    lv_port_unlock();
}

void ShowClcok_cb(lv_event_t * e)
{
    timeClient.begin();

    ShowClock_flag = true;

    lv_port_lock(0);

    lv_obj_t *lab_time = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(lab_time, &ui_font_FontNumber48bp4, LV_PART_MAIN);
    lv_label_set_text_static(lab_time, "23:59");
    lv_obj_align(lab_time, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_bg_opa(lab_time, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab_time, lv_color_make(196, 191, 191), LV_PART_MAIN);
    lv_timer_t *timer = lv_timer_create(clock_run_cb, 1000, (void *) lab_time);
    clock_run_cb(timer);

    lv_obj_t *lab_calendar = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(lab_calendar, &ui_font_FontNumber28bp4, LV_PART_MAIN);
    lv_label_set_text_static(lab_calendar, "2023-01-01");
    lv_obj_align(lab_calendar, LV_ALIGN_BOTTOM_LEFT, 20, -50);
    lv_obj_set_style_bg_opa(lab_calendar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab_calendar, lv_color_make(196, 191, 191), LV_PART_MAIN);
    lv_timer_t *timer_cal = lv_timer_create(calendar_run_cb, 1000, (void *) lab_calendar);
    calendar_run_cb(timer_cal);

    lv_obj_t *lab_week = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(lab_week, &ui_font_FontNumber28bp4, LV_PART_MAIN);
    lv_label_set_text_static(lab_week, "星期一");
    lv_obj_align(lab_week, LV_ALIGN_BOTTOM_RIGHT, -20, -50);
    lv_obj_set_style_bg_opa(lab_week, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab_week, lv_color_make(196, 191, 191), LV_PART_MAIN);
    lv_timer_t *timer_week = lv_timer_create(week_run_cb, 1000, (void *) lab_week);
    week_run_cb(timer_week);

    lv_port_unlock();
}

void setup()
{
    Serial.begin(115200); /* prepare for possible serial debug */

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);   

    ShowWifiList_flag = false;
    WifiConnected_flag = false;
    ShowClock_flag = false;

    panel = new ESP_Panel();

    /* Initialize LVGL core */
    lv_init();

    /* Initialize LVGL buffers */
    static lv_disp_draw_buf_t draw_buf;
    /* Using double buffers is more faster than single buffer */
    /* Using internal SRAM is more fast than PSRAM (Note: Memory allocated using `malloc` may be located in PSRAM.) */
    uint8_t *buf = (uint8_t *)heap_caps_calloc(1, LV_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_INTERNAL);
    assert(buf);
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LV_BUF_SIZE);

    /* Initialize the display device */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = ESP_PANEL_LCD_H_RES;
    disp_drv.ver_res = ESP_PANEL_LCD_V_RES;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

#if ESP_PANEL_USE_LCD_TOUCH
    /* Initialize the input device */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
#endif

    /* There are some extral initialization for ESP32-S3-LCD-EV-Board */
#ifdef ESP_PANEL_BOARD_ESP32_S3_LCD_EV_BOARD
    /* Initialize IO expander */
    ESP_IOExpander *expander = new ESP_IOExpander_TCA95xx_8bit(ESP_PANEL_LCD_TOUCH_BUS_HOST_ID, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, ESP_PANEL_LCD_TOUCH_I2C_IO_SCL, ESP_PANEL_LCD_TOUCH_I2C_IO_SDA);
    expander->init();
    expander->begin();
    /* Add into panel for 3-wire SPI */
    panel->addIOExpander(expander);
#endif

    /* There are some extral initialization for ESP32-S3-Korvo-2 */
#ifdef ESP_PANEL_BOARD_ESP32_S3_KORVO_2
    /* Initialize IO expander */
    ESP_IOExpander *expander = new ESP_IOExpander_TCA95xx_8bit(ESP_PANEL_LCD_TOUCH_BUS_HOST_ID, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, ESP_PANEL_LCD_TOUCH_I2C_IO_SCL, ESP_PANEL_LCD_TOUCH_I2C_IO_SDA);
    expander->init();
    expander->begin();
    /* Reset LCD */
    expander->pinMode(2, OUTPUT);
    expander->digitalWrite(2, LOW);
    usleep(20000);
    expander->digitalWrite(2, LOW);
    usleep(120000);
    expander->digitalWrite(2, HIGH);
    /* Turn on backlight */
    expander->pinMode(1, OUTPUT);
    expander->digitalWrite(1, HIGH);
    /* Keep CS low */
    expander->pinMode(3, OUTPUT);
    expander->digitalWrite(3, LOW);
#endif

    /* Initialize bus and device of panel */
    panel->init();
#if ESP_PANEL_LCD_BUS_TYPE != ESP_PANEL_BUS_TYPE_RGB
    /* Register a function to notify LVGL when the panel is ready to flush */
    /* This is useful for refreshing the screen using DMA transfers */
    panel->getLcd()->setCallback(notify_lvgl_flush_ready, &disp_drv);
#endif
    /* Start panel */
    panel->begin();

    /* Create a task to run the LVGL task periodically */
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    xTaskCreate(lvgl_task, "lvgl", 8192, NULL, 1, NULL);

    /**
     * To avoid errors caused by multiple tasks simultaneously accessing LVGL,
     * should acquire a lock before operating on LVGL.
     */

    lv_port_lock(0);
    
    ui_init();

    lv_port_unlock();

    Serial.println("Setup done");
}

void loop()
{
  if (ShowWifiList_flag == true) {
   num_wifi = WiFi.scanNetworks();
   Serial.println("Scan done");

    if(num_wifi == 0) {
        Serial.println("no networks found");
    } else if (ShowWifiList_flag == true){
        Serial.println("Wifi list show:");
        lv_port_lock(0);

        wifiListView = lv_list_create(lv_scr_act());
        lv_obj_set_size(wifiListView, 300, 190);
        for (int i = 0; i < num_wifi; i++) {
            lv_obj_t *wifiListItem = lv_list_add_btn(wifiListView, NULL, WiFi.SSID(i).c_str());
            lv_obj_set_user_data(wifiListItem, (void *)WiFi.SSID(i).c_str());
            lv_obj_add_event_cb(wifiListItem, wifiListClicked_cb, LV_EVENT_ALL, NULL);
        }
        lv_obj_align(wifiListView, LV_ALIGN_CENTER, 0, 20);

        lv_port_unlock();
    }
    WiFi.scanDelete();
  }

  if(WifiConnected_flag == true) {
    Serial.println("Wifi connecting...");
    Serial.printf("%s ", selectedWifiName);
    Serial.printf("%s\n", Wifipassword);
    connected_count++;

    lv_port_lock(0);

    if (WiFi.status() == WL_CONNECTED) {
        WifiConnected_flag = false;
        Serial.println("password correct: Wifi connected success");
        _ui_screen_change(&ui_ScreenClock, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_ScreenClock_screen_init);
        _ui_flag_modify(ui_LoadSpinner, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        lv_textarea_set_text(ui_TextPassword, "");
    } else if(connected_count == 3) {
        connected_count = 0;
        WifiConnected_flag = false;
        Serial.println("password wrong: Wifi connected failed");
        _ui_flag_modify(ui_LoadSpinner, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        lv_textarea_set_text(ui_TextPassword, "");
    }

    lv_port_unlock();
  }

  if(ShowClock_flag == true) {
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime((time_t *)&epochTime);

    snprintf(dateString, sizeof(dateString), "%04d-%02d-%02d", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday);
    Serial.print("Current date: ");
    Serial.println(dateString);
    
    Serial.print("Current day of the week: ");
    Serial.println(timeClient.getDay());
  }

  delay(5000);
}
