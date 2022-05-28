#!/bin/sh -eu

"$1" -i "${2}/test.esl.sha256.addition.unsorted.esl.goal" -A > test.esl.dump.x509.sha256.result.txt
cmp "${2}/test.esl.dump.x509.sha256.goal.txt" test.esl.sha256.addition.unsorted.result.txt

