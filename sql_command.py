import sqlite3

class SQL_command():
    def __init__(self) -> None:
        self.con = sqlite3.connect('data/database/iot.db', check_same_thread=False)
        self.cur = self.con.cursor()

    def add_device(self, device_id, user_id, device_type, device_name, device_status, last_update):
        self.cur.execute("INSERT INTO devices VALUES(?, ?, ?, ?, ?, ?, ?)", (device_id, user_id, device_type, device_name, device_status, last_update, "", ))
        self.con.commit()
        if self.cur.rowcount > 0:
            return True
        else:
            return False
    def delete_device(self, device_id, user_id):
        self.cur.execute("DELETE FROM device_data WHERE id = ?", (device_id, ))
        self.cur.execute("DELETE FROM devices WHERE id = ? AND user_id = ?", (device_id, user_id, ))
        self.con.commit()
        if self.cur.rowcount > 0:
            return True
        else:
            return False
        
    def update_data(self, device_id, device_data, timestamp):
        self.cur.execute("INSERT INTO device_data VALUES(?, ?, ?)", (device_id, device_data, timestamp, ))
        self.cur.execute("UPDATE devices SET last_update=?, last_data=? WHERE id=?;", (timestamp, device_data, device_id, ))
        self.con.commit()
        if self.cur.rowcount > 0:
            return True
        else:
            return False


    def get_data(self, time_start, time_end):
        self.cur.execute("SELECT * FROM data WHERE timestamp > ? AND timestamp < ?", (time_start, time_end))
        return self.cur.fetchall()

    def close(self):
        self.con.close()
