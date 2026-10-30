#!/bin/sh
# /etc/acpi/default.sh
# Default acpi script that takes an entry for all actions
#
# This handles the power button and the lid, which still need acpid.
#
# It used to handle low battery too, by reading
# /proc/acpi/battery/BAT0/state.  That interface no longer exists on current
# kernels, and ssuspend does the job itself now, so that section is gone.  Run
# this instead, once, from your session:
#
#     ssuspend -p -X -c 10 -w iwl3945
#
# which waits, holding no lock, until the battery is Discharging and under 10
# percent.  See the "Suspending on low battery" section of the README.

set $*

group=${1%%/*}
action=${1#*/}
device=$2
id=$3
value=$4

SUSPEND=/usr/local/sbin/ssuspend

logger "ACPI default.sh: group=$group action=$action device=$device id=$id  value=$value  full=$*"
 
log_unhandled() {
    logger "ACPI this event unhandled: $*"
}
 
sleepnow(){
    logger "ACPI default.sh sleepnow(): group=$group action=$action device=$device id=$id  value=$value  full=$*"
    $SUSPEND -X -p iwl3945
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
log_unhandled $*
exit


