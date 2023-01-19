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

    echo "Rebooting board.."
    screen -S tz-nw -p 0 -X stuff "root^M"
    screen -S tz-nw -p 0 -X stuff "^Creboot^M"
    screen -S tz-sw -p 0 -X stuff "^Creset^M"

    echo "Wait until the board boots up.."
    until cat $LOG_FILE | grep 'buildroot login:' ; do sleep 1; done

    echo "Logging in.."
    screen -S tz-nw -p 0 -X stuff "root^M"
    sleep 0.5

    echo "Synchronizing time.."
    TIMESTAMP=$(python3 /time-sync.py)
    screen -S tz-nw -p 0 -X stuff "^Cdate -s @$TIMESTAMP^M"
    sleep 0.5

    echo "Running EM.."
    screen -S tz-nw -p 0 -X stuff "^Coptee_event_manager $PORT^M"
    tail -f $LOG_FILE
else
    python3 /run_qemu.py
fi