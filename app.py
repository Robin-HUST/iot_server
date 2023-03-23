from flask import Flask, request
import json, time
from mqtt import MQTT_Client
from sql_command import SQL_command


app = Flask(__name__)
app.logger = None

sql_command = SQL_command()

@app.route('/api/add_device', methods=['GET'])
def add_device_api():
    device_id = request.args.get('device_id')
    user_id = request.args.get('user_id')
    device_type = request.args.get('device_type')
    device_name = request.args.get('device_name')
    last_update = round(time.time())
    response_data = {
        "success": True
    }
    if(not sql_command.add_device(device_id, user_id, device_type, device_name, 1, last_update)):
        response_data['success'] = False
    return json.dumps(response_data)

@app.route('/api/delete_device', methods=['GET'])
def delele_device_api():
    device_id = request.args.get('device_id')
    user_id = request.args.get('user_id')
    response_data = {
        "success": True
    }
    if(not sql_command.delete_device(device_id, user_id)):
        response_data['success'] = False
    return json.dumps(response_data)


mqtt_client = MQTT_Client("localhost", 8080)
app.run(debug=False, host="0.0.0.0")


