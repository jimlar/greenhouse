import paho.mqtt.client as mqtt
import json
import random
import argparse

snoop_only = True

def on_connect(client, userdata, flags, rc):
    print("Connected with result code %s" % rc)
    if not snoop_only:
        client.subscribe("/IoTmanager")
        client.subscribe("/IoTmanager/+/config")
        client.subscribe("/IoTmanager/+/+/status")
        client.subscribe("/IoTmanager/+/+/control")
        client.publish("/IoTmanager", "HELLO")
    else:
        client.subscribe("/IoTmanager/#")

def on_message(client, userdata, msg):
    print(msg.topic + " " + str(msg.payload))
    if not snoop_only:
        if "HELLO" == str(msg.payload):
            client.publish("/IoTmanager", "fake")

            #Water on/off
            client.publish("/IoTmanager/fake/config", json.dumps({"id":"5", "descr":"Water 1","widget":"toggle","topic":"/IoTmanager/fake/water1","color":"green"}))
            client.publish("/IoTmanager/fake/water1/status", json.dumps({"status": "0"}))

            client.publish("/IoTmanager/fake/config", json.dumps({"id":"6", "descr":"Water 2","widget":"toggle","topic":"/IoTmanager/fake/water2","color":"green"}))
            client.publish("/IoTmanager/fake/water2/status", json.dumps({"status": "0"}))

            #temp
            client.publish("/IoTmanager/fake/config", json.dumps({"id":"0", "descr":"Temperature", "widget":"small-badge"}))
            client.publish("/IoTmanager/fake/config", json.dumps({"id": "1",
                                                                  "widget": "chart",
                                                                  "descr": "Temp",
                                                                  "topic": "/IoTmanager/fake/temp0",
                                                                  "widgetConfig": {
                                                                    "type": "line",
                                                                    "maxCount": 20,
                                                                    "height": "50%",
                                                                  }}))
            for x in range(20):
                client.publish("/IoTmanager/fake/temp0/status", json.dumps({"status": random.randint(17, 27)}))

            #moist
            client.publish("/IoTmanager/fake/config", json.dumps({"id":"2", "descr":"Moisture", "widget":"small-badge"}))
            client.publish("/IoTmanager/fake/config", json.dumps({"id": "3",
                                                                  "widget": "chart",
                                                                  "descr": "Moisture",
                                                                  "topic": "/IoTmanager/fake/moist0",
                                                                  "widgetConfig": {
                                                                    "type": "line",
                                                                    "maxCount": 20,
                                                                    "height": "50%",
                                                                  }}))
            for x in range(20):
                client.publish("/IoTmanager/fake/moist0/status", json.dumps({"status": random.randint(60, 63)}))

            # Water control
            #client.publish("/IoTmanager/fake/config", json.dumps({"id":"4", "descr":"Watering", "widget":"range", "topic":"/IoTmanager/fake/water0", "style":"range-calm", "badge":"badge-assertive", "leftIcon":"ion-ios-rainy-outline", "rightIcon":"ion-ios-rainy"}))
            #client.publish("/IoTmanager/fake/water0/status", json.dumps({"status": random.randint(0, 1023)}))
        else:
            parts = msg.topic.split('/')  # topic == "/IoTmanager/fake/water0/control"
            if len(parts) == 5 and parts[-1] == 'control':
                device = parts[2]
                node = parts[2]
                client.publish("/".join(parts[0:4]) + "/status", json.dumps({"status": msg.payload}))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', '-s', help='mqtt server', required=True)
    parser.add_argument('--port', '-n', type=int, help='mqtt port', required=True)
    parser.add_argument('--user', '-u', help='mqtt username', required=True)
    parser.add_argument('--password', '-p', help='mqtt password', required=True)
    args = parser.parse_args()

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    client.username_pw_set(args.user, args.password)
    client.connect(args.server, args.port, 60)
    client.loop_forever()
