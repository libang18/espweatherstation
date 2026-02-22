/**
 * ESP32 Weather Station for GC9A01 Circular Display
 * Features: LVGL UI, WiFiManager with custom parameters, 
 * Open-Meteo API integration, and non-blocking Config Portal.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h> 
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <Preferences.h>
#include <vector>

// --- Resources (Must be enabled in lv_conf.h) ---
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_48);

// --- Display Settings (GC9A01 240x240) ---
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 240;
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

// --- UI Objects ---
lv_obj_t * ui_LoadingScreen;
lv_obj_t * ui_MainScreen;
lv_obj_t * label_city;
lv_obj_t * label_temp;
lv_obj_t * label_details;
lv_obj_t * label_icon;
lv_obj_t * label_wifi; 

// --- Global Variables ---
char wm_city_name[100] = "Prague"; 
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 15 * 60 * 1000; // Update every 15 minutes
Preferences preferences;

// --- WiFiManager Settings ---
WiFiManager wm;
WiFiManagerParameter* custom_city;
bool shouldSaveConfig = false;
bool portalActive = false;
unsigned long portalStartTime = 0;
const unsigned long PORTAL_TIMEOUT = 120000; // 2 minutes auto-close

// --- Callbacks ---
void saveConfigCallback() { shouldSaveConfig = true; }
void saveParamsCallback() { shouldSaveConfig = true; }

// --- LVGL Flush Callback (Bridge between LVGL and TFT_eSPI) ---
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp_drv);
}

// --- Helper: Remove Diacritics for API compatibility ---
String removeAccents(String s) {
  s.replace("á", "a"); s.replace("é", "e"); s.replace("í", "i"); s.replace("ó", "o"); s.replace("ú", "u"); s.replace("ů", "u"); s.replace("ý", "y");
  s.replace("č", "c"); s.replace("ď", "d"); s.replace("ě", "e"); s.replace("ň", "n"); s.replace("ř", "r"); s.replace("š", "s"); s.replace("ť", "t"); s.replace("ž", "z");
  s.replace("Á", "A"); s.replace("É", "E"); s.replace("Í", "I"); s.replace("Ó", "O"); s.replace("Ú", "U"); s.replace("Ů", "U"); s.replace("Ý", "Y");
  s.replace("Č", "C"); s.replace("Ď", "D"); s.replace("Ě", "E"); s.replace("Ň", "N"); s.replace("Ř", "R"); s.replace("Š", "S"); s.replace("Ť", "T"); s.replace("Ž", "Z");
  return s;
}

// --- Weather Code Mapping (WMO Standards) ---
String getWeatherDesc(int code) {
  switch (code) {
    case 0: return "Clear Sky";
    case 1: case 2: case 3: return "Mainly Cloudy";
    case 45: case 48: return "Foggy";
    case 51: case 53: case 55: return "Drizzle";
    case 61: case 63: case 65: return "Rainy";
    case 71: case 73: case 75: return "Snowy";
    case 95: case 96: case 99: return "Stormy";
    default: return "Unknown";
  }
}

String urlEncode(String str) {
  String encodedString = "";
  char c, code0, code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') { encodedString += "%20"; } 
    else if (isalnum(c)) { encodedString += c; } 
    else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encodedString += '%'; encodedString += code0; encodedString += code1;
    }
  }
  return encodedString;
}

// --- Fetch Data from APIs ---
void updateWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client;
  client.setInsecure(); 
  HTTPClient http;

  // 1. Geocoding: Get Lat/Lon from City Name (Nominatim API)
  String addressQuery = urlEncode(String(wm_city_name));
  String geoUrl = "https://nominatim.openstreetmap.org/search?q=" + addressQuery + "&format=json&limit=1";
  
  http.begin(client, geoUrl);
  http.addHeader("User-Agent", "ESP32-Weather-Station/1.0"); 
  if (http.GET() == 200) {
    DynamicJsonDocument doc(4096); 
    deserializeJson(doc, http.getString());
    if (doc.size() > 0) {
      String latStr = doc[0]["lat"];
      String lonStr = doc[0]["lon"];

      // 2. Weather: Get current weather (Open-Meteo API)
      String weatherUrl = "https://api.open-meteo.com/v1/forecast?latitude=" + latStr + "&longitude=" + lonStr + "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code";
      
      http.begin(client, weatherUrl);
      if (http.GET() == 200) {
        String weatherData = http.getString();
        DynamicJsonDocument wDoc(2048);
        deserializeJson(wDoc, weatherData);
        
        float temp = wDoc["current"]["temperature_2m"];
        int humidity = wDoc["current"]["relative_humidity_2m"];
        float wind = wDoc["current"]["wind_speed_10m"];
        int code = wDoc["current"]["weather_code"];
        
        lv_label_set_text(label_city, removeAccents(String(wm_city_name)).c_str()); 
        lv_label_set_text_fmt(label_temp, "%.1f°C", temp);
        lv_label_set_text_fmt(label_details, "%d%% | %.1f km/h", humidity, wind);
        lv_label_set_text(label_icon, getWeatherDesc(code).c_str());
      }
    }
  }
  http.end();
}

// --- UI Construction: Initial Loading Screen ---
void buildLoadingScreen() {
  ui_LoadingScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(ui_LoadingScreen, lv_color_hex(0x000000), 0);

  lv_obj_t * load_icon = lv_label_create(ui_LoadingScreen);
  lv_label_set_text(load_icon, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_font(load_icon, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(load_icon, lv_color_hex(0x38BDF8), 0);
  lv_obj_align(load_icon, LV_ALIGN_CENTER, 0, -30);

  lv_obj_t * load_label = lv_label_create(ui_LoadingScreen);
  lv_label_set_text(load_label, "CONNECTING...");
  lv_obj_set_style_text_font(load_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(load_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(load_label, LV_ALIGN_CENTER, 0, 30);

  lv_scr_load(ui_LoadingScreen);
}

// --- UI Construction: Main Dashboard ---
void buildMainScreen() {
  ui_MainScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(ui_MainScreen, lv_color_hex(0x000000), 0);

  label_wifi = lv_label_create(ui_MainScreen);
  lv_label_set_text(label_wifi, LV_SYMBOL_WIFI); 
  lv_obj_set_style_text_font(label_wifi, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x444444), 0); 
  lv_obj_align(label_wifi, LV_ALIGN_TOP_MID, 0, 15);

  label_city = lv_label_create(ui_MainScreen);
  lv_label_set_text(label_city, "");
  lv_obj_set_style_text_color(label_city, lv_color_hex(0xAAAAAA), 0); 
  lv_obj_set_style_text_font(label_city, &lv_font_montserrat_20, 0);  
  lv_obj_set_style_text_align(label_city, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(label_city, 200);
  lv_obj_align(label_city, LV_ALIGN_TOP_MID, 0, 45);

  label_temp = lv_label_create(ui_MainScreen);
  lv_label_set_text(label_temp, "--.-°C");
  lv_obj_set_style_text_color(label_temp, lv_color_hex(0xFFD700), 0); 
  lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_48, 0);  
  lv_obj_align(label_temp, LV_ALIGN_CENTER, 0, -10);

  label_details = lv_label_create(ui_MainScreen);
  lv_obj_set_style_text_color(label_details, lv_color_hex(0x666666), 0); 
  lv_obj_set_style_text_font(label_details, &lv_font_montserrat_20, 0);  
  lv_obj_align(label_details, LV_ALIGN_CENTER, 0, 30);

  label_icon = lv_label_create(ui_MainScreen);
  lv_obj_set_style_text_color(label_icon, lv_color_hex(0x38BDF8), 0); 
  lv_obj_set_style_text_font(label_icon, &lv_font_montserrat_20, 0);  
  lv_obj_align(label_icon, LV_ALIGN_BOTTOM_MID, 0, -35);
}

void setup() {
  Serial.begin(115200);
  delay(1000); 

  // Load saved city name
  preferences.begin("meteo-app", false);
  String savedCity = preferences.getString("city", "Prague");
  strncpy(wm_city_name, savedCity.c_str(), sizeof(wm_city_name));
  preferences.end();

  // Hardware Init
  tft.begin(); tft.setRotation(0); tft.fillScreen(TFT_BLACK); 
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth; disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush; disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  buildLoadingScreen();
  lv_timer_handler();
  buildMainScreen();

  // WiFiManager Configuration
  std::vector<const char *> menu = {"wifi", "param", "info", "restart", "exit"};
  wm.setMenu(menu);
  
  // Custom Script: Rename 'Setup' button to 'Location Settings'
  wm.setCustomHeadElement("<script>window.addEventListener('load', function() { var b = document.getElementsByTagName('button'); for(var i=0;i<b.length;i++) { if(b[i].innerText=='Setup') b[i].innerText='Location Settings'; } });</script>");
  
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setSaveParamsCallback(saveParamsCallback); 
  wm.setConfigPortalBlocking(false); 
  
  custom_city = new WiFiManagerParameter("city", "City / Address", wm_city_name, 100);
  wm.addParameter(custom_city);

  WiFi.begin(); 
  unsigned long startConnectAttempt = millis();
  while(WiFi.status() != WL_CONNECTED && (millis() - startConnectAttempt < 10000)) {
    lv_timer_handler(); 
    delay(50); 
  }

  if(WiFi.status() == WL_CONNECTED) {
    updateWeather();
    lastWeatherUpdate = millis();
  }

  lv_scr_load(ui_MainScreen);

  // Start the Access Point for configuration
  wm.startConfigPortal("Meteo-Station-Setup");
  portalActive = true; portalStartTime = millis();
  
  lv_label_set_text(label_wifi, LV_SYMBOL_WIFI " AP");
  lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x38BDF8), 0); 
}

void loop() {
  lv_timer_handler();
  delay(5);

  // Handle WiFiManager non-blocking portal
  if(portalActive) {
    wm.process(); 
    if((millis() - portalStartTime > PORTAL_TIMEOUT) && (WiFi.softAPgetStationNum() == 0)) {
      WiFi.softAPdisconnect(true); portalActive = false;
      lv_label_set_text(label_wifi, LV_SYMBOL_WIFI);
      lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x444444), 0);
    }
  }

  // Save city if changed in portal and restart
  if (shouldSaveConfig) {
    strcpy(wm_city_name, custom_city->getValue());
    preferences.begin("meteo-app", false);
    preferences.putString("city", wm_city_name);
    preferences.end();
    delay(500); ESP.restart(); 
  }

  // Periodic weather update
  if (millis() - lastWeatherUpdate > weatherInterval) {
    updateWeather();
    lastWeatherUpdate = millis();
  }
}
