#!/usr/bin/env bash
# Ejecuta los programas de prueba en el simulador y verifica su resultado.
# Uso: tests/run_programs.sh [ruta/al/rvsim]
cd "$(dirname "$0")/.." || exit 1
SIM=${1:-build/rvsim}
[ -x "$SIM" ] || SIM=build/rvsim.exe
fails=0

check() {  # check <nombre> <programa> <comandos> <texto esperado>
    local out
    out=$(printf '%b' "$3" | "$SIM" "tests/programs/$2")
    out=${out//$'\r'/}
    if [[ "$out" == *"$4"* ]]; then
        echo "OK    $1"
    else
        echo "FALLO $1 (se esperaba: $4)"
        echo "$out"
        fails=$((fails + 1))
    fi
}

check "riscvtest: mem[100] = 25" riscvtest.bin 'run\nwords 100\nexit\n' '0x00000064: 0x00000019'
check "quicksort: {1,2,3,4,6,8,9}" quicksort.bin 'run\nmem 0x1000 0x101b\nexit\n' \
      '01 00 00 00 02 00 00 00 03 00 00 00 04 00 00 00
0x00001010: 06 00 00 00 08 00 00 00 09 00 00 00'
check "simetrico: a0 = 1" symmetric.bin 'run\nregs a0\nexit\n' 'a0 = 0x00000001'

exit $fails
