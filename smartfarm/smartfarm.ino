#include <Servo.h>
#include <DHT.h>
#include <PubSubClient.h>
#include "WiFiEsp.h"
#include <stdlib.h>
#include <stdio.h>
#include <String.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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
int cdcValue = 0;
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
int displayToggle = 1;

static bool isLed = true;
static bool isPump = false;
static bool isFan = false;
static bool isWindow = false;

char ssid[] = "min0_5G";
char pass[] = "12345678";

int status = WL_IDLE_STATUS;
WiFiEspServer server_f(400);
WiFiEspClient espClient;
PubSubClient client(espClient);

const char* clientName = "";
const char* mqtt_server = "129.254.174.129";

DHT dht(DHTPIN, DHTTYPE);
Servo servo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

void printLCD(int col, int row, char* str) {
  for (int i = 0; i < strlen(str); i++) {
    lcd.setCursor(col + i, row);
    lcd.print(str[i]);
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  delay(10);
  Serial.print("IP Address: ");
  Serial.println(ip);
  char ipno2[26];
  sprintf(ipno2, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  printLCD(0, 1, ipno2);

  // print the received signal strength
  long rssi = WiFi.RSSI();

  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
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
  if(topicStr == "farm/actuator/window"){
    Serial.println("Window!");
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '1') angle = 10;
        else angle = 80;
        servo.attach(SERVOPIN);
        servo.write(angle);
  }

  if(topicStr == "farm/actuator/pump"){
    Serial.println("WaterPump!");

    if ((char)payload[0] == '1') {
      digitalWrite(WATER_PUMP_PIN, 1);
      water_State = true;
      isPump = water_State;
      if(water_State){
        for(int i = 0; i < 2500; i++) {
          water_Time += 1;
        }
        if ((water_Time) > 2500) {
          digitalWrite(WATER_PUMP_PIN, 0);
          water_State = false;
          water_Time = 0;
          isPump = water_State;
        }
      }
    }
    else {
      digitalWrite(WATER_PUMP_PIN, 0);
      water_State = false;
      water_Time = 0;
      isPump = water_State;
    }
    
  }

  if(topicStr == "farm/actuator/led"){
     if ((char)payload[0] == '1') {
      analogWrite(LIGHTPIN, 255);
      isLed = true;
     }
     else{
       analogWrite(LIGHTPIN, 0);
       isLed = false;
     }
  }
  if(topicStr == "farm/actuator/fan"){
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
  client.subscribe("farm/actuator/window", MQTTsubQos); // 창문
  client.subscribe("farm/actuator/pump", MQTTsubQos); // 물펌프
  client.subscribe("farm/actuator/led", MQTTsubQos); // 조명
  client.subscribe("farm/actuator/fan", MQTTsubQos); // 환기팬
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
  status = WiFi.status();
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    Serial.print("Status: ");
    Serial.println(status);

    status = WiFi.begin(ssid, pass);
    if (status != WL_CONNECTED) {
      Serial.println("Connect Fail - call wait status");

      delay (100);
    } else {
      delay (1000);
    }
  }
}

// ------------------------------------------------------------------------------------------

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

  lcd.init();
  lcd.backlight();
  printLCD(0, 0, "MMB_SmartFarm");
  printLCD(0, 1, "NETWORKING...");

  WiFi.init(&Serial2);
  setup_wifi();
  // check for the presence of the shield
  Serial.println("You're connected to the network");

  printWifiStatus();  // display IP address on LCD

  Serial.println("START");

  client.setServer(mqtt_server, 1883); 
  client.setCallback(callback);
  Serial.println("Callback!");
  if (client.connect(clientName)) {
    Serial.println("MQTT Clinet Connect!");
    subscribeMQTT();
    Serial.println("Subscribe!");
  }
  Serial.println("START!");

}
char tem[20];
char hum[20];
char illuminance[20];
char soilhumidity[20];
char pubJson[512];

void loop() {
  if (!client.connected()) {
    Serial.println("Mqtt Client Not Connected");
    reconnect();
  }
  client.loop();

  utoa(temperature, tem, 10);
  utoa(humidity, hum, 10);
  utoa(cdcValue, illuminance, 10);
  utoa(waterValue, soilhumidity, 10);

  sprintf(pubJson, "{\"Sensor\":{\"temperature\":%s,\"humidity\":%s,\"illuminance\":%s,\"soilhumidity\":%s},\"Actuator\":{\"led\":%d,\"fan\":%d,\"pump\":%d,\"window\":%d}}", 
           tem, hum, illuminance, soilhumidity,isLed, isFan, isPump, isWindow );

  send_time += 1;
  if ((send_time) > 10) {
    Serial.println("Sensor Timer is ON");
    client.publish("farm/message", pubJson, MQTTpubQos);

    Serial.print("temperature : ");
    Serial.println(temperature);
    Serial.print("humidity : ");
    Serial.println(humidity);
    Serial.print("illuminance : ");
    Serial.println(cdcValue);
    Serial.print("soilhumidity : ");
    Serial.println(waterValue);
    send_time = 0;
  }

  timeout += 1;
  if (timeout % 10 == 0) {

    cdcValue = analogRead(0);
    cdcValue /= 10;

    waterValue = analogRead(1);
    waterValue /= 10;

    humidity = dht.readHumidity();
    temperature = dht.readTemperature();

    lcd.clear();
    displayToggle = !displayToggle;
    if (displayToggle == 1) {
      memset(sData, 0x00, 64);
      sprintf(sData, "temp %02dC humi %02d%%", (int)temperature,
              (int)humidity);
      printLCD(0, 0, sData);
      memset(sData, 0x00, 64);
      sprintf(sData, "cdc%-04d soil%-04d", cdcValue, waterValue);
      printLCD(0, 1, sData);
    } else {
      memset(sData, 0x00, 64);
      sprintf(sData, "temp %02dC humi %02d%%", (int)temperature,
              (int)humidity);
      printLCD(0, 0, sData);
      memset(sData, 0x00, 64);
      sprintf(sData, "co2 %d ppm", get_co2_ppm);
      printLCD(0, 1, sData);
    }

    sprintf(sData, "{ \"temp\":%02d,\"humidity\":%02d,\"cdc\":%-04d,\"water\":%-04d,\"co2\":%-04d }",
            (int)temperature, (int)humidity,
            cdcValue, waterValue, get_co2_ppm);
    Serial1.println(sData);
  }

// 0 이면 farm/sensor/status -> 센서1,상태 센서2,상태


  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
    reconnect();
    Serial.println("reconnect!");
  }

  delay(1000);
}