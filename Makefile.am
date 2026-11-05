## Process this file with automake to create Makefile.in

## No AUTOMAKE_OPTIONS here: configure.ac's AM_INIT_AUTOMAKE([foreign]) sets
## the strictness for the whole tree, and this used to say "gnu", which
## contradicted it and made automake demand GNU boilerplate files.

SUBDIRS           = src

## doc/ holds an example script and no build rules, so it has no Makefile.am
## of its own.  Ship the example from here instead.
EXTRA_DIST        = doc/default.sh
