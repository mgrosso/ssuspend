# ssuspend

`ssuspend` is a small C program intended to assist in managing suspend and resume on Linux laptops or customized Linux environments.

## Motivation

The first and primary use case in 2008 was to allow unprivileged keystrokes captured by a window manager to safely read and write to the /sys/ paths needed for suspend to disk. Functionality was added to coordinate the before and after unloading and loading of kernel modules that didn't play nicely with suspend/resume, and additional features optionally ensure that the screen is locked when the laptop resumes.

18 years later Linux distributions and device modules are much better about handling suspend and resume out of the box, so this utility won't find a wide audience, however for those that want to run a tiny utility to suspend when battery hits 5% without fighting their distributions ACPI configurations or who want their own window manager hot-key to suspend exactly in the way that they want without introducing security issues, ssuspend still fills that need.

## Overview

ssuspend locks /sys/power/state, optionally removes zero or more
kernel modules, echoes a suspend mode into /sys/power/state, and then sleeps 30
or so seconds before unlocking that file.  Because it will not wait for the
lock but exit immediately if it cant acquire that lock exclusively, it is safe
to use from window managers where a keyboard shortcut keypress held down too
long or some other race condition might lead to the program being called
multiple times quickly in succession.  Because it is a small program written
carefully in c, it is fairly safe to make setuid; it refuses the options that
would let an unprivileged caller redirect what root touches.  See "What a
caller may and may not change" below, and the note on sudo after it.

It can also make the whole thing conditional on the battery being low and
discharging, and optionally wait for that to become true, which removes the
need for a custom acpid handler to watch the battery.  See "Suspending on low
battery" below.


## Installation

### Configure and Build

ssuspend uses GNU autoconf and automake.  The generated files are not kept in
git, so a fresh checkout has no ./configure yet and you have to make one:

```sh
autoreconf -i
./configure
make
```

A release tarball already contains ./configure, so there the first step is
unnecessary.

configure finds the helper programs ssuspend runs, and compiles their absolute
paths in.  Override any of them if it guesses wrong:

```sh
./configure --with-grep=/usr/bin/grep --with-mkswap=/usr/sbin/mkswap
```

--with-dd, --with-rmmod and --with-modprobe work the same way, and
--with-before-script / --with-after-script move the optional /etc/ssuspend
hooks.  A tool that is not found is only a warning, not an error: a built-in
default is used instead.

### Install

"make install" puts the binary in $prefix/sbin, or just copy "src/ssuspend"
wherever you like.

ssuspend writes to /sys/power/state, so it needs root.  Either make it setuid:

```sh
chown root:root /usr/local/sbin/ssuspend
chmod u+s /usr/local/sbin/ssuspend
```

or leave it unprivileged and call it through sudo, as the fvwm example below
does.  Both work, but they are not equally safe -- see below.

### What a caller may and may not change

ssuspend splits its inputs in two.

Policy is what a privileged process is allowed to touch: which files it reads
and writes, which programs it runs, and what environment those programs get.
That belongs to whoever installs the binary and sets the setuid bit, so it is
compiled in.  configure sets it:

```text
--with-grep=          --with-dd=             --with-mkswap=
--with-rmmod=         --with-modprobe=       --with-safe-path=
--with-before-script= --with-after-script=   --with-power-supply-path=
```

The programs run as root have no run time override at all, deliberately: such
an option would be an arbitrary-command-as-root switch.

Parameters are how one invocation behaves, and the caller picks them freely:
-t, -c, -w, -W, -p, -x, -X, -b, -a, -v, -l, -D.  They change timing and which
steps run, never what a privileged process touches, so none of them is gated.

Note the `--with-` options set policy only.  Plain defaults such as the -t
sleep and the -W interval are compile-time constants with no configure knob,
because they are parameters: a caller may already change them at run time.

Five options sit in between.  -f and -F name the control files written to, -d
names a swap device wiped and rebuilt, -B names a battery directory read, and
-S names the directory searched for one.  Each redirects where a root process
reads or writes -- -f on its own is enough to truncate any existing file and
refill it with whatever -s says -- so all five are refused outright when
ssuspend is running from its setuid or setgid bit.  Run it as root, or build it
without the setuid bit, and they work normally.  That is how to test them: a
non-setuid build behaves exactly like the installed one except that these five
are permitted.

-s and -m are allowed either way.  With the paths pinned they only write a word
to the compiled-in control files, where the kernel rejects anything it does not
recognise, so choosing "mem" instead of "disk" stays available to everyone.

The check is uid != euid (or gid != egid), which asks whether this binary
granted the privilege or the caller already had it.

### Why setuid is safer than sudo here

That check cannot help under sudo.  "sudo ssuspend" runs with real uid 0, so it
looks like genuine root and -f, -F, -d, -B and -S are all permitted.  If sudoers
grants ssuspend to anyone you would not hand a root shell to, you have to
restrict the arguments in sudoers as well, which is easy to get wrong.

Setuid has no such gap, so prefer it unless you have a reason not to.  Either
way, every helper ssuspend runs as root comes from a compiled-in absolute path
and gets a clean environment, so a hostile $PATH cannot redirect them.  The one
exception is the screen locker from -x and -X, which is deliberately run with
your own privileges and found on your own $PATH, since it only needs to talk to
your X session.

### Using with Fvwm

I personally use this with fvwm using the following lines...

```text
AddToFunc Suspend       "I" Exec exec sudo /usr/local/sbin/ssuspend -Xvlp --method suspend
```

and

```text
Key BackSpace     A   MC  Suspend
```

so that the laptop sleeps whenever alt-control-end is pressed. I haven't had to work around any misbehaving modules or device tree entries in quite some time so I haven't used the -a, -b, and module args in a number of years.

### Suspending on low battery

ssuspend can make the suspend conditional on the battery itself, which removes
the need for an acpid battery handler entirely:

```sh
ssuspend -p -X -c 10 iwl3945
```

suspends only if the battery is Discharging and below 10 percent, and exits 0
without doing anything otherwise.  Adding -w makes it wait rather than give up,
rechecking every 60 seconds -- or every -W seconds -- until the condition is
met:

```sh
ssuspend -p -X -c 10 -w iwl3945
```

That second form is meant to be started once from your session and left
running.  While waiting it holds no lock and does not blank the screen, so an
ordinary manual suspend still works in the meantime.

The battery is located by scanning /sys/class/power_supply for the first supply
whose type is Battery, so this keeps working across kernel upgrades and across
machines without having to chase the /sys/devices path.  Use -B to name one
explicitly if the machine has more than one battery, or -S to point the search
somewhere else.

### Using with acpid

NOTE: this section predates the -c and -w options above, which are now the
easier way to suspend on low battery.  The script is still useful for power
button and lid handling, which do need acpid.  Its original battery section
read /proc/acpi/battery/BAT0/state, an interface that no longer exists on
current kernels, so treat that part as historical.

The script I use is in doc/default.sh.  Read it before copying it to
/etc/acpi/default.sh, to make sure you dont lose some other functionality
specific to your own distribution that isn't in this one.

## Options

This is a paste of `ssuspend -h`, showing the compiled-in defaults;
regenerate it after changing any option.  The helper paths shown are the
built-in fallbacks, and may differ from what `configure` detects on your
machine.

```text
$ ./src/ssuspend -h
./src/ssuspend [options] [module [ module ... ] ]

version 1.0.0

   each module argument found in /proc/modules is removed in order on the way down,
       and restored in reverse order on the way back up.

   options are as follows:
    -h  or -? or --help                 get this message
    -b  or --before                     runs /etc/ssuspend/before after lock but before ssuspending
    -a  or --after                      runs /etc/ssuspend/after after resume but before releasing lock
    -f  <file> or --file <file>         where to write the new state. defaults to /sys/power/state.
    -s  <state> or --state <state>      what state to write to the control file. defaults to 'disk'.
    -t  <seconds> or --time <seconds>   how many seconds to sleep after resume while holding the lock.
                                        defaults to 30, in order to prevent extra keypress events from
                                        causing double suspends where your laptop resumes only to
                                        suspend again immediately.
    -d  <device> or --device <device>   an optional swap device to wipe clean of stale suspend images
                                        after waking up.
    -p  or --disk                       write the hibernation method to /sys/power/disk before suspending.
                                        newer kernels require this before they will accept 'disk' on
                                        /sys/power/state.
    -F  <file> or --disk-file <file>    where to write the method. defaults to /sys/power/disk.
                                        implies -p.
    -m  <method> or --method <method>   which method to write to /sys/power/disk. defaults to 'platform'.
                                        implies -p.
    -c  <pct> or --capacity <pct>       only suspend if the battery is Discharging and its
                                        charge is below <pct> percent, from 1 to 100.  without this
                                        option the battery is never consulted.  if the condition is
                                        not met ssuspend exits 0 without suspending.
    -w  or --wait                       instead of giving up when the -c condition is not met, sleep
                                        and check again, until it is. requires -c.  no lock is held
                                        and the screen is not blanked while waiting.
    -W  <seconds> or --wait-interval <seconds>
                                        how long -w sleeps between checks. defaults to
                                        60 seconds. implies -w.
    -B  <dir> or --battery <dir>        battery to read, eg /sys/class/power_supply/BAT0.  by default
                                        the first supply under the directory below whose type is
                                        Battery is used, so this is only needed for a machine with
                                        more than one battery.
    -S  <dir> or --supply-dir <dir>     where to look for batteries when -B is not given. defaults
                                        to /sys/class/power_supply.
    -x  or --lock-x                     tells ssuspend to lock the screen before suspending
    -X  or --lock-x-nofail              tells ssuspend to lock the screen before suspending if possible
                                        but continue in any case
                                        both run xscreensaver-command as the invoking user rather than as
                                        root, keeping your environment and locating it on $PATH.
                                        every other helper runs as root from a compiled-in absolute
                                        path with a clean environment (PATH=/sbin:/bin:/usr/sbin:/usr/bin),
                                        so /etc/ssuspend/before and /etc/ssuspend/after
                                        do not inherit the calling environment.
    -l  or --syslog                     tells ssuspend to write status information to syslog(3).
                                        warnings and errors only unless -v is also given.
    -v  or --verbose                    tells ssuspend to write status information to standard error.
    -D  or --dryrun                     causes ssuspend to print out what it would do to stderr.
                                        implies verbose.

   build time vs run time:

    -f, -F, -d, -B and -S say where a privileged process reads and writes, so
    they are refused when ssuspend is running from its setuid or setgid bit.
    that policy belongs to whoever installed it: the defaults above are
    compiled in, and configure's --with-grep, --with-dd, --with-mkswap,
    --with-rmmod, --with-modprobe, --with-before-script, --with-after-script,
    --with-power-supply-path and --with-safe-path set them.  run as root, or
    build without the setuid bit, and all five work normally -- which is how to
    test them.

    -t, -W, -s and -m are allowed either way.  with the paths pinned they only write a
    word to the control files above, and the kernel rejects any word it does
    not know, so picking 'mem' instead of 'disk' stays available to everyone.

    the programs ssuspend runs as root have no run time override at all, by
    design: one would be an arbitrary-command-as-root option.
$
```
