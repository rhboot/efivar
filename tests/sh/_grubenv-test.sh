#!/bin/sh -eu

# Usage: editenv dir testname torun

"$1" "${3}.result.env" create
"$1" "${3}.result.env" set debug=all,-scripting,-lexer
truncate -s 512 "${3}.result.env"
eval "$4"
if grep -q "Do not edit" "${3}.result.env" ; then
	cmp "${3}.result.var" "${2}/${3}.new.goal.var"
else
	cmp "${3}.result.var" "${2}/${3}.old.goal.var"
fi
rm -f "${3}.result."*
