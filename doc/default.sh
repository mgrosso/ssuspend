#!/bin/sh
# /etc/acpi/default.sh
# Default acpi script that takes an entry for all actions
 
set $*
 
group=${1%%/*}
action=${1#*/}
device=$2
id=$3
value=$4
 
#minimum milliamp hours remaining:
MIN_MAH=100
SUSPEND=/usr/local/sbin/ssuspend 
BAT_STATE_FILE=/proc/acpi/battery/BAT0/state
 
logger "ACPI default.sh: group=$group action=$action device=$device id=$id  value=$value  full=$*"
 
log_unhandled() {
    logger "ACPI this event unhandled: $*"
}
 
sleepnow(){
    logger "ACPI default.sh sleepnow(): group=$group action=$action device=$device id=$id  value=$value  full=$*"
    $SUSPEND -X iwl3945
    exit $?
}
 
if [ x$group == xbutton ] ; then
    logger "ACPI default.sh group == button"
    if [ x$action == xpower ] ; then
        logger "ACPI default.sh button/power"
        sleepnow 
    fi  
    if [ x$action == xlid ] ; then
        logger "ACPI default.sh button/lid"
        xset dpms force off 
        exit $?
    fi  
fi
if [ x$group == xbattery ] ; then
    logger "ACPI default.sh group == battery"
    if [ x$action == xbattery ] ; then
        MAH=$( cat $BAT_STATE_FILE |\
                /bin/grep ^remaining|\
                gawk '{print $3 }') 
        if [ $MAH -ge $MIN_MAH ] ; then 
             logger ACPI BAT0 ok, $MAH mah remaining
             exit
        else
             logger ACPI BAT0 dead. use $MAH mah to suspend
             sleepnow 
        fi
    fi  
fi
log_unhandled $*  
exit 


