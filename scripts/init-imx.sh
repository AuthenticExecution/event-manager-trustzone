#!/bin/bash

set -e

NW=$1
SW=$2
FILE=tz.txt

echo "" > $FILE

echo "Opening screen sessions"
screen -L -Logfile $FILE -dmS tz-nw $NW 115200
screen -r tz-nw -X colon "logfile flush 0.001^M"
screen -L -Logfile $FILE -dmS tz-sw $SW 115200
screen -r tz-sw -X colon "logfile flush 0.001^M"
sleep 0.5

echo "Rebooting board"
screen -S tz-nw -p 0 -X stuff "root^M"
screen -S tz-nw -p 0 -X stuff "^Creboot^M"
screen -S tz-sw -p 0 -X stuff "^Creset^M"

echo "Wait until the board boots up.."
until cat $FILE | grep 'buildroot login:' ; do sleep 1; done
echo "Logging in.."
screen -S tz-nw -p 0 -X stuff "root^M"
sleep 0.5

echo "Synchronizing time.."
TIMESTAMP=$(python3 scripts/time-sync.py)
screen -S tz-nw -p 0 -X stuff "^Cdate -s @$TIMESTAMP^M"
sleep 0.5

echo "Closing sessions"
screen -X -S tz-nw quit
screen -X -S tz-sw quit

cat $FILE
rm -rf $FILE