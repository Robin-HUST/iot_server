import paho.mqtt.client as mqtt
from sql_command import SQL_command
import json, time


push_data_topic = "test"
sql_command = SQL_command()

class MQTT_Client():
    def __init__(self, broker, port):
        self.broker = broker
        self.port = port
        self.push_data_topic = push_data_topic
        client = mqtt.Client()
        client.on_connect = self.on_connect
        client.connect(self.broker, self.port)
        client.loop_start()

    def on_connect(self, client, userdata, flags, rc):
        client.subscribe(self.push_data_topic)
        client.message_callback_add(self.push_data_topic, self.push_data_callback)

    def push_data_callback(self, client, userdata, msg):
        msg = json.loads(msg.payload.decode())
        device_id = msg['device_id']
        device_type = msg['device_type']
        device_data = msg['device_data']
        timestamp = round(time.time())
        print(sql_command.update_data(device_id, device_data, timestamp))


# {\"device_id\": \"abc\", \"device_type\": 1, \"device_data\": 80}