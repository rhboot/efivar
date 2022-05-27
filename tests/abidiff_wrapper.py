#!/usr/bin/env python3

import subprocess
import sys

ABIDIFF_ERROR = 1
ABIDIFF_USAGE_ERROR = 2
ABIDIFF_ABI_CHANGE = 4
ABIDIFF_ABI_INCOMPATIBLE_CHANGE = 8


pr = subprocess.run(sys.argv[1:])
ret = pr.returncode

# An error code of 4 means there is a change but it is not a necessarily a breaking change.
if ret == ABIDIFF_ABI_CHANGE:
	sys.stderr.write("WARNING! abidiff reports there is an abi change but isn't sure if it is breaking.\nPlease check this manually!\n")
	exit(77)
else:
	exit(ret)
