from flask import Flask
from datetime import datetime
import time

NETWORK_DELAY = 0.005
app = Flask("time-server")

@app.route("/")
def hello_world():
    timestamp = datetime.now().timestamp()
    remaining = 1 - (timestamp % 1) - NETWORK_DELAY
    #print(f"ts: {timestamp} remaining: {remaining}")
    time.sleep(remaining)
    return str(int(timestamp + 1))

app.run("0.0.0.0")