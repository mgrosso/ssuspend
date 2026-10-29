/*
** ----------------------------------------------------------------------
** ----------------------------------------------------------------------
**
** File:      ssuspend.c
** Author:    mgrosso 
** Created:   Sat Feb  2 05:34:02 PST 2008 on caliban
** Project:   ssuspend
** Purpose:   atomically suspend a linux laptop. 
** 
** $Id$
** ----------------------------------------------------------------------
** ----------------------------------------------------------------------
*/

// suspending linux is as simple as "echo disk >/sys/power/state"
//
// except
//
// only root can do this, so you right a setuid shell script, which is a
// bit evil
//
// and then you discover that you need to remove and restore some 
// modules that dont behave so your shell script gets more complicated
//
// and then you realize that sometimes your keystroke shortcut that runs
// the shell script is sometimes running the shellscript more than once
// so laptop starts to wake up and then goes right back to sleep. 
//
// this program was finally motivated by the last issue.
//
// TODO: use /etc/ssuspend/{before|after} exclusively
// TODO: and have them call /etc/ssuspend/{before|after}.d/*

#define _GNU_SOURCE
//configure writes the helper paths below into this header.  it is optional:
//every path has an #ifndef default further down, so this file still builds
//standalone with a bare "gcc src/ssuspend.c".
#ifdef HAVE_CONFIG_H
#include "system_config.h"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <sys/swap.h>
#include <syslog.h>

static pid_t child_pid;
static int   child_exit;

static int argc_start=1;
static char *short_options = "baf:s:t:d:pm:DxXlvh?";
static int before=0;
static int after=0;
static int verbose=0;
static int use_syslog=0;
static int dryrun=0;
static int do_lockx=0;
static int do_lockx_nofail=0;
static int do_disk=0;
#define POWER_STATE_PATH "/sys/power/state"
char *power_state_path=POWER_STATE_PATH ;
#define STATE_WORD "disk"
char *state_word=STATE_WORD ;
//newer kernels want the hibernation method written here before STATE_WORD
//will be accepted by POWER_STATE_PATH.
#define POWER_DISK_PATH "/sys/power/disk"
char *power_disk_path=POWER_DISK_PATH ;
#define DISK_WORD "platform"
char *disk_word=DISK_WORD ;
char *device_path=NULL ;
static int seconds = 30;
const char *argvzero = NULL;
//NOTE: every entry passes NULL for flag and its short option letter for val, so
//getopt_long() returns that letter and handle_options() sees it in the switch
//below.  a non-NULL flag makes getopt_long() return 0 instead, which used to
//land on "case 0: usage()" and made these options unusable.
static struct option long_options [] = {
    { "before",0,NULL,'b'},
    { "after",0,NULL,'a'},
    { "file",1,NULL,'f'},
    { "state",1,NULL,'s'},
    { "time",1,NULL,'t'},
    { "device",1,NULL,'d'},
    { "disk",0,NULL,'p'},
    { "method",1,NULL,'m'},
    { "dryrun",0,NULL,'D'},
    { "syslog",0,NULL,'l'},
    { "verbose",0,NULL,'v'},
    { "lock-x",0,NULL,'x'},
    { "lock-x-nofail",0,NULL,'X'},
    { "help",0,NULL,'h'},
    {0,0,0,0}
};

//the helper paths below are absolute on purpose.  this program is installed
//setuid root, so resolving them through the caller's $PATH would let anyone
//substitute their own binary and have us run it as root.  configure fills
//these in via system_config.h; the #ifndef defaults are the fallback.
//
//LOCKX is the one deliberate exception: it only has to talk to the invoking
//user's X session, so lock_x() drops privileges before exec'ing it and $PATH
//resolution is harmless -- a hijacked $PATH buys nothing the user did not
//already have.
#define LOCKX "xscreensaver-command"
static char *lockx_args[3] = { LOCKX, "-lock",NULL };
static const char *lockx = LOCKX ;

//the root helpers get this instead of whatever the caller had in their
//environment.  the hooks in /etc/ssuspend are shell scripts, so they need a
//PATH to be useful, but it must be ours and not the caller's.
//
//this is built from scratch rather than filtered, which is why there is no
//IFS here: an unset IFS leaves the shell to use its own default, and nothing
//hostile can survive into an environment we construct ourselves.  LANG and
//LC_* are omitted for the same reason, and the resulting C locale keeps
//grep's matching predictable as a bonus.
#ifndef SAFE_PATH
#define SAFE_PATH "/sbin:/bin:/usr/sbin:/usr/bin"
#endif
static char *safe_env[2] = { "PATH=" SAFE_PATH, NULL };

#ifndef GREP_PATH
#define GREP_PATH "/bin/grep"
#endif
static char  * grepmod_args[6] = { GREP_PATH,"-w","-q",NULL,"/proc/modules",NULL };
static const char *grep = GREP_PATH ;
#define GREPMOD_ARG 3

#ifndef RMMOD_PATH
#define RMMOD_PATH "/sbin/rmmod"
#endif
static char  * rmmod_args[3] = { RMMOD_PATH,NULL,NULL };
static const char * rmmod = RMMOD_PATH ;
#define RMMOD_ARG 1

#ifndef MODPROBE_PATH
#define MODPROBE_PATH "/sbin/modprobe"
#endif
static char  * modprobe_args[3] = { MODPROBE_PATH,NULL,NULL };
static const char * modprobe = MODPROBE_PATH ;
#define MODPROBE_ARG 1

#ifndef DD_PATH
#define DD_PATH "/bin/dd"
#endif
static char  * dd_args[6] = { DD_PATH,"if=/dev/zero",NULL,"count=10240","bs=1024", NULL };
static const char * dd = DD_PATH;
#define DD_ARG 2

#ifndef MKSWAP_PATH
#define MKSWAP_PATH "/sbin/mkswap"
#endif
static char  * mkswap_args[5] = { MKSWAP_PATH,"-L","ssuspend",NULL,NULL };
static const char * mkswap = MKSWAP_PATH;
#define MKSWAP_ARG 3

#ifndef PRESHELL_PATH
#define PRESHELL_PATH "/etc/ssuspend/before"
#endif
static char  * preshell_args[2] = { PRESHELL_PATH,NULL };
static const char * preshell = PRESHELL_PATH;
#define PRESHELL_ARG 0
#ifndef POSTSHELL_PATH
#define POSTSHELL_PATH "/etc/ssuspend/after"
#endif
static char  * postshell_args[2] = { POSTSHELL_PATH,NULL };
static const char * postshell = POSTSHELL_PATH;
#define POSTSHELL_ARG 0


//TODO: check /proc/meminfo to see if we are likely to have enough room to suspend. 
//no gaurantees, but if MemTotal-Buffers-Cache <  SwapFree * 0.8 we should be ok.
//read the file and parse.
//"egrep","MemTotal|Buffers|Cached|SwapTotal|SwapFree", "/proc/meminfo" 
 
typedef struct node_struct {
    struct node_struct *next;
    char  *data;
} node_t;

static node_t *head=NULL;

static int power_state_fd=-1;

static void opensyslog(){
    if(use_syslog){
        openlog("ssuspend",LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER );
        //NOTE: setlogmask() wants a bitmask built with LOG_MASK()/LOG_UPTO(),
        //not the priority values themselves.  ORing the priorities together
        //gives 7, which setlogmask reads as EMERG|ALERT|CRIT only, so every
        //LOG_INFO was being discarded.
        if(verbose ){
            setlogmask( LOG_UPTO(LOG_DEBUG) );
        }else{
            setlogmask( LOG_UPTO(LOG_WARNING) );
        }
    }else{
        setlogmask( LOG_MASK(LOG_EMERG) );
    }
}

static void puke(const char *message){
    if(use_syslog){
        syslog(LOG_ERR, "exiting because of failure with: %s, errno=%i sterror=%s", message, errno, strerror(errno));
    }else{
        perror(message);
    }
    exit(1);
}

static int warnif(int ret, const char *message){
    if( ret != 0 ){
        if(use_syslog){
            syslog(LOG_WARNING, "warning for: %s, errno=%i sterror=%s", message, errno, strerror(errno));
        }else{
            perror(message);
        }
    }
    return ret;
}

#define SSUSPEND_MAX_MESSAGE 1024
static void verbose_log(const char *format, ...){
    if(verbose || use_syslog){
        char buf[SSUSPEND_MAX_MESSAGE];
        va_list ap;
        va_start(ap, format);
        memset( buf, '\0', SSUSPEND_MAX_MESSAGE );
        vsnprintf( buf, SSUSPEND_MAX_MESSAGE-1, format, ap );
        buf[SSUSPEND_MAX_MESSAGE-1]='\0';
        va_end( ap);
        //buf is already formatted, and can contain '%' from argv, so it must be
        //passed as an argument and never as a format string of its own.
        if(verbose){
            fprintf(stderr, "%s", buf);
        }
        if(use_syslog){
            syslog(LOG_INFO, "%s", buf);
        }
    }
}

static void pukeif(int ret, const char *message){
    if( ret != 0 ){
        puke(message);
    }
}

static void push( char *s ){
    node_t *n=(node_t *)malloc(sizeof(node_t));
    if(!n){
        puke("cant malloc");
    }
    n->data=s;
    if(head==NULL){
        n->next=NULL;
        head =n;
    }else{
        n->next=head;
        head=n;
    }
}

static char * pop(){
    if(head==NULL){
        return NULL;
    }
    char *giveback=head->data;
    node_t *oldhead=head;
    head=head->next;
    free(oldhead);
    return giveback;
}


static int get_exit_status(pid_t pid){
    int eintr_count;
    int ret;
    int flags=0;
    int status=-1;
    for( eintr_count=10;
        eintr_count && 
            ( -1 == ( ret = waitpid( pid, &status, flags ))) && 
            errno == EINTR ;
        --eintr_count ){
        ;//noop
    }
    if(!eintr_count){
        puke("could not get exit status, too many EINTR");
    }
    if(ret==0){
        puke("could not get exit status, process not dead yet even though flags said to wait");
    }
    if(ret==-1){
        if(child_pid==pid){
            return child_exit;
        }
        puke("could not get exit status");
    }
    if(ret==pid){
        if( WIFEXITED(status) ){
            return WEXITSTATUS(status);
        }
        return status;
    }
    puke("return result of waitpid was not the pid we expected.");
    //not reached.
    return ret;
}

//there are exactly two kinds of exec here, and the two properties travel
//together:
//
//  as_user==0  root helper.  keeps our privileges, gets safe_env instead of the
//              caller's environment, and is exec'd by absolute path with no
//              $PATH search at all.
//  as_user==1  the X screen locker.  drops to the invoking user first, then
//              keeps their environment (it needs DISPLAY and XAUTHORITY) and is
//              located on their $PATH.  neither is a privilege once we are no
//              longer root.
static int fork_exec_wait_flags(const char *command, char *const args[], int as_user){
    const char *a;
    int i=0;
    verbose_log( "%s: running %s%s, args:", argvzero, command,
            as_user ? " as the invoking user" : " as root with a clean environment");
    for( a = args[i]; a && *a; a=args[++i] ){
        verbose_log("%s %s arg %i=[%s]", argvzero, command, i, a );
    }
    if(dryrun){
        return 0;
    }
    int giveback=1;
    pid_t pid=fork();
    if(pid==-1){
        puke("cant fork");
    }
    else if( pid==0){
        if(as_user){
            //order matters: once setuid() has succeeded we are no longer
            //privileged enough to setgid().  both must be checked, because
            //exec'ing after a failed drop would run the child as root.
            if( setgid(getgid()) != 0 ){
                puke("could not drop group privileges");
            }
            if( setuid(getuid()) != 0 ){
                puke("could not drop user privileges");
            }
            execvp(command,args);
        }else{
            //execve() and not execvp(): no $PATH search, so command has to be
            //absolute.  configure hands us absolute paths and the #ifndef
            //defaults are absolute, so a relative one is a misconfiguration and
            //will fail loudly below rather than resolve against anything.
            execve(command,args,safe_env);
        }
        puke(command);
    }
    else{
        giveback=get_exit_status(pid);
    }
    return giveback;
}

static int fork_exec_wait(const char *command, char *const args[]){
    return fork_exec_wait_flags(command,args,0);
}

//for helpers that must not run as root.  see the LOCKX note above.
static int fork_exec_wait_nopriv(const char *command, char *const args[]){
    return fork_exec_wait_flags(command,args,1);
}

static int fork_exec_wait_arg(const char *command, char * args[], int argsub, char *arg ){
    args[argsub]=arg;
    return fork_exec_wait(command,args);
}

static void lock_x(){
    if(do_lockx){
        pukeif(fork_exec_wait_nopriv(lockx,lockx_args),lockx);
    }else if(do_lockx_nofail){
        fork_exec_wait_nopriv(lockx,lockx_args);
    }
}

static void pushif(char *s ){
    int ret=fork_exec_wait_arg(grep, grepmod_args,GREPMOD_ARG,s);
    if(ret==0){
        pukeif(fork_exec_wait_arg(rmmod, rmmod_args, RMMOD_ARG, s ),rmmod);
        push(s);
    }
}

static void remove_modules(int argc, char **argv ){
    int mod;
    for(mod=argc_start; mod < argc; ++mod){
        pushif(argv[mod]);
    }
}

static void addback_modules(){
    char *s=NULL;
    while((s=pop())){
        warnif(fork_exec_wait_arg(modprobe, modprobe_args,MODPROBE_ARG,s),s);
    }
}

static void lock_file(){
    verbose_log("%s: opening and locking file (%s)\n", argvzero, power_state_path);
    if(dryrun){
        return ;
    }
    //power_state_fd=open(power_state_path, O_TRUNC | O_SYNC | O_NOFOLLOW | O_WRONLY );
    power_state_fd=open(power_state_path, O_TRUNC | O_SYNC | O_WRONLY );
    if(power_state_fd == -1 ){
        puke("could not open state file, default /sys/power/state, for writing");
    }
    pukeif(flock(power_state_fd,LOCK_EX|LOCK_NB ),"could not lock /sys/power/state exclusively");
    verbose_log("%s: opened and locked file (%s)->%i\n", argvzero, power_state_path, power_state_fd);
}

static int do_swapoff(char *path){
    verbose_log("%s: calling %s(%s)\n",argvzero,"swapoff",path);
    if(dryrun){
        return 0;
    }
    return warnif(swapoff(path),"swapoff"); 
}

static int do_wipeswap(char *path){
    size_t len = strlen(path) + strlen("of=") + 1;
    char *s = malloc(len);
    pukeif(s==NULL ? 1 : 0, "could not malloc a very small string.");
    *s='\0';
    sprintf(s,"of=%s",path);
    int ret = warnif(fork_exec_wait_arg(dd, dd_args, DD_ARG, s),dd);
    free(s);
    return ret;
}

static int do_mkswap(char *path){
    return warnif(fork_exec_wait_arg(mkswap, mkswap_args,MKSWAP_ARG,path),mkswap);
}

static int do_swapon(char *path){ 
    verbose_log("%s: calling %s(%s)\n",argvzero,"swapon",path);
    if(dryrun){
        return 0;
    }
    return warnif(swapon(path,0),"swapon"); 
}

static int clean_image_from_device(){
    if(!device_path){
        return 0;
    }
    int ret = (     
            (do_swapoff(device_path)) || 
            (do_wipeswap(device_path)) ||
            (do_mkswap(device_path)) ||
            (do_swapon(device_path)) 
    );
    warnif(ret, "problem with wiping the swap device.");
    return ret;
}

//newer kernels require the hibernation method, eg "platform", to be written to
///sys/power/disk before they will accept "disk" on /sys/power/state.  unlike
//write_file() below this write returns immediately and does not suspend us.
static void write_disk_method(){
    ssize_t written;
    size_t  count;
    int     disk_fd;
    if(!do_disk){
        return;
    }
    count=strlen(disk_word);
    verbose_log("%s: writing (%s) to file (%s)\n", argvzero, disk_word, power_disk_path);
    if(dryrun){
        return ;
    }
    disk_fd=open(power_disk_path, O_TRUNC | O_SYNC | O_WRONLY );
    if(disk_fd == -1 ){
        puke("could not open disk method file, default /sys/power/disk, for writing");
    }
    written=write(disk_fd,(void *)disk_word,count);
    if(written!=(ssize_t)count){
        if(written==-1){
            puke("write to /sys/power/disk or its alternate failed.");
        }
        puke("could not write the whole method word to /sys/power/disk or its alternate in one shot.  bit odd that.");
    }
    warnif(close(disk_fd),"problem closing /sys/power/disk or its alternate");
    verbose_log("%s: wrote (%s) to file (%s)\n", argvzero, disk_word, power_disk_path);
}

static void write_file(){
    ssize_t written;
    size_t  count=strlen(state_word);
    //this call will put us to sleep.
    written=write(power_state_fd,(void *)state_word,count);
    //when we get here, either the call succeeded, in which case we have just finished resuming,
    //or the call failed in which case we never went to sleep.
    if(written!=count){
        if(written==-1){
            puke("write to /sys/power/state or its alternate failed.");
        }else if(written<count){
            puke("could not write the entire word 'disk' to /sys/power/state or its alternate in one shot.  bit odd that.");
        }else if(written>count){
            puke("wrote too many characters trying to write 'disk' to /sys/power/state.  bit odd that.");
        }
    }
    //warnif(fdatasync(power_state_fd),"could not fdatasync() /sys/power/state or its alternate.");
}

static void close_file(){
    //NOTE: strictly speaking this isnt needed as _exit() will do it for us after 
    //NOTE: stubbing as doing it caused problems.
    //warnif(close(power_state_fd), "problem closing /sys/power/state");
}

static void usage(){
    fprintf(stderr,"%s [options] [module [ module ... ] ]\n"
"\n"
"version 1.0.0\n"
"\n"
"   each module argument found in /proc/modules is removed in order on the way down, and restored \n"
"       in reverse order on the way back up.\n"
"\n"
"   options are as follows:"
"    -h  or -? or --help                 get this message\n"
"    -b  or --before                     runs " PRESHELL_PATH " after lock but before ssuspending\n"
"    -a  or --after                      runs " POSTSHELL_PATH " after resume but before releasing lock\n"
"    -f  <file> or --file <file>         where to write the new state. defaults to /sys/power/state.\n"
"    -s  <state> or --state <state>      what state to write to the control file. defaults to 'disk'.\n"
"    -t  <seconds> or --time <seconds>   how many seconds to sleep after resume while holding the lock.\n"
"    -d  <device> or --device <device>   an optional swap device to wipe clean of stale suspend images after waking up.\n"
"                                        This option defaults to 30 in order to prevent extra keypress\n"
"                                        events from causing double suspends where your laptop resumes\n"
"                                        only to suspend again immediately.\n"
"    -p  or --disk                       write the hibernation method to /sys/power/disk before suspending.\n"
"                                        newer kernels require this before they will accept 'disk' on\n"
"                                        /sys/power/state.\n"
"    -m  <method> or --method <method>   which method to write to /sys/power/disk. defaults to 'platform'.\n"
"                                        implies -p.\n"
"    -x  or --lock-x                     tells ssuspend to lock the screen before suspending\n"
"    -X  or --lock-x-nofail              tells ssuspend to lock the screen before suspending if possible\n"
"                                        but continue in any case\n"
"                                        both run " LOCKX " as the invoking user rather than as\n"
"                                        root, keeping your environment and locating it on $PATH.\n"
"                                        every other helper runs as root from a compiled-in absolute\n"
"                                        path with a clean environment (PATH=" SAFE_PATH "),\n"
"                                        so " PRESHELL_PATH " and " POSTSHELL_PATH "\n"
"                                        do not inherit the calling environment.\n"
"    -l  or --syslog                     TODO: tells ssuspend to write status information to syslog(3).\n"
"    -v  or --verbose                    tells ssuspend to write status information to standard error.\n"
"    -D  or --dryrun                    causes ssuspend to print out what it would do to stderr. implies verbose.\n"
"\n", argvzero
   );
    exit(1);
}

static void strdup_option( char **dest ){
    *dest = strdup(optarg);
    if(*dest == NULL ){
        puke("could not allocate memory to copy option string.");
    }
}

static int do_sleep(){
    if(verbose){
        fprintf(stderr,"%s: sleeping for (%i)\n", argvzero,seconds);
        if(use_syslog){
            syslog(LOG_INFO, "%s: sleeping for (%i)\n", argvzero,seconds);
        }
    }
    if(dryrun){
        return 0;
    }
    return warnif(sleep(seconds), "sleep was interrupted");
}

static void script(int opt,const char *script, char *script_args[] ){
    if(!opt){
        return;
    }
    return pukeif(fork_exec_wait(script, script_args),script);
}

static void handle_options( int argc, char **argv ){
    int c;
    while(1){
        c=getopt_long(argc,argv,short_options,long_options,NULL);
        if(c==-1){
            argc_start=optind;
            break;
        }
        switch(c){
        case 'b':
            before=1;
            break;
        case 'a':
            after=1;
            break;
        case 'f':
            strdup_option(&power_state_path);
            break;
        case 's':
            strdup_option(&state_word);
            break;
        case 't':
            seconds=atoi(optarg);
            break;
        case 'd':
            strdup_option(&device_path);
            break;
        case 'p':
            do_disk=1;
            break;
        case 'm':
            strdup_option(&disk_word);
            do_disk=1;
            break;
        case 'D':
            dryrun = 1;
            verbose=1;
            break;
        case 'x':
            do_lockx=1;
            break;
        case 'X':
            do_lockx_nofail=1;
            break;
        case 'l':
            use_syslog=1;
            break;
        case 'v':
            verbose=1;
            break;
        case 'h':
        case '?':
        default:
            usage();
        }
    }
}

int main(int argc, char **argv ){
    argvzero = argv[0];
    handle_options(argc, argv );
    opensyslog();
    lock_x();
    lock_file();
    script(before,preshell,preshell_args);
    remove_modules(argc, argv );
    write_disk_method();
    if(!dryrun){
        write_file();
    }
    addback_modules();
    script(after,postshell,postshell_args);
    do_sleep();
    clean_image_from_device();
    close_file();
    return 0;
}

