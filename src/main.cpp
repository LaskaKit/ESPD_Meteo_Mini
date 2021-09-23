/* LasKKit ESPDisplay for Weather Station. 
 * Thingspeak edition
 * Read Temperature, Humidity and pressure from Thingspeak and show on the display
 * For settings see config.h
 * 
 * Email:obchod@laskarduino.cz
 * Web:laskarduino.cz
 * 
 * in User_Setup.h set ESP32 Dev board pinout to 
 * TFT_MISO 12
 * TFT_MOSI 13
 * TFT_SCLK 14
 * TFT_CS   15
 * TFT_DC   32
 * TFT_RST   5 
 * 
 * Miles Burton DS18B20 library
 * https://github.com/milesburton/Arduino-Temperature-Control-Library
 */

#include <Arduino.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include "SPI.h"
#include "WiFi.h"
#include "ThingSpeak.h"
#include <NTPClient.h>          // date and time from NTP

#include "config_my.h"          // change to config.h and fill the file.
#include "iot_iconset_16x16.h"  // WIFI and battery icons
//#include "Fonts/GFXFF/FreeMono12pt7b.h" // Include the header file attached to this sketch


#define ADC 34                      // Battery voltage mesurement
//#define DALLAS 12
#define USE_STATIC_IP false         // if we want to use a static IP address

#define DALLAS 26
// if we have DS18B20 installed on display
#if DALLAS
  #include <OneWire.h>
  #include <DallasTemperature.h>
  OneWire oneWireDS(DALLAS);
  DallasTemperature dallas(&oneWireDS);
#endif

// if we want to use a static IP address
#if USE_STATIC_IP
  IPAddress ip(192,168,100,244);      // pick your own IP outside the DHCP range of your router
  IPAddress gateway(192,168,100,1);   // watch out, these are comma's not dots
  IPAddress subnet(255,255,255,0);
#endif  

// TFT SPI
#define TFT_LED   33                  // TFT backlight pin

#define TFT_DISPLAY_RESOLUTION_X 240
#define TFT_DISPLAY_RESOLUTION_Y 320

// Define the colors, color picker here: https://ee-programming-notepad.blogspot.com/2016/10/16-bit-color-generator-picker.html
#define TFT_TEXT_COLOR              0xFFFF  // white     0xFFFF
#define TFT_BACKGROUND_COLOR        0x00A6  // dark blue 0x00A6
#define TFT_TILE_SHADOW_COLOR       0x0000  // black     0x0000
#define TFT_TILE_BACKGROUND_COLOR_1 0x0700  // green     0x0700
#define TFT_TILE_BACKGROUND_COLOR_2 0x3314  // blue      0x3314
#define TFT_TILE_BACKGROUND_COLOR_3 0xDEC0  // yellow    0xDEC0
#define TFT_TILE_BACKGROUND_COLOR_4 0xD000  // red       0xD000
#define TFT_LED_PWM                 15     // dutyCycle 0-255 last minimum was 15

#define BOOT_MESSAGE "booting..."
#define REFRESH_RATE_MS 60*1000
#define REFRESH_RATE_DALLAS_MS 60*1000


// Konstanty pro vykresleni dlazdic
#define TILES_OFFSET_Y  60   // Tiles Y start ofset
#define TEXT_PADDING    5    // odsazeni text
#define TILE_MARGIN     10   // odsazeni dlazdicka
#define TILE_SIZE_X (TFT_DISPLAY_RESOLUTION_X - 3 * TILE_MARGIN ) / 2
#define TILE_SIZE_Y (TFT_DISPLAY_RESOLUTION_Y - TILES_OFFSET_Y - 3 * TILE_MARGIN) / 2

float temp;
float m_volt;
float temp_in;
float d_volt;
float temp_box;
int pressure;
int humidity;
int32_t wifiSignal;
String date;
uint64_t nextRefresh;
uint64_t nextRefreshDallas;
bool firstLoop = true;

//char buff[512];


WiFiClient client;
// Secify the time server pool and the offset, (+3600 in seconds, GMT +1 hour)
// !!!!!!!!!!!!!!!!!!!!!!TODO WRONG SUMMER TIME (- 1 HOUR)
// additionaly you can specify the update interval (in milliseconds). 
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
TFT_eSPI display = TFT_eSPI();       // Invoke custom library

void readChannel(){
   //---------------- Channel 1 ----------------//
  temp = ThingSpeak.readFloatField(myChannelNumber, 1, myReadAPIKey);
  Serial.print("Temperature: ");
  Serial.println(temp);
  //---------------- Channel 2 ----------------//
  pressure = ThingSpeak.readIntField(myChannelNumber, 2, myReadAPIKey);
  Serial.print("Pressure: ");
  Serial.println(pressure);
   //---------------- Channel 3 ----------------//
  humidity = ThingSpeak.readIntField(myChannelNumber, 3, myReadAPIKey);
  Serial.print("Humidity: ");
  Serial.println(humidity);
   //---------------- Channel 4 ----------------//
  m_volt = ThingSpeak.readFloatField(myChannelNumber, 4, myReadAPIKey);
  m_volt = round(m_volt*100.0)/100.0;     // round to x,xx
  Serial.print("Meteo Battery voltage: ");
  Serial.println(m_volt);
     //---------------- Channel 5 ----------------//
  temp_box = ThingSpeak.readFloatField(myChannelNumber, 5, myReadAPIKey);
  Serial.print("Temperature inside: ");
  Serial.println(temp_box);
}

// Samotna funkce pro vykresleni barevne dlazdice
void drawTile(uint8_t position, char title[], char value[], uint16_t color) {
  // Souradnie dlazdice
  uint16_t x = 0;
  uint16_t y = 0;

  // Souradnice dlazdice podle jedne ze ctyr moznych pozic (0 az 3)
  switch (position) {
    case 0:
      x = TILE_MARGIN;
      y = TILES_OFFSET_Y;
      break;
    case 1:
      x = (TILE_MARGIN * 2) + TILE_SIZE_X;
      y = TILES_OFFSET_Y;
      break;
    case 2:
      x = TILE_MARGIN;
      y = TILES_OFFSET_Y + TILE_SIZE_Y + TILE_MARGIN;
      break;
    case 3:
      x = (TILE_MARGIN * 2) + TILE_SIZE_X;
      y = TILES_OFFSET_Y + TILE_SIZE_Y + TILE_MARGIN;
      break;
  }

  display.fillRect(x, y, TILE_SIZE_X, TILE_SIZE_Y, color);

  // Vycentrovani a vykresleni title u dlazdice
  display.setTextSize(2);
  display.setTextFont(1);
  display.setTextDatum(TC_DATUM);
  display.setTextColor(TFT_TEXT_COLOR, color);
  display.drawString(title, x + ((TILE_SIZE_X / 2)), y + TEXT_PADDING);
  
  // Vycentrovani a vykresleni hlavni hodnoty
  display.setTextSize(3);
  display.setTextFont(2);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_TEXT_COLOR, color);
  display.drawString(value, x + (TILE_SIZE_X / 2), y + (TILE_SIZE_Y / 2));
}

/* Pomocne pretizene funkce pro rozliseni, jestli se jedna o blok
   s promennou celeho cisla, nebo cisla s desetinou carkou
*/
void drawTile(uint8_t position, char title[], float value, uint16_t color) {
  // Prevod ciselne hodnoty float na retezec
  char strvalue[8];
  dtostrf(value, 3, 1, strvalue);
  drawTile(position, title, strvalue, color);
}

void drawTile(uint8_t position, char title[], int value, uint16_t color) {
  // Prevod ciselne hodnoty int na retezec
  char strvalue[8];
  itoa(value, strvalue, 10);
  drawTile(position, title, strvalue, color);
}

uint8_t getWifiStrength(){
  int32_t strength = WiFi.RSSI();
  Serial.print("Wifi Strenght: " + String(strength) + "dB; ");

  uint8_t percentage;
  if(strength <= -100) {
    percentage = 0;
  } else if(strength >= -50) {  
    percentage = 100;
  } else {
    percentage = 2 * (strength + 100);
  }
  Serial.println(String(percentage) + "%");  //Signal strength in %  

  if (percentage >= 75) strength = 4;
  else if (percentage >= 50 && percentage < 75) strength = 3;
  else if (percentage >= 25 && percentage < 50) strength = 2;
  else if (percentage >= 10 && percentage < 25) strength = 1;
  else strength = 0;
  return strength;
}

uint8_t getIntBattery(){
   d_volt = analogRead(ADC);                // max 3.3V ESP32, do not use reading in ranges 0-0.1V and 3.2-3.3V
   // d_volt will be from 0 to 4095, 12bit ADC resolution
   Serial.println(d_volt);
   if (d_volt > 0) {
     // We using 100k+300k voltage divider, max voltage on battery 4.24V will equal 3.18V on ADC input
     d_volt = d_volt / 4095.0 * 4.4;    // max measured voltage will be 4.4, which equals 3.3V on ADC input
     Serial.println(String(d_volt) + "V");
   }

  // Měření napětí baterie | Battery voltage measurement

  // Simple percentage converting
  if (d_volt >= 4.0) return 5;
  else if (d_volt >= 3.8 && d_volt < 4.0) return 4;
  else if (d_volt >= 3.73 && d_volt < 3.8) return 3;
  else if (d_volt >= 3.65 && d_volt < 3.73) return 2;
  else if (d_volt >= 3.6 && d_volt < 3.65) return 1;
  else if (d_volt < 3.6) return 0;
  else return 0;
}

String getTime(){
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }

  String formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T"); // found index for "T" in String, example: 2020-12-18T13:54:20Z
  
  String dayStamp = formattedDate.substring(0, splitT);   
  String day = dayStamp.substring(8, 10);
  String month = dayStamp.substring(5, 7);
  String year = dayStamp.substring(2, 4);
  
  //Extract time
  String timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  String hour = timeStamp.substring(0, 5);
  return day + "." + month + "." + year + " " + hour;
}

void drawScreen() {

  display.fillRect(0, 0, TFT_DISPLAY_RESOLUTION_X, TILES_OFFSET_Y, TFT_BACKGROUND_COLOR); // clear top and time bars background

  // logo laskarduino
 //display.drawBitmap(TFT_DISPLAY_RESOLUTION_X/2-24, TFT_DISPLAY_RESOLUTION_Y/2-24, laskarduino_glcd_bmp, 48, 48, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR);

  // WiFi signal
  int32_t wifiSignalMax = 4;
  int32_t offset = 6;
  
  display.drawBitmap(0, 0, wifi1_icon16x16, 16, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR);  

  for (int32_t i = 1; i <= wifiSignalMax; i++)
      display.drawRect(i * offset - 6 + 18, 0, 4, 13, TFT_TEXT_COLOR);

  for (int32_t i = 1; i <= wifiSignal; i++)
      display.fillRect(i * offset - 6 + 18, 0, 4, 13, TFT_TEXT_COLOR);

  // Napeti baterie meteostanice
  String meteoBateryVoltage = "";
  meteoBateryVoltage = String(m_volt,2)  + "v";
  display.setTextSize(2); 
  display.setTextFont(1); 
  display.setTextColor(TFT_WHITE, TFT_BACKGROUND_COLOR);
  display.setTextDatum(TC_DATUM);
  display.drawString(meteoBateryVoltage, TFT_DISPLAY_RESOLUTION_X / 2, 0);
 
  // Napeti baterie
  uint8_t intBatteryPercentage = getIntBattery();
  switch (intBatteryPercentage) {
    case 5:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_100, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR
);
      break;
     case 4:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_80, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR
);
      break;
    case 3:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_60, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR
);
      break;
    case 2:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_40, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR
);
      break;
     case 1:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_20, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR
);
      break;
    case 0:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_0, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR
);
      break;
    default:
    break;
  }

  // datum a cas
  display.setTextSize(2);
  display.setTextFont(2);
  display.setTextColor(TFT_WHITE, TFT_BACKGROUND_COLOR);
  display.setTextDatum(MC_DATUM);
  display.drawString(date, TFT_DISPLAY_RESOLUTION_X / 2, 35);

  //draw squares
  drawTile(0, "Tout,`C", temp, TFT_TILE_BACKGROUND_COLOR_1);
  drawTile(1, "Vlh,%", humidity, TFT_TILE_BACKGROUND_COLOR_2);
  drawTile(2, "Tl,hPa", pressure, TFT_TILE_BACKGROUND_COLOR_3);
  drawTile(3, "Tin,`C", temp_in, TFT_TILE_BACKGROUND_COLOR_4);
  
}

void WiFiConnection(){
  // pripojeni k WiFi
  // Connecting to last using WiFi

  Serial.println();
  Serial.print("Connecting to...");
  Serial.println(ssid);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.print("Connecting to... ");
  display.println(ssid);

  #if USE_STATIC_IP
    if (!WiFi.config(ip, gateway, subnet)) {
      Serial.println("STA Failed to configure");
      display.println("STA Failed to configure");
    }
  #endif
  WiFi.begin(ssid, pass);

  int i = 0;
  int a = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    a++;
    i++;
    if (i == 10) {
      i = 0;
      Serial.println(".");
      display.println(".");
    } else {
      Serial.print("."); 
      display.print(".");
    }
  }
  Serial.println("");
  Serial.println("Wi-Fi connected successfully");
  display.println("Wi-Fi connected successfully");
}

void getDallas(){
  #if DALLAS
    dallas.requestTemperatures();
    temp_in = dallas.getTempCByIndex(0); // (x) - pořadí dle unikátní adresy čidel
    Serial.print("Temp_in: "); Serial.print(temp_in); Serial.println(" °C");
  #else
    temp_in = 0;
    Serial.println("No dallas was defined");
  #endif
}


void setup() {

  Serial.begin(9600);
  while(!Serial) {} // Wait until serial is ok

  pinMode(ADC, INPUT);
  
  // configure backlight LED PWM functionalitites
  ledcSetup(0, 5000, 8);              // ledChannel, freq, resolution
  ledcAttachPin(TFT_LED, 0);          // ledPin, ledChannel
  ledcWrite(1, TFT_LED_PWM);          // dutyCycle 0-255
  
  display.begin();
  display.setRotation(0);
  display.fillScreen(TFT_BLACK);
  display.setTextSize(1);
  display.setTextFont(1);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.setTextDatum(MC_DATUM);  

  display.drawString(BOOT_MESSAGE, display.width() / 2, display.height() / 2);

  // pripojeni k WiFi
  WiFiConnection();

  #if DALLAS 
    dallas.begin();
  #endif
  ThingSpeak.begin(client);
  timeClient.begin();

}

void loop() {
  
  uint16_t t_x = 0, t_y = 0; // To store the touch coordinates
  // Pressed will be set true is there is a valid touch on the screen
 //boolean pressed = display.getTouch(&t_x, &t_y);
 // if (pressed) { 
 //   display.setTextDatum(TC_DATUM);
 //   display.drawString("TOUCH", t_y, t_x);
 // }

  if (millis() > nextRefreshDallas) {
    nextRefreshDallas = millis() + REFRESH_RATE_DALLAS_MS;
    getDallas();
  }

  // Zmer hodnoty, prekresli displej a opakuj za nextRefresh milisekund
  if (millis() > nextRefresh) {
    nextRefresh = millis() + REFRESH_RATE_MS;

    getDallas();
    wifiSignal = getWifiStrength();
    date = getTime();
    readChannel();

    if (firstLoop){
      display.fillScreen(TFT_BACKGROUND_COLOR);
      firstLoop = false;
    }
    drawScreen();
  }
}