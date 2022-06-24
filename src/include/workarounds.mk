# SPDX-License-Identifier: SPDX-License-Identifier: LGPL-2.1-or-later
#
# workarounds.mk - workarounds for weird stuff behavior

LD_FLAVOR := $(shell LC_ALL=C $(LD) --version | grep -E '^(LLD|GNU ld)'|sed 's/ .*//g')
LD_VERSION := $(shell LC_ALL=C $(LD) --version | grep -E '^(LLD|GNU ld)'|sed 's/.* //')
# 2.35 is definitely broken and 2.36 seems to work
LD_DASH_T := $(shell \
	if [ "x${LD_FLAVOR}" = xLLD ] ; then \
		echo '-T' ; \
	elif [ "x${LD_FLAVOR}" = xGNU ] ; then \
		if echo "${LD_VERSION}" | grep -q -E '^2\.3[6789]|^2\.[456789]|^[3456789]|^[[:digit:]][[:digit:]]' ; then \
			echo '-T' ; \
		else \
			echo "" ; \
		fi ; \
	else \
		echo "Your linker is not supported" ; \
		exit 1 ; \
	fi)

export LD_DASH_T

# vim:ft=make
