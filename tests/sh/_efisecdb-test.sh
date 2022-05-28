#!/bin/sh -eu

# Usage: goal result efisecdb

if ! cmp "$1" "$2" ; then
	if echo "$1" | grep -qE "\.txt$" ; then
		diff -U 200 "$1" "$2"
	else
		"$3" --dump --annotate -s none -i "$1" > "${1}.txt"
		"$3" --dump --annotate -s none -i "$2" > "${2}.txt"
		diff -U 200 "${1}.txt" "${2}.txt"
		rm "${1}.txt" "${2}.txt"
	fi
	exit 1
fi
