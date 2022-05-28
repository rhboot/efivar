#!/bin/sh -eu

"$1" -i "${2}/test.conin.var.goal.var" -e test.conin.var.0.result.var
"$1" -i test.conin.var.0.result.var -e test.conin.var.1.result.var -D
cmp "${2}/test.conin.var.goal.var" test.conin.var.1.result.var
