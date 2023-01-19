#!/bin/bash

set -e

NW=$1
HOST=$2
FILE=tz.txt

echo "" > $FILE

echo "Opening screen session"
screen -L -Logfile $FILE -dmS tz-nw $NW 115200
screen -r tz-nw -X colon "logfile flush 0.001^M"
sleep 0.5

read -p "Unplug power and ethernet, wait 2 seconds, plug power. Press Enter to continue"

echo "Wait until the board boots up.."
until cat $FILE | grep 'buildroot login:' ; do sleep 1; done
echo "Logging in.."
screen -S tz-nw -p 0 -X stuff "root^M"

read -p "Plug the ethernet cable and press Enter to continue"
sleep 2

echo "Synchronizing time.."
python3 scripts/time-sync.py &
sleep 0.5
screen -S tz-nw -p 0 -X stuff "^Cdate -s @`wget -qO- $HOST`^M"
sleep 0.5
kill %1
sleep 0.5

cat $FILE
rm -rf $FILE