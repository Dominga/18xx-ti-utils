#!/bin/sh

wpa_cli -imesh0 terminate

iw mesh0 del

### stop udhcp client, if not started
output=`ps | grep -v grep | grep udhcpc`
set -- $output
echo $output
if [ -n "$output" ]; then
   killall udhcpc
fi
