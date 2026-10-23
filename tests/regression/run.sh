#!/bin/bash
# Regression tests for the FiPE engine.
#
#   bash tests/regression/run.sh <path to BS> [case ...]
#
# Every directory cases/<name>/ holds data.graph, query.graph and expected (the embedding count, checked with
# independent subgraph matching implementations). Each case runs the FiPE engine with the CFL, LDF, NLF and DPiso
# filters (they give different candidate sets, so different FiPE code paths), one method per configuration
# directory written here (support.txt: filter <F>, order GQL, engine FiPE):
#   BS -d data.graph -q query.graph -num MAX -time_limit 60000 -o out.csv -conf <conf of F>
# The count is the last field of the method's line in out.csv, the second field is 1 when the time limit stopped it.
# A case fails when the run crashes, writes no result line or a different count. A case with a file
# time_limit (milliseconds) runs with that limit instead: it must stop within the limit plus 5 s, and its count
# may be lower than expected (a partial count, then with the time-limit field 1) but never higher. Memory errors that do not crash are only
# reported by a sanitizer build, e.g.
#   cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" && make
# The expected counts are embeddings. This branch defines FIPE_HOMOMORPHISM in configuration/config.h, which makes
# the FiPE engine multiply the label-group counts without injectivity (not an embedding count); comment it out and
# rebuild before running these tests.
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
for f in CFL LDF NLF DPiso; do
    mkdir -p "$work/conf_$f"
    printf 'filter: %s\norder: GQL\nengine: FiPE\n' "$f" > "$work/conf_$f/support.txt"
    printf 'wo_order: FiPE\n' > "$work/conf_$f/special_engine.txt"
    for x in excluded incompatible wanted src; do : > "$work/conf_$f/$x.txt"; done
done
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
        rm -f "$work/out.csv"
        (cd "$work" && $to "$bs" -d "$d/data.graph" -q "$d/query.graph" -num MAX -time_limit $limit \
              -o "$work/out.csv" -conf "$work/conf_$f" > "$work/log.txt" 2>&1)
        rc=$?
        ms=$(( ($(date +%s%N) - t0) / 1000000 ))
        line=$(grep ',FiPE,' "$work/out.csv" 2>/dev/null | tail -1)
        got=$(printf '%s' "$line" | awk -F, '{print $NF}')
        stopped=$(printf '%s' "$line" | awk -F, '{print $2}')
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
            elif [ "$got" != "$expected" ] && [ "$stopped" != 1 ]; then
                echo "FAIL $c ($f): partial count $got without the time-limit field 1"
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
