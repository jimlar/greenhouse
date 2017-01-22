#include <deque>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "settings.h"

#define WATER1_PIN 13
#define WATER2_PIN 15
#define WATER_COUNTER1_PIN 2
#define TEMP1_PIN A0

boolean water1 = false;
unsigned long water1_pulses = 0;
boolean water2 = false;
unsigned long water2_pulses = 0;

unsigned long last_periodic_send = 0;
int periodic_send_interval_secs = 2;
std::deque<float> temp1 = {};

WiFiClient espClient;
PubSubClient client(espClient);

void water1_pulse_received()
{
  water1_pulses++;
}


void control_water_valves() {
    if (!water1) {
      water1_pulses = 0;
    }
    if (!water2) {
      water2_pulses = 0;
    }
    digitalWrite(WATER1_PIN, water1 ? HIGH : LOW);
    digitalWrite(WATER2_PIN, water2 ? HIGH : LOW);
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
  client.publish("/IoTmanager/gh/config", "{\"id\":\"4\", \"descr\":\"Water amount 1 (milliliters)\", \"widget\":\"small-badge\"}");
  client.publish("/IoTmanager/gh/config", "{\"id\":\"5\", \"descr\": \"Water amount 1\", \"widget\": \"chart\", \"topic\": \"/IoTmanager/gh/water_amount1\", \"widgetConfig\": { \"type\": \"line\", \"maxCount\": 100}}");
  client.publish("/IoTmanager/gh/config", "{\"id\":\"6\", \"descr\":\"Water amount 2 (milliliters)\", \"widget\":\"small-badge\"}");
  client.publish("/IoTmanager/gh/config", "{\"id\":\"7\", \"descr\": \"Water amount 2\", \"widget\": \"chart\", \"topic\": \"/IoTmanager/gh/water_amount2\", \"widgetConfig\": { \"type\": \"line\", \"maxCount\": 100}}");
}

void publish_water_flow() {
    unsigned long milliliters1 = (unsigned long) (water1_pulses / 0.47);
    client.publish("/IoTmanager/gh/water_amount1/status", ("{\"status\": " + String(milliliters1) + "}").c_str());

    unsigned long milliliters2 = (unsigned long) (water2_pulses / 0.47);
    client.publish("/IoTmanager/gh/water_amount2/status", ("{\"status\": " + String(milliliters2) + "}").c_str());
}

void publish_all_status() {
  client.publish("/IoTmanager/gh/water1/status", boolean_as_status(water1));
  client.publish("/IoTmanager/gh/water2/status", boolean_as_status(water2));
  for (float v: temp1) {
    client.publish("/IoTmanager/gh/temp1/status", ("{\"status\": " + String(v) + "}").c_str());
  }
  publish_water_flow();
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
  last_periodic_send = millis();
  pinMode(WATER1_PIN, OUTPUT);
  pinMode(WATER2_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(WATER_COUNTER1_PIN), water1_pulse_received, FALLING);
  control_water_valves();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  setup_hardware();
  setup_wifi();
  setup_mqtt();
}

void publish_periodic_data() {
  if (millis() < last_periodic_send + (1000 * periodic_send_interval_secs)) {
    return;
  }
  last_periodic_send = millis();


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
  publish_water_flow();
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();
  publish_periodic_data();
}
