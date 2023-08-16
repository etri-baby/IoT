#include <RtcDS3231.h>
#include <Emotion_Farm.h>
#include <Servo.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <stdlib.h>
#include <stdio.h>
#include <String.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "WiFiEsp.h"
#define DHTPIN 12
#define DHTTYPE DHT11
#define SERVOPIN 9
#define LIGHTPIN 4
#define FAN_PIN 32
#define WATER_PUMP_PIN 31
#define USE_NETWORK 1
#define USE_BLUETOOTH 1
#define DEBUG 1
#define MQTTpubQos 1
#define MQTTsubQos 1
#define ACUTATOR_ON 1
#define ACTUATOR_OFF 0

String topicStr = "";
float temperature, humidity;
int angle = 0;
int get_co2_ppm = 0;
int cdsValue = 0;
int waterValue = 0;
int lightOutput = 0;
int fanOutput = 0;
int waterPumpPin = 0;
int timeout = 0;
int send_time = 0;
bool water_State = false;
unsigned water_Time = 0;
unsigned local_time = 0;
char sData[64] = {
  0x00,
};
char rData[32] = {
  0x00,
};
char nData[32] = {
  0x00,
};
int rPos = 0;
int nPos = 0;
int right = 10;
int displayToggle = 0;
static bool isLed = true;
static bool isPump = false;
static bool isFan = false;
static bool isWindow = false;

char ssid[] = "min0";
char pass[] = "12345678";
int status = WL_IDLE_STATUS;
WiFiEspServer server_f(400);
WiFiEspClient espClient;

PubSubClient client(espClient);
const char* clientName = "";
const char* mqtt_server = "129.254.174.120";

DHT dht(DHTPIN, DHTTYPE);
Servo servo;
LiquidCrystal_I2C lcd(0x27, 16, 2);
RtcDS3231<TwoWire> Rtc(Wire);

void printLCD(int col, int row, char* str) {
  for (int i = 0; i < strlen(str); i++) {
    lcd.setCursor(col + i, row);
    lcd.print(str[i]);
  }
}

void printWifiStatus() {
#if 1  // #if DEBUG
  // print the SSID of the network you're attached to
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
#endif

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  delay(10);
#if 1  // #if DEBUG
  Serial.print("IP Address: ");
  Serial.println(ip);
#endif
  char ipno2[26];
  sprintf(ipno2, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  printLCD(0, 1, ipno2);

  // print the received signal strength
  long rssi = WiFi.RSSI();

#if 1  // #if DEBUG
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
#endif
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  topicStr = topic;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  servo.attach(SERVOPIN);
  servo.detach();
  if (topicStr == "smart/farm/actuator/window") {
    Serial.println("Window!");
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '1') angle = 10;
    else angle = 80;
    servo.attach(SERVOPIN);
    servo.write(angle);
  }
  if (topicStr == "smart/farm/actuator/pump") {
    Serial.println("WaterPump!");
    if ((char)payload[0] == '1') {
      digitalWrite(WATER_PUMP_PIN, 1);
      water_State = true;
      isPump = water_State;
      if (water_State) {
        for (int i = 0; i < 2500; i++) {
          water_Time += 1;
        }
        if ((water_Time) > 2500) {
          digitalWrite(WATER_PUMP_PIN, 0);
          water_State = false;
          water_Time = 0;
          isPump = water_State;
        }
      }
    } else {
      digitalWrite(WATER_PUMP_PIN, 0);
      water_State = false;
      water_Time = 0;
      isPump = water_State;
    }
  }
  if (topicStr == "smart/farm/actuator/led") {
    if ((char)payload[0] == '1') {
      analogWrite(LIGHTPIN, 255);
      isLed = true;
    } else {
      analogWrite(LIGHTPIN, 0);
      isLed = false;
    }
  }
  if (topicStr == "smart/farm/actuator/fan") {
    if ((char)payload[0] == '1') {
      digitalWrite(FAN_PIN, 1);
      isFan = true;
    } else {
      digitalWrite(FAN_PIN, 0);
      isFan = false;
    }
  }
}
void subscribeMQTT() {
  client.subscribe("smart/farm/actuator/window", MQTTsubQos);  // 창문
  client.subscribe("smart/farm/actuator/pump", MQTTsubQos);    // 물펌프
  client.subscribe("smart/farm/actuator/led", MQTTsubQos);     // 조명
  client.subscribe("smart/farm/actuator/fan", MQTTsubQos);     // 환기팬
}

void reconnect() {
  Serial.print("Attempting MQTT connection...");
  client.setServer(mqtt_server, 1883);
  if (client.connect(mqtt_server)) {
    subscribeMQTT();
    Serial.println("Subscribe!");
  }
  client.setCallback(callback);
}

void setup_wifi() {
#if USE_NETWORK
  // initialize serial for ESP module
  Serial2.begin(9600);
  // initialize ESP module
  WiFi.init(&Serial2);

  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
#if DEBUG
    Serial.println("WiFi shield not present");
#endif
    // don't continue
    while (true)
      ;
  }

  // attempt to connect to WiFi network
  while (status != WL_CONNECTED) {
#if DEBUG
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
#endif
    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
  }
#if DEBUG
  Serial.println("You're connected to the network");
#endif
  printWifiStatus();  // display IP address on LCD
  delay(2000);

  server_f.begin();
#endif

#if DEBUG
  Serial.println("START");
#endif
}

void setup_rtc() {
  Serial.begin(115200);
  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  //--------RTC SETUP ------------
  // if you are using ESP-01 then uncomment the line below to reset the pins to
  // the available pins for SDA, SCL
  // Wire.begin(0, 2); // due to limited pins, use pin 0 and 2 for SDA, SCL

  Rtc.Begin();
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000 /* us */, true /* reset_on_timeout */);
#endif

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

  if (!Rtc.IsDateTimeValid()) {
    Serial.println("RTC lost confidence in the DateTime!");

    // following line sets the RTC to the date & time this sketch was compiled
    // it will also reset the valid flag internally unless the Rtc device is
    // having an issue

    Rtc.SetDateTime(compiled);
  }

  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) {
    Serial.println("RTC is older than compile time, updating DateTime");
    Rtc.SetDateTime(compiled);
  } else if (now > compiled) {
    Serial.println("RTC is newer than compile time, this is expected");
  } else if (now == compiled) {
    Serial.println("RTC is the same as compile time, while not expected all is still fine");
  }
}

void setup() {
  pinMode(LIGHTPIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(WATER_PUMP_PIN, OUTPUT);
  servo.attach(SERVOPIN);
  Serial.begin(9600);
  Serial1.begin(9600);
  Serial2.begin(9600);
  dht.begin();
  analogWrite(LIGHTPIN, 255);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  lcd.begin();
  lcd.backlight();
  printLCD(0, 0, "START SMART FARM");
  printLCD(0, 1, "CONNECTING...");

  lcd.createChar(0, temp);
  lcd.createChar(1, C);
  lcd.createChar(2, humi);
  lcd.createChar(3, Qmark);
  lcd.createChar(4, water);
  lcd.createChar(5, good);
  lcd.createChar(6, wind);
  lcd.createChar(7, per);

  setup_wifi();
  setup_rtc();
  //check for the presence of the shield
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  Serial.println("Callback!");

  if (client.connect(clientName)) {
    Serial.println("MQTT Clinet Connect!");
    subscribeMQTT();
    Serial.println("Subscribe!");
  }
}

char tem[20];
char hum[20];
char illuminance[20];
char soilhumidity[20];
char pubJson[512];

void loop() {
  RtcDateTime now = Rtc.GetDateTime();
  if (!client.connected()) {
    Serial.println("Mqtt Client Not Connected");
    reconnect();
  }
  client.loop();

  utoa(temperature, tem, 10);
  utoa(humidity, hum, 10);
  utoa(cdsValue, illuminance, 10);
  utoa(waterValue, soilhumidity, 10);
  sprintf(pubJson, "{\"Sensor\":{\"temperature\":%s,\"humidity\":%s,\"illuminance\":%s,\"soilhumidity\":%s},\"Actuator\":{\"led\":%d,\"fan\":%d,\"pump\":%d,\"window\":%d}}",
          tem, hum, illuminance, soilhumidity, isLed, isFan, isPump, isWindow);
  
  send_time += 1;
  if ((send_time) > 30) {
    Serial.println("Sensor Timer is ON");
    client.publish("smart/farm/message", pubJson, MQTTpubQos);
    Serial.print("temperature : ");
    Serial.println(temperature);
    Serial.print("humidity : ");
    Serial.println(humidity);
    Serial.print("illuminance : ");
    Serial.println(cdsValue);
    Serial.print("soilhumidity : ");
    Serial.println(waterValue);
    send_time = 0;
  }

  timeout += 1;
  if (timeout % 10 == 0) {
    cdsValue = analogRead(0);
    cdsValue /= 10;
    waterValue = analogRead(1);
    waterValue /= 10;
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();

    lcd.clear();
    displayToggle = !displayToggle;
    if (displayToggle == 1) {
      lcd.setCursor(0, 0);
      sprintf(sData, "%04u/%02u/%02u", now.Year(), now.Month(), now.Day());
      lcd.print(sData);
      lcd.setCursor(0, 1);
      sprintf(sData, "%02u:%02u", now.Hour(), now.Minute());
      lcd.print(sData);
    } else {
      //온도
      lcd.setCursor(0, 0);
      lcd.write(0);
      lcd.print(":");
      sprintf(sData, "%02d", (int)temperature);
      lcd.print(sData);
      lcd.write(1);

      //토양습도
      lcd.setCursor(6, 0);
      lcd.write(2);
      lcd.print(":");
      sprintf(sData, "%02d", (int)humidity);
      lcd.print(sData);
      lcd.write(7);

      //조도
      lcd.setCursor(12, 0);
      lcd.write(6);
      lcd.print(":");
      sprintf(sData, "%02d", cdsValue);
      lcd.print(sData);

      //습도
      lcd.setCursor(0, 1);
      lcd.write(4);
      lcd.print(":");
      sprintf(sData, "%02d", waterValue);
      lcd.print(sData);
      lcd.write(7);

      //이산화탄소
      lcd.setCursor(6, 1);
      sprintf(sData, "co2: %02dppm", get_co2_ppm);
      lcd.print(sData);
    }
    sprintf(sData, "{ \"temp\":%02d,\"humidity\":%02d,\"cds\":%-04d,\"water\":%-04d,\"co2\":%-04d }",
            (int)temperature, (int)humidity,
            cdsValue, waterValue, get_co2_ppm);
    Serial1.println(sData);
  }

  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
    reconnect();
    Serial.println("reconnect!");
  }
  delay(1000);
}