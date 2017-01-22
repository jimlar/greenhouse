#include <deque>
#include <ESP8266WiFi.h>
#include <Servo.h>
#include <PubSubClient.h>
#include "settings.h"

#define WATER1_PIN 13
#define WATER2_PIN 15
#define TEMP1_PIN A0

boolean water1 = false;
boolean water2 = false;

unsigned long last_temp_read = 0;
int temp_read_interval_secs = 2;
std::deque<float> temp1 = {};

int door_pos = 0;
Servo door_servo;

WiFiClient espClient;
PubSubClient client(espClient);

void control_water_valves() {
    digitalWrite(WATER1_PIN, water1 ? HIGH : LOW);
    digitalWrite(WATER2_PIN, water2 ? HIGH : LOW);
}

void control_door(int new_pos) {
  door_pos = new_pos;
  float fraction = new_pos / 1024.0;
  int servo_pos = (int) (fraction * 180);
  door_servo.write(servo_pos);
}

const char* boolean_as_status(boolean b) {
  if (b) {
    return "{\"status\": \"1\"}";
  } else {
    return "{\"status\": \"0\"}";
  }
}

char* payload_to_string(byte *payload, unsigned int length) {
  char *pl = (char*) malloc(length + 1);
  for (int i = 0; i < length; i++) {
    pl[i] = (char) payload[i];
  }
  pl[length] = 0;
  return pl;
}

void publish_configs() {
  client.publish("/IoTmanager", "gh");
  client.publish("/IoTmanager/gh/config", "{\"id\":\"0\", \"descr\":\"Water 1\",\"widget\":\"toggle\",\"topic\":\"/IoTmanager/gh/water1\",\"color\":\"green\"}");
  client.publish("/IoTmanager/gh/config", "{\"id\":\"1\", \"descr\":\"Water 2\",\"widget\":\"toggle\",\"topic\":\"/IoTmanager/gh/water2\",\"color\":\"green\"}");
  client.publish("/IoTmanager/gh/config", "{\"id\":\"2\", \"descr\":\"Temperature\", \"widget\":\"small-badge\"}");
  client.publish("/IoTmanager/gh/config", "{\"id\":\"3\", \"descr\": \"Temp\", \"widget\": \"chart\", \"topic\": \"/IoTmanager/gh/temp1\", \"widgetConfig\": { \"type\": \"line\", \"maxCount\": 20}}");
  client.publish("/IoTmanager/gh/config", "{\"id\":\"4\", \"descr\":\"Door\", \"widget\":\"range\", \"topic\":\"/IoTmanager/gh/door\"}");
}

void publish_all_status() {
  client.publish("/IoTmanager/gh/water1/status", boolean_as_status(water1));
  client.publish("/IoTmanager/gh/water2/status", boolean_as_status(water2));
  for (float v: temp1) {
    client.publish("/IoTmanager/gh/temp1/status", ("{\"status\": " + String(v) + "}").c_str());
  }
  client.publish("/IoTmanager/gh/door/status", ("{\"status\": " + String(door_pos) + "}").c_str());
}

void set_water1(boolean on) {
  water1 = on;
  control_water_valves();
  client.publish("/IoTmanager/gh/water1/status", boolean_as_status(water1));
}

void set_water2(boolean on) {
  water2 = on;
  control_water_valves();
  client.publish("/IoTmanager/gh/water2/status", boolean_as_status(water2));
}

void set_door(int val) {
  control_door(val);
  client.publish("/IoTmanager/gh/door/status", ("{\"status\": " + String(val) + "}").c_str());
}

void on_message(char* topic, byte* payload, unsigned int length) {
  char *pl = payload_to_string(payload, length);
  String pls = String(pl);
  String topics = String(topic);
  Serial.println("Message arrived [" + String(topic) + "]: " + pls);

  if (pls == "HELLO") {
    publish_configs();
    publish_all_status();

  } else if (topics == "/IoTmanager/gh/water1/control") {
    set_water1(pls == "1");

  } else if (topics == "/IoTmanager/gh/water2/control") {
    set_water2(pls == "1");

  } else if (topics == "/IoTmanager/gh/door/control") {
    set_door(pls.toInt());
  }

  free(pl);
}

void on_connected() {
  client.subscribe("/IoTmanager", 1);
  client.subscribe("/IoTmanager/gh/+/control", 1);
  client.publish("/IoTmanager", "HELLO");
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker...");

    if (client.connect("greenhouse", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");

      on_connected();
    } else {
      Serial.println("failed, rc=" + String(client.state()) + " try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup_wifi() {
  Serial.print("Connecting to " + String(WIFI_SSID) + " ");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("connected as " + WiFi.localIP());
}

void setup_mqtt() {
  Serial.print("MQTT max packet size: ");
  Serial.println(MQTT_MAX_PACKET_SIZE);
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(on_message);
}

void setup_hardware() {
  last_temp_read = millis();
  pinMode(WATER1_PIN, OUTPUT);
  pinMode(WATER2_PIN, OUTPUT);
  control_water_valves();
  door_servo.attach(16);
  control_door(0);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  setup_hardware();
  setup_wifi();
  setup_mqtt();
}

void read_temp() {
  if (millis() < last_temp_read + (1000 * temp_read_interval_secs)) {
    return;
  }
  last_temp_read = millis();
  int sensor_val = analogRead(TEMP1_PIN);
  float voltage = (sensor_val / 1024.0) * 1.0;
  float temp = (voltage - 0.5) * 100;
  temp1.push_back(temp);
  if (temp1.size() > 20) {
    temp1.pop_front();
  }
  client.publish("/IoTmanager/gh/temp1/status", ("{\"status\": " + String(temp) + "}").c_str());
  /*
  Serial.print("Temp sensor val ");
  Serial.print(sensor_val);
  Serial.print(" voltage ");
  Serial.print(voltage);
  Serial.print(" degrees ");
  Serial.println(temp);
  */
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();
  read_temp();
}
