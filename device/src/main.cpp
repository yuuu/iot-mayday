#include <Arduino.h>

#include <M5Stack.h>

#define TINY_GSM_MODEM_UBLOX
#include <TinyGsmClient.h> 

TinyGsm modem(Serial2);
TinyGsmClient ctx(modem);

#include <Arduino_JSON.h>
#include <PubSubClient.h>
PubSubClient MqttClient(ctx);
const char *THING_NAME = "tof";
const char *PUB_TOPIC = "distances";
const char *SUB_TOPIC = "/iot-mayday";

#define LOOP_INTERNAL (50)
#define BEEP_INTERVAL (1600)
#define DISPLAY_INTERVAL (30000)

int received = false;
unsigned long receivedAt = 0;
int beep = false;

void setup_modem() {
  M5.Lcd.print("modem.restart()");
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  modem.restart();
  M5.Lcd.println("done");

  M5.Lcd.print("getModemInfo:");
  String modemInfo = modem.getModemInfo();
  M5.Lcd.println(modemInfo);

  M5.Lcd.print("waitForNetwork()");
  while (!modem.waitForNetwork()) M5.Lcd.print(".");
  M5.Lcd.println("Ok");

  M5.Lcd.print("gprsConnect(soracom.io)");
  modem.gprsConnect("soracom.io", "sora", "sora");
  M5.Lcd.println("done");

  M5.Lcd.print("isNetworkConnected()");
  while (!modem.isNetworkConnected()) M5.Lcd.print(".");
  M5.Lcd.println("Ok");

  M5.Lcd.print("My IP addr: ");
  IPAddress ipaddr = modem.localIP();
  M5.Lcd.print(ipaddr);
  delay(2000);
  M5.Lcd.fillScreen(BLACK);
}

void send_distance(uint16_t distance) {
  char payload[512];
  sprintf(payload, "{\"distance\":%u}", distance);
  char topic[256];
  sprintf(topic, "%s/%s", PUB_TOPIC, modem.getIMSI().c_str());
  Serial.print("topic>");
  Serial.println(topic);
  MqttClient.publish(topic, payload);
  Serial.println(payload);
  Serial.println("sent.");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("subscribed.");

  String buf_t = String(topic);
  payload[length] = '\0'; /* https://hawksnowlog.blogspot.com/2017/06/convert-byte-array-to-string.html */
  String buf_p = String((char*) payload);

  Serial.print("Incoming topic: ");
  Serial.println(buf_t);
  Serial.print("Payload>");
  Serial.println(buf_p);

  JSONVar json = JSON.parse((char*) payload);
  if(json.hasOwnProperty("message")){
    M5.Lcd.fillRect(0, 70, 319, 90, BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(0, 100);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.println((const char*)json["message"]);
    receivedAt = millis();
    received = true;
  }
}

void setup_mqtt() {
  Serial.print("ThingName(mqtt_id): ");
  Serial.println(THING_NAME);
  MqttClient.setServer("beam.soracom.io", 1883);
  MqttClient.setCallback(callback);
  if (!MqttClient.connect(THING_NAME)) {
    Serial.println(MqttClient.state());
  }
  Serial.print("topic>");
  Serial.println(SUB_TOPIC);
  MqttClient.subscribe(SUB_TOPIC);
}

void check_connection() {
  if (!MqttClient.connected()) {
    // disconnect
    MqttClient.disconnect();
    modem.restart();

    // connect
    setup_modem();
    setup_mqtt();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("begin setup.");
  M5.begin();
  M5.Power.begin();
  Wire.begin();
  String f20 = "genshin-regular-20pt";
  M5.Lcd.loadFont(f20, SD);
  setup_modem();
  setup_mqtt();
  Serial.println("end setup.");
}

void loop() {
  M5.update();

  Serial.println("begin loop.");

  unsigned long start = millis();

  if ((receivedAt + DISPLAY_INTERVAL) <= start) {
    M5.Lcd.clear(BLACK);
  }
  if (received) {
    if ((receivedAt + BEEP_INTERVAL) <= start) {
      received = false;
      M5.Speaker.mute();
    } else {
      beep = !beep;
      if (beep) {
        M5.Speaker.tone(1000);
      } else {
        M5.Speaker.mute();
      }
    }
  }

  while (millis() < (start + LOOP_INTERNAL)) {
    MqttClient.loop();
  }

  Serial.println("end loop.");
  Serial.println("");
}
