#include <Arduino.h>

#include <M5Stack.h>
#include <string.h>

#define TINY_GSM_MODEM_UBLOX
#include <TinyGsmClient.h> 

TinyGsm modem(Serial2);
TinyGsmClient ctx(modem);

#include <Arduino_JSON.h>
#include <PubSubClient.h>
PubSubClient MqttClient(ctx);
const char *THING_NAME = "tof";
const char *PUB_TOPIC = "distances";
const char *SUB_TOPIC = "/iot-mayday/call";

#define LOOP_INTERNAL (50)
#define BEEP_INTERVAL (1600)
#define DISPLAY_INTERVAL (30000)

int received = false;
unsigned long receivedAt = 0;
char channel[64] = {0};
char from[32] = {0};
char ts[32] = {0};
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

void display_message(const char* from, const char* message) {
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 40);
  M5.Lcd.print("@");
  M5.Lcd.println(from);
  M5.Lcd.setCursor(0, 80);
  M5.Lcd.println(message);
  M5.Lcd.setCursor(0, 200);
  M5.Lcd.println("        OK          Later         NG        ");
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
    display_message((const char*)json["from"], (const char*)json["message"]);

    strncpy(from, json["from"], sizeof(from));
    strncpy(channel, json["channel"], sizeof(channel));
    strncpy(ts, json["ts"], sizeof(ts));

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

  // IO
  pinMode(21, OUTPUT);
  digitalWrite(21, 0);

  // font
  String f20 = "genshin-regular-20pt";
  M5.Lcd.loadFont(f20, SD);

  // network
  setup_modem();
  setup_mqtt();

  Serial.println("end setup.");
}

void start_mayday() {
  // パトランプ点灯
  digitalWrite(21, 1);

  static bool beep = false;
  beep = !beep;
  if (beep) {
    M5.Speaker.tone(1000);
  } else {
    M5.Speaker.mute();
  }
}

void stop_beep() {
  M5.Speaker.mute();
}

void end_mayday() {
  // パトランプ消灯
  digitalWrite(21, 0);

  // LCD消灯
  M5.Lcd.clear(BLACK);

  received = false;
}

void send_response(const char* response) {
  JSONVar json;
  json["response"] = response;
  json["channel"] = channel;
  json["from"] = from;
  json["ts"] = ts;

  MqttClient.publish("/iot-mayday/response", JSON.stringify(json).c_str());
  Serial.println(JSON.stringify(json).c_str());
  Serial.println("sent.");
}

void check_button() {
  Serial.println("check button.");
  if (M5.BtnA.wasPressed()) {
    Serial.println("OK");
    send_response("OK");
    end_mayday();
  } else if (M5.BtnB.wasPressed()) {
    Serial.println("Later");
    send_response("Later");
    end_mayday();
  } else if (M5.BtnC.wasPressed()) {
    Serial.println("NG");
    send_response("NG");
    end_mayday();
  }
}

void loop() {
  M5.update();

  Serial.println("begin loop.");
  check_connection();

  unsigned long start = millis();

  if (received) {
    if ((receivedAt + DISPLAY_INTERVAL) <= start) {
      end_mayday();
    } else if ((receivedAt + BEEP_INTERVAL) <= start) {
      stop_beep();
    } else {
      start_mayday();
    }
    check_button();
  }

  while (millis() < (start + LOOP_INTERNAL)) {
    MqttClient.loop();
  }

  Serial.println("end loop.");
  Serial.println("");
}
