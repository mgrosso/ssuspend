#!/bin/bash  -x
## ----------------------------------------------------------------------
## ----------------------------------------------------------------------
##
## File:      auto.sh
## Author:    mgrosso Matthew E Grosso
## Created:   Wed Dec 17 01:50:20 EST 2003 on erasmus.erasmus.org
## Project:   
## Purpose:   
## 
## Copyright (c) 2003 Matthew E Grosso. All Rights Reserved.
## 
## $Id: auto.sh,v 1.2 2004/01/25 04:32:46 mgrosso Exp $
## ----------------------------------------------------------------------
## ----------------------------------------------------------------------

if [ $# -gt 0 ] ; then
    if [ $1 = "--clean" ] ; then
        find . -name Makefile -exec rm -f {} \;
        find . -name Makefile.in -exec rm -f {} \;
        rm -rf .config.env inc/config.h aclocal.m4 autom4te.cache configure config.status
        exit 0
    fi
fi

#grep -q CONFIG_H_IN_EXTRA inc/config.h.in 2>/dev/null
#if [ $? -ne 0 ] ; then
#    cat config.h.in.extra >>inc/config.h.in
#fi

aclocal || exit 1;
autoheader || exit 1;
automake -i  || exit 1;
autoconf || exit 1;

