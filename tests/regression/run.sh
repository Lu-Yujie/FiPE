#!/bin/bash
# Regression tests for the FiPE engine.
#
#   bash tests/regression/run.sh <path to BS> [case ...]
#
# Every directory cases/<name>/ holds data.graph, query.graph and expected (the embedding count, checked with
# independent subgraph matching implementations). Each case runs with the CFL, LDF, NLF and DPiso filters
# (they give different candidate sets, so different FiPE code paths):
#   BS -d data.graph -q query.graph -filter <F> -order GQL -engine FiPE -num MAX -time_limit 60000
# A case fails when the run crashes, prints no '#Embeddings' line or prints a different count. A case with a file
# time_limit (milliseconds) runs with that limit instead: it must stop within the limit plus 5 s, and its count
# may be lower than expected (a partial count, then with 'Timed Out: 1') but never higher. Memory errors that do not crash are only
# reported by a sanitizer build, e.g.
#   cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" && make
set -u
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}   # sanitizer builds: report memory errors, not leaks
bs=${1:?usage: run.sh <path to BS> [case ...]}
shift
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
bs=$(cd "$(dirname "$bs")" && pwd)/$(basename "$bs")
[ -x "$bs" ] || { echo "run.sh: $bs is not executable"; exit 2; }
if [ $# -gt 0 ]; then cases=("$@"); else cases=($(ls "$here/cases")); fi
to=""
command -v timeout > /dev/null && to="timeout 600"
# decimal_le a b: a <= b for non-negative decimal integers of any length
decimal_le() { [ ${#1} -lt ${#2} ] || { [ ${#1} -eq ${#2} ] && [[ ! "$1" > "$2" ]]; }; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
fail=0
total=0
for c in "${cases[@]}"; do
    d="$here/cases/$c"
    expected=$(tr -d ' \n' < "$d/expected")
    limit=60000
    [ -f "$d/time_limit" ] && limit=$(tr -d ' \n' < "$d/time_limit")
    for f in CFL LDF NLF DPiso; do
        total=$((total + 1))
        t0=$(date +%s%N)
        out=$(cd "$work" && $to "$bs" -d "$d/data.graph" -q "$d/query.graph" -filter $f -order GQL -engine FiPE \
              -num MAX -time_limit $limit 2>&1)
        rc=$?
        ms=$(( ($(date +%s%N) - t0) / 1000000 ))
        got=$(printf '%s\n' "$out" | sed -n 's/^#Embeddings: //p' | tail -1)
        if [ $rc -ne 0 ] || [ -z "$got" ]; then
            echo "FAIL $c ($f): exit code $rc, no count"
            fail=$((fail + 1))
        elif [ -f "$d/time_limit" ]; then
            if [ $ms -gt $((limit + 5000)) ]; then
                echo "FAIL $c ($f): ran $ms ms with -time_limit $limit"
                fail=$((fail + 1))
            elif ! decimal_le "$got" "$expected"; then
                echo "FAIL $c ($f): count $got exceeds the total $expected"
                fail=$((fail + 1))
            elif [ "$got" != "$expected" ] && ! printf '%s\n' "$out" | grep -q '^Timed Out: 1$'; then
                echo "FAIL $c ($f): partial count $got without 'Timed Out: 1'"
                fail=$((fail + 1))
            else
                echo "ok   $c ($f): $got in $ms ms (limit $limit ms)"
            fi
        elif [ "$got" != "$expected" ]; then
            echo "FAIL $c ($f): expected $expected, got $got"
            fail=$((fail + 1))
        else
            echo "ok   $c ($f): $got"
        fi
    done
done
echo "$((total - fail))/$total passed"
[ $fail -eq 0 ]
