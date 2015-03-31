#!/bin/sh
aclocal -I m4
autoconf
automake --foreign --add-missing
autoheader