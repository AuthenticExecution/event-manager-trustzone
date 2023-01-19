from datetime import datetime
import time

NETWORK_DELAY = 0.005

timestamp = datetime.now().timestamp()
remaining = 1 - (timestamp % 1) - NETWORK_DELAY
#print(f"ts: {timestamp} remaining: {remaining}")
time.sleep(remaining)
print(int(timestamp + 1))