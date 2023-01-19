#!/bin/bash

set -e

LOG_FILE=tz.log
rm -f $LOG_FILE

if [[ $IMX = "1" ]]; then
    echo "Running on IMX"
    screen -L -Logfile $LOG_FILE -dmS tz-nw /dev/NW 115200
    screen -L -Logfile $LOG_FILE -dmS tz-sw /dev/SW 115200
    screen -r tz-nw -X colon "logfile flush 0.001^M"
    screen -r tz-sw -X colon "logfile flush 0.001^M"
    #echo "Synchronizing time.."
    #python3 /time-sync.py &
    #screen -S tz-nw -p 0 -X stuff "^Cdate -s @`wget -qO- 134.58.46.188:55555`^M"
    echo "Running EM.."
    screen -S tz-nw -p 0 -X stuff "^Coptee_event_manager^M"
    tail -f $LOG_FILE
else
    python3 /run_qemu.py
fi