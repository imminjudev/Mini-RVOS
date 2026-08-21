#!/bin/sh

set -eu

KERNEL="build/kernel.elf"

if [ ! -f "$KERNEL" ]; then
    echo "[FAIL] kernel image not found: $KERNEL"
    exit 1
fi

OUTPUT="$(mktemp)"

cleanup()
{
    rm -f "$OUTPUT"
}

trap cleanup EXIT

STATUS=0

{
    # QEMU/OpenSBI가 부팅되고
    # Mini-RVOS shell이 stdin을 받을 준비가 될 때까지 기다린다.
    sleep 2

    printf 'help\n'
    printf 'pid\n'
    printf 'echo hello\n'
    printf 'cat /hello.txt\n'
    printf 'memtest\n'
    printf 'exit\n'
} |
    timeout 10s \
        qemu-system-riscv64 \
        -machine virt \
        -m 128M \
        -nographic \
        -bios default \
        -kernel "$KERNEL" \
        >"$OUTPUT" 2>&1 ||
    STATUS=$?

# SYS_EXIT 이후 Mini-RVOS는 CPU를 park한다.
# 따라서 timeout의 124 종료 코드는 정상이다.
if [ "$STATUS" -ne 0 ] &&
   [ "$STATUS" -ne 124 ]; then

    echo "[FAIL] QEMU terminated unexpectedly"
    cat "$OUTPUT"
    exit 1
fi

check_output()
{
    PATTERN="$1"

    if ! grep -Fq "$PATTERN" "$OUTPUT"; then
        echo "[FAIL] missing output:"
        echo "       $PATTERN"
        echo
        echo "----- QEMU OUTPUT -----"
        cat "$OUTPUT"
        echo "-----------------------"
        exit 1
    fi
}

check_output "[OK] filesystem initialized"
check_output "[OK] shell process created"
check_output "[OK] entering user shell"

check_output "Mini-RVOS shell"
check_output "commands: help pid echo cat exit memtest"
check_output "hello"
check_output "Hello from /hello.txt in Mini-RVOS!"
check_output "[OK] user pointer validation"
check_output "Mini-RVOS shell exited."

if grep -Fq "[FAIL] unexpected exception" "$OUTPUT"; then
    echo "[FAIL] unexpected exception detected"
    cat "$OUTPUT"
    exit 1
fi

if grep -Fq "[FAIL] unexpected interrupt" "$OUTPUT"; then
    echo "[FAIL] unexpected interrupt detected"
    cat "$OUTPUT"
    exit 1
fi

if grep -Fq "[FAIL] user pointer validation" "$OUTPUT"; then
    echo "[FAIL] user pointer validation failed"
    cat "$OUTPUT"
    exit 1
fi

echo "[OK] Mini-RVOS smoke test passed"
