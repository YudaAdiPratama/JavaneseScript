#!/bin/sh
# Nglakokake saben berkas .jws lan mbandhingake asile (stdout, stderr, lan
# kode metu) karo berkas .out sing cocog.
cd "$(dirname "$0")/.." || exit 1
pass=0
fail=0

check() {
    actual=$( (echo Yuda | ./jws "$1" 2>&1; echo "exit=$?") )
    if [ "$actual" = "$(cat "$2")" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "GAGAL: $1"
        printf '%s\n' "$actual" | diff "$2" - | head -10
    fi
}

for f in examples/*.jws; do
    check "$f" "tests/conto/$(basename "${f%.jws}").out"
done
for f in tests/program/*.jws tests/kesalahan/*.jws; do
    check "$f" "${f%.jws}.out"
done

echo "$pass lulus, $fail gagal"
[ "$fail" -eq 0 ]
