#include <deque>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <U8g2lib.h>
#include <DHT.h>

#include "settings.h"

#define PULSES_PER_LITER 485
#define WATER1_PIN D8
#define WATER2_PIN D5
#define WATER_COUNTER1_PIN D3
#define WATER_COUNTER2_PIN D7
#define DHTPIN D6


// Wemos mini d1 oled shield
U8G2_SSD1306_64X48_ER_F_SW_I2C u8g2(U8G2_R0, SCL, SDA);

// Init DHT
DHT dht(DHTPIN, DHT21);

volatile boolean water1 = false;
volatile uint32_t water1_pulses = 0;
volatile uint32_t last_counter1_flowratetimer = 0;
volatile float counter1_flowrate = 0;

volatile boolean water2 = false;
volatile uint32_t  water2_pulses = 0;
volatile uint32_t last_counter2_flowratetimer = 0;
volatile float counter2_flowrate = 0;

volatile uint32_t last_periodic_send = 0;
int periodic_send_interval_secs = 2;

std::deque<float> temp1 = {};
std::deque<float> hum1 = {};

WiFiClient espClient;
PubSubClient client(espClient);

String status_wifi = "down";
String status_mqtt = "down";

void redraw_display() {
    u8g2.firstPage();
    do {
      u8g2.setCursor(0, 10);
      u8g2.print("WIFI: " + status_wifi);
      u8g2.setCursor(0, 10 * 2);
      u8g2.print("MQTT: " + status_mqtt);

      u8g2.setCursor(0, 10 * 3);
      if (water1)
        u8g2.print("WATER1: ON");
      else
        u8g2.print("WATER1: OFF");

      u8g2.setCursor(0, 10 * 4);
      if (water2)
        u8g2.print("WATER2: ON");
      else
        u8g2.print("WATER2: OFF");

    } while ( u8g2.nextPage() );
}

void control_water_valves() {
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

boolean water_graphs = false;

void publish_configs() {
  client.publish("/IoTmanager", "gh");
  client.publish("/IoTmanager/gh/config", "{\"id\":\"0\", \"descr\":\"Water 1\",\"widget\":\"toggle\",\"topic\":\"/IoTmanager/gh/water1\",\"color\":\"green\"}");
  if (water_graphs) {
    client.publish("/IoTmanager/gh/config", "{\"id\":\"1\", \"descr\":\"Water amount 1 (liters)\", \"widget\":\"small-badge\"}");
    client.publish("/IoTmanager/gh/config", "{\"id\":\"2\", \"descr\": \"Water amount 1\", \"widget\": \"chart\", \"topic\": \"/IoTmanager/gh/water_amount1\", \"widgetConfig\": { \"type\": \"line\", \"maxCount\": 100}}");
  } else {
    client.publish("/IoTmanager/gh/config", "{\"id\":\"1\", \"descr\": \"Litres since on\", \"widget\": \"anydata\", \"topic\": \"/IoTmanager/gh/water_amount1\",\"class1\":\"item no-border\",\"class3\":\"assertive\",\"style2\": \"font-size:16px;float:left\", \"style3\":\"font-size:20px;font-weight:bold;float:right\"}");
    client.publish("/IoTmanager/gh/config", "{\"id\":\"2\", \"descr\": \"Flowrate (l/m)\", \"widget\": \"anydata\", \"topic\": \"/IoTmanager/gh/water_flowrate1\",\"class1\":\"item no-border\",\"class3\":\"assertive\",\"style2\": \"font-size:16px;float:left\", \"style3\":\"font-size:20px;font-weight:bold;float:right\"}");
  }
  client.publish("/IoTmanager/gh/config", "{\"id\":\"3\", \"descr\":\"Water 2\",\"widget\":\"toggle\",\"topic\":\"/IoTmanager/gh/water2\",\"color\":\"green\"}");

  if (water_graphs) {
    client.publish("/IoTmanager/gh/config", "{\"id\":\"4\", \"descr\":\"Water amount 2 (liters)\", \"widget\":\"small-badge\"}");
    client.publish("/IoTmanager/gh/config", "{\"id\":\"5\", \"descr\": \"Water amount 2\", \"widget\": \"chart\", \"topic\": \"/IoTmanager/gh/water_amount2\", \"widgetConfig\": { \"type\": \"line\", \"maxCount\": 100}}");
  } else {
    client.publish("/IoTmanager/gh/config", "{\"id\":\"4\", \"descr\": \"Litres since on\", \"widget\": \"anydata\", \"topic\": \"/IoTmanager/gh/water_amount2\",\"class1\":\"item no-border\",\"class3\":\"assertive\",\"style2\": \"font-size:16px;float:left\", \"style3\":\"font-size:20px;font-weight:bold;float:right\"}");
    client.publish("/IoTmanager/gh/config", "{\"id\":\"5\", \"descr\": \"Flowrate (l/m)\", \"widget\": \"anydata\", \"topic\": \"/IoTmanager/gh/water_flowrate2\",\"class1\":\"item no-border\",\"class3\":\"assertive\",\"style2\": \"font-size:16px;float:left\", \"style3\":\"font-size:20px;font-weight:bold;float:right\"}");
  }

  client.publish("/IoTmanager/gh/config", "{\"id\":\"6\", \"descr\":\"Temperature (C)\", \"widget\":\"small-badge\"}");
  client.publish("/IoTmanager/gh/config", "{\"id\":\"7\", \"descr\": \"Temp\", \"widget\": \"chart\", \"topic\": \"/IoTmanager/gh/temp1\", \"widgetConfig\": { \"type\": \"line\", \"maxCount\": 20, \"height\": \"70%\"}}");

  client.publish("/IoTmanager/gh/config", "{\"id\":\"8\", \"descr\":\"Humidity (%)\", \"widget\":\"small-badge\"}");
  client.publish("/IoTmanager/gh/config", "{\"id\":\"9\", \"descr\": \"Hum\", \"widget\": \"chart\", \"topic\": \"/IoTmanager/gh/hum1\", \"widgetConfig\": { \"type\": \"line\", \"maxCount\": 20, \"height\": \"70%\"}}");
}

void publish_water_flow() {
    float liters1 = (float) water1_pulses / PULSES_PER_LITER;
    client.publish("/IoTmanager/gh/water_amount1/status", ("{\"status\": " + String(liters1) + "}").c_str());
    client.publish("/IoTmanager/gh/water_flowrate1/status", ("{\"status\": " + String(counter1_flowrate) + "}").c_str());

    float liters2 = (float) water2_pulses / PULSES_PER_LITER;
    client.publish("/IoTmanager/gh/water_amount2/status", ("{\"status\": " + String(liters2) + "}").c_str());
    client.publish("/IoTmanager/gh/water_flowrate2/status", ("{\"status\": " + String(counter2_flowrate) + "}").c_str());
}

void publish_all_status() {
  client.publish("/IoTmanager/gh/water1/status", boolean_as_status(water1));
  client.publish("/IoTmanager/gh/water2/status", boolean_as_status(water2));
  for (float v: temp1) {
    client.publish("/IoTmanager/gh/temp1/status", ("{\"status\": " + String(v) + "}").c_str());
  }
  for (float v: hum1) {
    client.publish("/IoTmanager/gh/hum1/status", ("{\"status\": " + String(v) + "}").c_str());
  }
  publish_water_flow();
}

void set_water1(boolean on) {
  if (on && !water1) {
    water1_pulses = 0;
  }
  if (!on && water1) {
    counter1_flowrate = 0;
  }
  water1 = on;
  control_water_valves();
  client.publish("/IoTmanager/gh/water1/status", boolean_as_status(water1));
}

void set_water2(boolean on) {
  if (on && !water2) {
    water2_pulses = 0;
  }
  if (!on && water2) {
    counter2_flowrate = 0;
  }
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
  redraw_display();
}

void on_connected() {
  client.subscribe("/IoTmanager", 1);
  client.subscribe("/IoTmanager/gh/+/control", 1);
  client.publish("/IoTmanager", "HELLO");
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.println("Connecting MQTT");
    status_mqtt = "...";
    redraw_display();

    if (client.connect("greenhouse", MQTT_USER, MQTT_PASSWORD)) {
      status_mqtt = "OK";
      redraw_display();
      Serial.println("MQTT connected");

      on_connected();
    } else {
      status_mqtt = "FAIL";
      redraw_display();
      Serial.println("MQTT connect failed, will retry in 5 seconds");
      delay(5000);
    }
  }
}

void setup_wifi() {
  Serial.println("Starting wifi...");
  status_wifi = "...";
  redraw_display();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    redraw_display();
    delay(100);
    i++;
  }

  status_wifi = "OK";
  redraw_display();
  Serial.println("Wifi started");
}

void setup_mqtt() {
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(on_message);
}

void water1_pulse_received() {
  float time_since_last_pulse_us = (ESP.getCycleCount() - last_counter1_flowratetimer) / clockCyclesPerMicrosecond();
  float counter1_flowrate_hz = 1000000.0 / time_since_last_pulse_us;

  // See: https://www.adafruit.com/products/833
  counter1_flowrate = (counter1_flowrate_hz + 3) / (PULSES_PER_LITER / 60);
  water1_pulses++;
  last_counter1_flowratetimer = ESP.getCycleCount();
}

void water2_pulse_received() {
  float time_since_last_pulse_us = (ESP.getCycleCount() - last_counter2_flowratetimer) / clockCyclesPerMicrosecond();
  float counter2_flowrate_hz = 1000000.0 / time_since_last_pulse_us;

  // See: https://www.adafruit.com/products/833
  counter2_flowrate = (counter2_flowrate_hz + 3) / (PULSES_PER_LITER / 60);
  water2_pulses++;
  last_counter2_flowratetimer = ESP.getCycleCount();
}

void setup_hardware() {
  dht.begin();

  last_periodic_send = millis();
  pinMode(WATER1_PIN, OUTPUT);
  pinMode(WATER2_PIN, OUTPUT);
  control_water_valves();

  pinMode(WATER_COUNTER1_PIN, INPUT_PULLUP);
  pinMode(WATER_COUNTER2_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WATER_COUNTER1_PIN), water1_pulse_received, FALLING);
  attachInterrupt(digitalPinToInterrupt(WATER_COUNTER2_PIN), water2_pulse_received, FALLING);
}


void setup_display() {
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  redraw_display();
}

void setup() {
  setup_display();
  Serial.begin(115200);
  Serial.println("");
  setup_hardware();
  setup_wifi();
  setup_mqtt();
}

void publish_periodic_data() {
  if (millis() < last_periodic_send + (1000 * periodic_send_interval_secs)) {
    return;
  }
  last_periodic_send = millis();

  float temp = dht.readTemperature();
  if (!isnan(temp)) {
    temp1.push_back(temp);
    if (temp1.size() > 20) {
      temp1.pop_front();
    }
    client.publish("/IoTmanager/gh/temp1/status", ("{\"status\": " + String(temp) + "}").c_str());
  }

  float hum = dht.readHumidity();
  if (!isnan(hum)) {
    hum1.push_back(hum);
    if (hum1.size() > 20) {
      hum1.pop_front();
    }
    client.publish("/IoTmanager/gh/hum1/status", ("{\"status\": " + String(hum) + "}").c_str());
  }

  publish_water_flow();
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();
  publish_periodic_data();
}
