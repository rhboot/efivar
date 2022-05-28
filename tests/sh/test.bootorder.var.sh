#!/bin/sh -eu

"$1" -i "${2}/test.bootorder.var.goal.var" -e test.bootorder.var.0.result.var
"$1" -i test.bootorder.var.0.result.var -e test.bootorder.var.1.result.var -D
cmp "${2}/test.bootorder.var.goal.var" test.bootorder.var.1.result.var
