import paho.mqtt.client as mqtt
import uuid
import io

broker = "192.168.0.100"

client = mqtt.Client()

client.connect(broker)

client.publish("home/meterkast/setting", "{\"type\":\"ota\"}")

client.disconnect()

data = []

with open('include/credentials.h', 'r') as file:
    data = file.readlines()

i = 0
for line in data:
    if line.startswith('#define UUID'):
        data[i] = '#define UUID \"ESPmeterkast_' + str(uuid.uuid4()) + '\"\n'
    i += 1

with open('include/credentials.h', 'w') as file:
    file.writelines(data)
