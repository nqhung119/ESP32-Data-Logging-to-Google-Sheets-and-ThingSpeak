#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <MQ135.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ThingSpeak.h>
#include <TimeLib.h>
#include <SimpleTimer.h>

#define DHTPIN 16
#define DHTTYPE DHT11

#define MQ135PIN 32

#define wifiStatusLed 17

char ssid[] = "QUANGHUNG";
char pass[] = "QUANGHUNG119";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200;
const int   daylightOffset_sec = 0;

String GOOGLE_SCRIPT_ID = "AKfycbx8uMuJ9MOe5hQ4kEJC1gGUbHAFd-M4L6XjNb_wHfF-YTUOLB_pZSWgFhSjzu1rDKBfJQ";

unsigned long myChannelNumber = 2133857;
const char * myWriteAPIKey = "2S5J8ZYD8BTP21JA";

float t = 0;
float h = 0;

float rzero = 0;
float correctedRZero = 0;
float resistance = 0;
float analogValue = 0;
float ppm = 0;
float correctedPPM = 0;

DHT dht(DHTPIN, DHTTYPE);
MQ135 mq135_sensor = MQ135(MQ135PIN);
LiquidCrystal_I2C lcd(0x27, 20, 4);
SimpleTimer timer;
WiFiClient client;

void connectInternet() {
  WiFi.begin(ssid, pass);
  Serial.print("Dang ket noi Wi-Fi.");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10) {
    delay(1000);
    Serial.print(".");
    retries++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(wifiStatusLed, LOW);
    Serial.println("Khong the ket noi Wi-Fi.");
  } 
  else {
    digitalWrite(wifiStatusLed, HIGH);
    Serial.println("Da ket noi Wi-Fi.");
  }
}

void getDHTDataSensor() {
  t = dht.readTemperature();
  h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("Khong nhan duoc du lieu tu cam bien DHT11.");
    return;
  }
}

void getMQDataSensor() {
  analogValue = analogRead(32);
  
  rzero = mq135_sensor.getRZero();
  correctedRZero = mq135_sensor.getCorrectedRZero(t, h);
  resistance = mq135_sensor.getResistance();
  ppm = mq135_sensor.getPPM();
  correctedPPM = mq135_sensor.getCorrectedPPM(t, h);
}

void sendGoogleSheets() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Khong lay duoc thoi gian.");
    return;
  }

  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  String asString(timeStringBuff);
  asString.replace(" ", "-");
  Serial.print("Time:");
  Serial.println(asString);
  
  String urlFinal = "https://script.google.com/macros/s/"+GOOGLE_SCRIPT_ID+"/exec?"
  + "date=" + asString 
  + "&temperature=" + String(t)
  + "&humidity=" + String(h)
  + "&ppm=" + String(correctedPPM);

  Serial.print("POST data to spreadsheet:");
  Serial.println(urlFinal);

  HTTPClient http;
  http.begin(urlFinal.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET(); 
    Serial.print("HTTP Status Code: ");
    Serial.println(httpCode);
    String payload;
    if (httpCode > 0) {
        payload = http.getString();
        Serial.println("Payload: "+payload);    
    }
  http.end();
}

void sendThingSpeak() {
  ThingSpeak.setField(1, t);
  ThingSpeak.setField(2, h);
  ThingSpeak.setField(3, correctedPPM);

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  if (x == 200) {
    Serial.println("Channel update successful.");
  }
  else {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
}

void displayDataSensor() {
  Serial.print("Nhiet do: ");
  Serial.print(t);
  Serial.print("*C");
  Serial.print("\n");
  Serial.print("Do am: ");
  Serial.print(h);
  Serial.print("%");
  Serial.print("\n");

  Serial.print("Analog value: ");
  Serial.print(analogValue);
  Serial.print("\n");
  Serial.print("PPM: ");
  Serial.print(ppm);
  Serial.print("\n");
  Serial.print("Correct PPM: ");
  Serial.print(correctedPPM);
  Serial.print("\n");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nhiet do: ");
  lcd.print(t);
  lcd.print("*C");

  lcd.setCursor(0, 1);
  lcd.print("Do am: ");
  lcd.print(h);
  lcd.print("%");

  lcd.setCursor(0, 2);
  lcd.print("Mat do: ");
  lcd.print(correctedPPM);
  lcd.print("ppm");

  if (correctedPPM >= 2000) {
    lcd.setCursor(0, 3);
    lcd.print("Khong khi o nhiem.");
  }
  else {
    lcd.setCursor(0, 3);
    lcd.print("Khong khi sach.");
  }
}

void setup() {
  Serial.begin(115200);
  connectInternet();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  lcd.init();
  lcd.backlight();
  dht.begin();
  ThingSpeak.begin(client);

  pinMode (wifiStatusLed, OUTPUT);

  timer.setInterval(1000, getDHTDataSensor);
  timer.setInterval(1000, getMQDataSensor);
  timer.setInterval(1000, sendGoogleSheets);
  timer.setInterval(1000, sendThingSpeak);
  timer.setInterval(1000, displayDataSensor);
}

void loop() {
  timer.run();
}
