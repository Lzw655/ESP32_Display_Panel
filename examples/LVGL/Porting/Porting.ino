#include <Arduino.h>
#include <lvgl.h>
#include <ESP_Panel_Library.h>
#include <ESP_IOExpander_Library.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <ui.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ArduinoNvs.h>
#include "../../ui/src/ui_events.h"


#define LV_BUF_SIZE     (ESP_PANEL_LCD_H_RES * 20)

static bool NVS_OK = false;

ESP_Panel *panel = NULL;
SemaphoreHandle_t lvgl_mux = NULL;                  // LVGL mutex

extern bool WifiConnected_Flag;
extern bool WifiList_switch;

static bool NVS_Flag = false;

static char hours_rol[10];
static char min_rol[10];
static char sec_rol[10];

static char AlarmTime[30]; 

lv_obj_t * Wifi_List = NULL;

static int Connected_Count = 0;
static int WeatherRefresh_Count = 0;

static const char *SelectedWifiName = NULL;
static const char *WifiPassword = NULL;

String st_WifiName;
String st_WifiPassWord;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp6.aliyun.com", 28800, 60000);

HTTPClient http;

String API = "SqXvU5BorXASbNWsT";    //填写：心知天气密钥
String CITY = "上海";                 //填写：所在城市
String url_xinzhi = "";
String WeatherURL = "";
String Weather = "";
String PreWeather = "";             
int temperature = 999;
char temperature_buffer[20];

static char dateString[20];
static int Num_Wifi = 0;

bool Passwordvalid_flag = false;

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

String GitURL(String api,String city)
{
  url_xinzhi =  "https://api.seniverse.com/v3/weather/now.json?key=";
  url_xinzhi += api;
  url_xinzhi += "&location=";
  url_xinzhi += city;
  url_xinzhi += "&language=zh-Hans&unit=c";
  return url_xinzhi;
}

void ParseWeather(String url)
{  
  DynamicJsonDocument doc(1024); //分配内存,动态

  http.begin(url);
 
  int httpGet = http.GET();
  if(httpGet > 0)
  { 
    if(httpGet == HTTP_CODE_OK) {
      String json = http.getString();
      deserializeJson(doc, json);
      
      Weather = doc["results"][0]["now"]["text"].as<String>();   //天气
      temperature = doc["results"][0]["now"]["temperature"].as<int>();
      
      Serial.printf("Weather: %s\n", Weather);
      Serial.printf("temperature: %d\n", temperature);
    } else {
      Serial.printf("ERROR: HTTP_CODE");
    }
  } else {
    Serial.printf("ERROR: httpGet");
  }

  http.end();
}

void WifiClock_run_cb(lv_timer_t *timer)
{
    if(WifiConnected_Flag == true) {
        lv_port_lock(0);

        lv_obj_t *ui_LabelTime = (lv_obj_t *) timer->user_data;
        lv_label_set_text_fmt(ui_LabelTime, "%s", timeClient.getFormattedTime());

        lv_port_unlock();
    }
}

void WifiCalendar_run_cb(lv_timer_t *timer)
{
    if(WifiConnected_Flag == true) {
        lv_port_lock(0);

        lv_obj_t *ui_Labelcalendar = (lv_obj_t *) timer->user_data;
        lv_label_set_text_fmt(ui_Labelcalendar, "%s", dateString);

        lv_port_unlock();
    }
}

void WifiWeek_run_cb(lv_timer_t *timer)
{
    if(WifiConnected_Flag == true) {
        String wk[7] = {"日","一","二","三","四","五","六"};
        String s = "星期" + wk[timeClient.getDay()];

        lv_port_lock(0);

        lv_obj_t *ui_LabelWeek = (lv_obj_t *) timer->user_data;
        lv_label_set_text_fmt(ui_LabelWeek, "%s", s);

        lv_port_unlock();
    }
}

void WifiWeather_run_cb(lv_timer_t *timer)
{
    if(WifiConnected_Flag == true) {
        lv_port_lock(0);
        
        lv_obj_t *ui_Labelweather = (lv_obj_t *) timer->user_data;
        lv_label_set_text_fmt(ui_Labelweather, "%s", Weather);
        if(PreWeather != Weather) {
            if(Weather == "晴") {
                _ui_flag_modify(ui_ImageCloudy, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                _ui_flag_modify(ui_ImageRain, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                _ui_flag_modify(ui_ImageSnow, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                _ui_flag_modify(ui_ImageSunny, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
            } else if(Weather == "阴" || Weather == "多云") {
                _ui_flag_modify(ui_ImageCloudy, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
                _ui_flag_modify(ui_ImageRain, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                _ui_flag_modify(ui_ImageSnow, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                _ui_flag_modify(ui_ImageSunny, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);                
            } else if(Weather == "小雨" || Weather == "中雨" || Weather == "大雨" || Weather == "暴雨") {
                _ui_flag_modify(ui_ImageCloudy, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                _ui_flag_modify(ui_ImageRain, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
                _ui_flag_modify(ui_ImageSnow, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                _ui_flag_modify(ui_ImageSunny, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);  
            }
            else {
                _ui_flag_modify(ui_ImageCloudy, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                _ui_flag_modify(ui_ImageRain, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                _ui_flag_modify(ui_ImageSnow, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
                _ui_flag_modify(ui_ImageSunny, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD); 
            }
        }
        PreWeather = Weather;
        lv_port_unlock();
    }
}

void WifiTemperature_run_cb(lv_timer_t *timer)
{
    if(WifiConnected_Flag == true) {
        lv_port_lock(0);

        lv_obj_t *ui_Labeltemperature = (lv_obj_t *) timer->user_data;
        sprintf(temperature_buffer, "%d℃", temperature);
        lv_label_set_text_fmt(ui_Labeltemperature, "%s", temperature_buffer);

        lv_port_unlock();
    }
}

void WifiListClicked_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    
    if (event_code == LV_EVENT_CLICKED) {
        lv_port_lock(0);
        
        WifiList_switch = false;
        Serial.println("WifiList_switch: false");

        SelectedWifiName = lv_list_get_btn_text(Wifi_List, target);
        if (SelectedWifiName != NULL) {
            Serial.printf("%s\n", SelectedWifiName);
        }

        _ui_screen_change(&ui_ScreenPassord, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_ScreenPassord_screen_init);
        
        lv_port_unlock();
    }
}

void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);

    if(event_code == LV_EVENT_CLICKED && lv_keyboard_get_selected_btn(target) == 39) {
        WifiPassword = lv_textarea_get_text(ui_TextPassword);
        Serial.printf("%s\n", WifiPassword);

        WiFi.begin(SelectedWifiName, WifiPassword);

        Passwordvalid_flag = true;

        _ui_flag_modify(ui_SpinnerLoadPassword, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
}

void Ala_confirm_cb(lv_event_t * e)
{
    lv_roller_get_selected_str(ui_Rollerhour, hours_rol, sizeof(hours_rol));
    lv_roller_get_selected_str(ui_Rollerminute, min_rol, sizeof(hours_rol));
    lv_roller_get_selected_str(ui_Rollersecond, sec_rol, sizeof(min_rol));

    strcpy(AlarmTime, hours_rol); 
    strcat(AlarmTime, ":"); 
    strcat(AlarmTime, min_rol); 
    strcat(AlarmTime, ":"); 
    strcat(AlarmTime, sec_rol); 
    Serial.printf("AlarmTime: %s\n", AlarmTime);


    _ui_screen_change(&ui_ScreenClock, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_ScreenClock_screen_init);
}

void setup()
{
    Serial.begin(115200); /* prepare for possible serial debug */ 
    NVS.begin();
    NVS_Flag = false;

    WeatherRefresh_Count = 12;
    PreWeather = "晴";

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
    /* For the newest version sub board, need to set `ESP_PANEL_LCD_RGB_IO_VSYNC` to high before initialize LCD */
    pinMode(ESP_PANEL_LCD_RGB_IO_VSYNC, OUTPUT);
    digitalWrite(ESP_PANEL_LCD_RGB_IO_VSYNC, HIGH);
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
    if(NVS_Flag == false) {
        if(NVS.getInt("NVS_WifiFg") == 1) {
            st_WifiName = NVS.getString("NVS_WifiNa");
            st_WifiPassWord = NVS.getString("NVS_WifiPw");
            
            SelectedWifiName = st_WifiName.c_str();
            WifiPassword = st_WifiPassWord.c_str();

            Serial.printf("NVS: SelectedWifiName: %s\n", SelectedWifiName);
            Serial.printf("NVS: WifiPassword: %s\n", WifiPassword);
            
            WiFi.begin(SelectedWifiName, WifiPassword);
            while(WiFi.status() != WL_CONNECTED) {
                Serial.println("Wifi connecting...");
                Serial.printf("NVS: SelectedWifiName: %s\n", SelectedWifiName);
                Serial.printf("NVS: WifiPassword: %s\n", WifiPassword);
                delay(1000);
            }
            WeatherURL = GitURL(API, CITY);
            WifiConnected_Flag = true;
        } else {
            WifiConnected_Flag = false;
        }
        NVS_Flag = true;
    }

    if(WifiList_switch == true) {
        Num_Wifi = WiFi.scanNetworks();
        Serial.println("Scan done");

        if(Num_Wifi == 0) {
            Serial.println("no networks found");
        } else if(WifiList_switch == true) {
            WifiList_switch = false;
            Serial.println("WifiList_switch: false");

            Serial.println("Wifi list show:");

            _ui_flag_modify(ui_SpinnerLoadWifi, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        
            lv_port_lock(0);

            Wifi_List = lv_list_create(lv_scr_act());
            lv_obj_set_size(Wifi_List, 250, 200);
            for (int i = 0; i < Num_Wifi; i++) {
                lv_obj_t *Wifi_List_Item = lv_list_add_btn(Wifi_List, NULL, WiFi.SSID(i).c_str());
                lv_obj_set_user_data(Wifi_List_Item, (void *)WiFi.SSID(i).c_str());
                lv_obj_add_event_cb(Wifi_List_Item, WifiListClicked_cb, LV_EVENT_ALL, NULL);
            }
            lv_obj_align(Wifi_List, LV_ALIGN_CENTER, 0, 20);

            lv_port_unlock();
        }
        WiFi.scanDelete();
    }

    if(Passwordvalid_flag == true) {
        Serial.println("Wifi connecting...");
        Serial.printf("%s ",  SelectedWifiName);
        Serial.printf("%s\n", WifiPassword);
        Connected_Count++;

        lv_port_lock(0);

        if (WiFi.status() == WL_CONNECTED) {
            lv_textarea_set_text(ui_TextPassword, "");
            Serial.println("password correct: Wifi connected success");

            WeatherURL = GitURL(API, CITY);

            NVS_OK = NVS.setString("NVS_WifiNa", String(SelectedWifiName), 1);
            if(NVS_OK == false) {
                Serial.println("NVS_OK: false 1.NVS_WifiName");
            }
            Serial.printf("NVS_WifiNa:%s\n", String(SelectedWifiName));
            
            NVS_OK = NVS.setString("NVS_WifiPw", String(WifiPassword), 1);
            if(NVS_OK == false) {
                Serial.println("NVS_OK: false 2.NVS_WifiPassword");
            }
            Serial.printf("NVS_WifiPw:%s\n", String(WifiPassword));

            WifiConnected_Flag = true;
            NVS_OK = NVS.setInt("NVS_WifiFg", WifiConnected_Flag, 1);
            if(NVS_OK == false) {
                Serial.println("NVS_OK: false 3.NVS_WifiConnected_Flag");
            }
            Serial.printf("NVS_WifiFg:%d\n", WifiConnected_Flag);

            Passwordvalid_flag = false;
            Serial.println("WifiConnected_Flag: true");
            Serial.println("Passwordvalid_flag: false");

            _ui_flag_modify(ui_SpinnerLoadPassword, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
            _ui_screen_change(&ui_ScreenClock, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_ScreenClock_screen_init);
        } else if(Connected_Count >= 2) {
            lv_textarea_set_text(ui_TextPassword, "");
            Serial.println("password wrong: Wifi connected failed");
            
            WifiConnected_Flag = false;
            NVS_OK = NVS.setInt("NVS_WifiFg", WifiConnected_Flag, 1);

            Passwordvalid_flag = false;
            Serial.println("WifiConnected_Flag: false");
            Serial.println("Passwordvalid_flag: false");
            
            Connected_Count = 0;
            
            _ui_flag_modify(ui_SpinnerLoadPassword, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        }

        
        lv_port_unlock();
    }

    if(WifiConnected_Flag == true) {
        timeClient.update();
        unsigned long epochTime = timeClient.getEpochTime();
        struct tm *ptm = gmtime((time_t *)&epochTime);
        snprintf(dateString, sizeof(dateString), "%04d-%02d-%02d", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday);
        
        Serial.print("Now time: ");
        Serial.println(timeClient.getFormattedTime());
        Serial.print("Current date: ");
        Serial.println(dateString);
        Serial.print("Current day of the week: ");
        Serial.println(timeClient.getDay());

        WeatherRefresh_Count++;

        if(WeatherRefresh_Count >= 12) {
            WeatherRefresh_Count = 0;
            ParseWeather(WeatherURL);
        }
    }

    delay(5000);
}
