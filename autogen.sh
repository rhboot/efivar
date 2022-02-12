#!/bin/sh

libtoolize
aclocal
autoheader
autoconf
automake --add-missing
