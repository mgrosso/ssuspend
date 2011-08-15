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

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <sys/swap.h>

static pid_t child_pid;
static int   child_exit;

static int argc_start=1;
static char *short_options = "f:s:t:d:DxXlvh?";
static int verbose=0;
static int syslog=0;
static int dryrun=0;
static int do_lockx=0;
static int do_lockx_nofail=0;
#define POWER_STATE_PATH "/sys/power/state"
char *power_state_path=POWER_STATE_PATH ;
#define STATE_WORD "disk"
char *state_word=STATE_WORD ;
char *device_path=NULL ;
static int seconds = 30;
const char *argvzero = NULL;
static struct option long_options [] = {
    { "file",1,NULL,'f'},
    { "state",1,NULL,'s'},
    { "time",1,NULL,'t'},
    { "device",1,NULL,'d'},
    { "dryrun",0,&dryrun,'D'},
    { "syslog",0,&syslog,1},
    { "verbose",0,&verbose,1},
    { "lock-x",0,&do_lockx,1},
    { "lock-x-nofail",0,&do_lockx_nofail,1},
    { "help",0,0,0},
    {0,0,0,0}
};

#define LOCKX "xscreensaver-command"
static char *lockx_args[3] = { LOCKX, "-lock",NULL };
static const char *lockx = LOCKX ;

#define GREP "grep"
static char  * grepmod_args[6] = { GREP,"-w","-q",NULL,"/proc/modules",NULL };
static const char *grep = GREP ;
#define GREPMOD_ARG 3

#define RMMOD "/sbin/rmmod"
static char  * rmmod_args[3] = { RMMOD,NULL,NULL };
static const char * rmmod = RMMOD ;
#define RMMOD_ARG 1

#define MODPROBE "/sbin/modprobe"
static char  * modprobe_args[3] = { MODPROBE,NULL,NULL };
static const char * modprobe = MODPROBE ;
#define MODPROBE_ARG 1

#define DD "dd"
static char  * dd_args[6] = { DD,"if=/dev/zero",NULL,"count=10240","bs=1024", NULL };
static const char * dd = DD;
#define DD_ARG 2

#define MKSWAP "mkswap"
static char  * mkswap_args[5] = { MKSWAP,"-L","ssuspend",NULL,NULL };
static const char * mkswap = MKSWAP;
#define MKSWAP_ARG 3


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

static void puke(const char *message){
    perror(message);
    exit(1);
}

static int warnif(int ret, const char *message){
    if( ret != 0 ){
        perror(message);
    }
    return ret;
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

static int fork_exec_wait(const char *command, char *const args[]){
    if(verbose){
        fprintf(stderr,"%s: running %s, args:", argvzero,command);
        const char *a;
        int i=0;
        for( a = args[i]; a && *a; a=args[++i] ){
            fprintf(stderr," %i=[%s]", i, a );
        }
        fprintf(stderr,"\n" );
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
        execvp(command,args);
        puke(command);
    }
    else{
        giveback=get_exit_status(pid);
    }
    return giveback;
}

static int fork_exec_wait_arg(const char *command, char * args[], int argsub, char *arg ){
    args[argsub]=arg;
    return fork_exec_wait(command,args);
}

static void lock_x(){
    if(do_lockx){
        pukeif(fork_exec_wait(lockx,lockx_args),lockx);
    }else if(do_lockx_nofail){
        fork_exec_wait(lockx,lockx_args);
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
    if(verbose){
        fprintf(stderr,"%s: locking file (%s)\n",
                argvzero,power_state_path);
    }
    if(dryrun){
        return ;
    }
    //power_state_fd=open(power_state_path, O_TRUNC | O_SYNC | O_NOFOLLOW | O_WRONLY );
    power_state_fd=open(power_state_path, O_TRUNC | O_SYNC | O_WRONLY );
    if(power_state_fd == -1 ){
        puke("could not open state file, default /sys/power/state, for writing");
    }
    pukeif(flock(power_state_fd,LOCK_EX|LOCK_NB ),"could not lock /sys/power/state exclusively");
}

static int do_swapoff(char *path){
    if(verbose){
        fprintf(stderr,"%s: calling %s(%s)\n",argvzero,"swapoff",path);
    }
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
    if(verbose){
        fprintf(stderr,"%s: calling %s(%s)\n",argvzero,"swapon",path);
    }
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
"    -f  <file> or --file <file>         where to write the new state. defaults to /sys/power/state.\n"
"    -s  <state> or --state <state>      what state to write to the control file. defaults to 'disk'.\n"
"    -t  <seconds> or --time <seconds>   how many seconds to sleep after resume while holding the lock.\n"
"    -d  <device> or --device <device>   an optional swap device to wipe clean of stale suspend images after waking up.\n"
"                                        This option defaults to 30 in order to prevent extra keypress\n"
"                                        events from causing double suspends where your laptop resumes\n"
"                                        only to suspend again immediately.\n"
"    -x  or --lock-x                     tells ssuspend to lock the screen before suspending\n"
"    -X  or --lock-x-nofail              tells ssuspend to lock the screen before suspending if possible\n"
"                                        but continue in any case\n"
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
    }
    if(dryrun){
        return 0;
    }
    return warnif(sleep(seconds), "sleep was interrupted");
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
        case 0:
            usage();
            break;//not reached.
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
            syslog=1;
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
    lock_x();
    lock_file();
    remove_modules(argc, argv );
    if(!dryrun){
        write_file();
    }
    addback_modules();
    do_sleep();
    clean_image_from_device();
    close_file();
    return 0;
}

