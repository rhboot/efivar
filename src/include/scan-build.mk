SCAN_BUILD ?= $(shell x=$$(which --skip-alias --skip-functions scan-build 2>/dev/null) ; [ -n "$$x" ] && echo 1)
ifeq ($(SCAN_BUILD),)
	SCAN_BUILD_ERROR = $(error scan-build not found)
endif

scan-test : ; $(SCAN_BUILD_ERROR)

scan-clean : clean
	@if [[ -d scan-results ]]; then rm -rf scan-results && echo "removed 'scan-results'"; fi

scan-build : | scan-test
scan-build : clean
	scan-build -o scan-results make $(DASHJ) CC=clang all

scan-build-all: | scan-build
scan : | scan-build

.PHONY : scan-build scan-clean scan-build-all scan

# vim:ft=make
