# rvsim: simulador RV32I

Simulador del ISA base RISC-V de 32 bits (RV32I) para la Tarea 5 de CS3051 Arquitectura de Computadores (2026-II).
Carga un programa ya compilado (binario crudo o ELF), lo ejecuta paso a paso y permite inspeccionar PC, registros y memoria desde una consola interactiva.

## Compilar

Requiere un compilador C++17 y CMake ≥ 3.16.

```bash
cmake -S . -B build
cmake --build build
```

En Windows con MSYS2 (ucrt64) agrega `-G "MinGW Makefiles"` al primer comando. El ejecutable se enlaza estático, así que `build/rvsim.exe` funciona sin las DLL de MSYS2.

## Uso

```bash
./build/rvsim tests/programs/quicksort.bin
```

```
"tests/programs/quicksort.bin" cargado a memoria (binario crudo, 256 bytes). PC = 0x00000000
> step 2
0x00000000:  00010137  lui sp, 0x10
0x00000004:  00001537  lui a0, 0x1
> run
La instrucción en 0x0000004C salta a sí misma (bucle infinito): fin del programa.
> words 0x1000 7
0x00001000: 0x00000001  (1)
...
> exit
```

| Comando | Descripción |
|---|---|
| `step [n]` / `s` | Ejecuta `n` instrucciones (1 por defecto) y muestra cada una desensamblada |
| `run` / `r` / `continue` / `c` | Ejecuta hasta el fin del programa, un breakpoint o un error |
| `pc` | Muestra el PC |
| `regs [r ...]` | Muestra los 32 registros, o solo los indicados (`x5`, `t0`, `a0`, `fp`...) |
| `mem <ini> [fin]` | Bytes de memoria en `[ini, fin]` (16 bytes por defecto) |
| `words <dir> [n]` | `n` palabras de 32 bits desde `dir`, en hex y decimal con signo |
| `disasm [dir] [n]` / `d` | Desensambla `n` instrucciones desde `dir` (PC y 10 por defecto) |
| `break <dir>` / `delete <dir>` / `breaks` | Manejo de breakpoints |
| `reset` | Recarga el programa y reinicia registros y memoria |
| `help`, `exit` | Ayuda y salida |

Los números aceptan decimal o hexadecimal (`0x1000`).

## Características

**Obligatorias**
- Las 37 instrucciones del Anexo A: loads/stores de 8/16/32 bits con y sin signo, ALU con inmediatos y registros, `lui`, `auipc`, los 6 branches, `jal` y `jalr`.
- Memoria plana de 32 bits, byte-addressable y little-endian.
- Carga de binarios crudos en la dirección `0x00000000`.
- Ejecución paso a paso e inspección de PC, registros y memoria.

**Adicionales**
- **Desensamblador**: `step` muestra cada instrucción ejecutada, y `disasm` lista el código alrededor del PC.
- **Carga de ELF32** (RISC-V little-endian): se detecta por la firma del archivo, se copian los segmentos `PT_LOAD` y el PC empieza en `e_entry`.
- **`ecall`** con las llamadas de SPIM/CPUlator (código en `a7`): 1 print int, 4 print string, 5 read int, 8 read string, 10 exit, 11 print char, 12 read char, 17 y 93 exit con código en `a0`. `ebreak` detiene la ejecución.
- **Breakpoints** y `run`.

**Errores explícitos.** Una instrucción inválida, un PC desalineado o una `ecall` no soportada detienen la ejecución con un mensaje que incluye el PC y la palabra de instrucción. Una instrucción que salta a sí misma (`j .`, con la que terminan los programas de prueba) se reporta como fin del programa.

## Diseño

| Archivo | Responsabilidad |
|---|---|
| `src/memory.h` | Memoria dispersa por páginas de 4 KiB (`unordered_map`): solo reserva lo que se escribe y lo no escrito vale 0 |
| `src/decode.h` | Extracción de campos e inmediatos con signo de los formatos R/I/S/B/U/J, y nombres ABI |
| `src/cpu.{h,cpp}` | Estado arquitectural (PC, `x[32]`, memoria) y `step()`: fetch → decode → execute → actualizar PC. `x0` se mantiene en 0 |
| `src/disasm.{h,cpp}` | Desensamblador |
| `src/loader.{h,cpp}` | Carga de binario crudo y ELF32 |
| `src/main.cpp` | Consola interactiva |

Al iniciar o tras `reset`, todos los registros (incluido `sp`) valen 0 y el PC vale `0x0` (o `e_entry` en un ELF). Los programas de prueba inicializan `sp` por su cuenta.

## Pruebas

```bash
./build/test_cpu              # tests unitarios: cada instrucción, little-endian, ecall, desensamblador
bash tests/run_programs.sh    # programas completos
```

| Programa | Resultado esperado |
|---|---|
| `riscvtest.bin` | escribe 25 (`0x19`) en la dirección 100 (`0x64`) |
| `quicksort.bin` | deja `{1, 2, 3, 4, 6, 8, 9}` en `0x1000` |
| `symmetric.bin` | termina con `a0 = 1` |

Las fuentes `.s` están en `tests/programs/`. `quicksort.bin` y `symmetric.bin` se compilaron en CPUlator (https://cpulator.01xz.net/?sys=rv32): el código máquina que muestra su desensamblado está guardado en los `.hex` del mismo directorio y se convirtió a binario little-endian. `riscvtest.bin` se armó con el código máquina que trae su propio listado. Otra forma de obtener un binario es exportar la memoria en formato *Raw* desde `0x0000`, como indica el Anexo B del enunciado.
