// rvsim: simulador interactivo del ISA RV32I.
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "cpu.h"
#include "decode.h"
#include "disasm.h"
#include "loader.h"

namespace {

const char* const HELP = R"(Comandos:
  step [n]            ejecuta n instrucciones (por defecto 1)       alias: s
  run                 ejecuta hasta fin, breakpoint o error         alias: r, continue, c
  pc                  muestra el PC
  regs [r ...]        muestra todos los registros o solo los dados (x5, t0, a0...)
  mem <ini> [fin]     muestra los bytes de memoria en [ini, fin] (por defecto 16 bytes)
  words <dir> [n]     muestra n palabras de 32 bits desde dir (por defecto 1)
  disasm [dir] [n]    desensambla n instrucciones desde dir (por defecto PC, 10)  alias: d
  break <dir>         agrega un breakpoint                          alias: b
  delete <dir>        quita un breakpoint
  breaks              lista los breakpoints
  reset               recarga el programa y reinicia el estado
  help                muestra esta ayuda
  exit                sale del simulador                            alias: quit, q
)";

constexpr uint64_t RUN_LIMIT = 100'000'000;

uint32_t parse_num(const std::string& s) {
    size_t used = 0;
    unsigned long long v = std::stoull(s, &used, 0);  // acepta 0x.., decimal
    if (used != s.size() || v > 0xffffffffull) throw std::invalid_argument(s);
    return uint32_t(v);
}

int parse_reg(const std::string& s) {
    if (s == "fp") return 8;
    for (int i = 0; i < 32; ++i)
        if (s == rv::ABI_NAMES[i] || s == "x" + std::to_string(i)) return i;
    return -1;
}

std::string reg_label(int i) {
    std::ostringstream o;
    o << "x" << i << " (" << rv::ABI_NAMES[i] << ")";
    return o.str();
}

class Shell {
public:
    explicit Shell(std::string path) : path_(std::move(path)) {}

    void load() {
        cpu_.mem.clear();
        LoadInfo info = load_program(path_, cpu_.mem);
        cpu_.reset(info.entry);
        std::cout << '"' << path_ << "\" cargado a memoria (" << info.format << ", "
                  << info.bytes << " bytes). PC = " << hex32(cpu_.pc) << "\n";
    }

    // Procesa una línea. Devuelve false si el usuario pidió salir.
    bool command(const std::string& line) {
        std::istringstream ss(line);
        std::string cmd;
        std::vector<std::string> args;
        if (!(ss >> cmd)) return true;
        for (std::string a; ss >> a;) args.push_back(a);

        try {
            if (cmd == "exit" || cmd == "quit" || cmd == "q") return false;
            else if (cmd == "help" || cmd == "h") std::cout << HELP;
            else if (cmd == "step" || cmd == "s") step(args.empty() ? 1 : parse_num(args[0]));
            else if (cmd == "run" || cmd == "r" || cmd == "continue" || cmd == "c") run();
            else if (cmd == "pc") std::cout << "pc = " << hex32(cpu_.pc) << "\n";
            else if (cmd == "regs") regs(args);
            else if (cmd == "mem" && !args.empty())
                mem(parse_num(args[0]), args.size() > 1 ? parse_num(args[1]) : parse_num(args[0]) + 15);
            else if (cmd == "words" && !args.empty())
                words(parse_num(args[0]), args.size() > 1 ? parse_num(args[1]) : 1);
            else if (cmd == "disasm" || cmd == "d")
                disasm(args.empty() ? cpu_.pc : parse_num(args[0]), args.size() > 1 ? parse_num(args[1]) : 10);
            else if ((cmd == "break" || cmd == "b") && !args.empty()) {
                breaks_.insert(parse_num(args[0]));
                std::cout << "Breakpoint en " << hex32(parse_num(args[0])) << "\n";
            } else if (cmd == "delete" && !args.empty()) {
                breaks_.erase(parse_num(args[0]));
            } else if (cmd == "breaks") {
                if (breaks_.empty()) std::cout << "No hay breakpoints.\n";
                for (uint32_t b : breaks_) std::cout << hex32(b) << "\n";
            } else if (cmd == "reset") load();
            else std::cout << "Comando inválido o faltan argumentos. Escribe 'help'.\n";
        } catch (const std::invalid_argument&) {
            std::cout << "Número inválido.\n";
        } catch (const std::out_of_range&) {
            std::cout << "Número fuera de rango.\n";
        } catch (const SimError& e) {
            std::cout << "Error: " << e.what() << "\n";
        }
        return true;
    }

private:
    CPU cpu_;
    std::string path_;
    std::set<uint32_t> breaks_;

    void print_inst(uint32_t addr) {
        uint32_t inst = cpu_.mem.read32(addr);
        std::cout << hex32(addr) << ":  " << std::hex << std::setw(8) << std::setfill('0') << inst
                  << std::dec << std::setfill(' ') << "  " << disassemble(inst, addr) << "\n";
    }

    // Ejecuta una instrucción. Devuelve false si la ejecución debe detenerse.
    bool exec_one(bool verbose) {
        const uint32_t old_pc = cpu_.pc;
        if (verbose) print_inst(old_pc);
        try {
            cpu_.step();
        } catch (const SimError& e) {
            std::cout << "Error: " << e.what() << "\n";
            return false;
        }
        if (cpu_.halted) {
            std::cout << "\nPrograma terminado con código " << cpu_.exit_code << ".\n";
            return false;
        }
        if (cpu_.pc == old_pc) {  // p. ej. "j ." al final de los programas
            std::cout << "La instrucción en " << hex32(old_pc)
                      << " salta a sí misma (bucle infinito): fin del programa.\n";
            return false;
        }
        return true;
    }

    void step(uint32_t n) {
        for (uint32_t i = 0; i < n; ++i)
            if (!exec_one(true)) return;
    }

    void run() {
        for (uint64_t i = 0; i < RUN_LIMIT; ++i) {
            if (!exec_one(false)) return;
            if (breaks_.count(cpu_.pc)) {
                std::cout << "Breakpoint alcanzado:\n";
                print_inst(cpu_.pc);
                return;
            }
        }
        std::cout << "Se alcanzó el límite de " << RUN_LIMIT << " instrucciones. PC = " << hex32(cpu_.pc) << "\n";
    }

    void regs(const std::vector<std::string>& names) {
        if (names.empty()) {
            for (int i = 0; i < 32; ++i)
                std::cout << std::left << std::setw(10) << reg_label(i) << std::right << " = "
                          << hex32(cpu_.x[i]) << (i % 4 == 3 ? "\n" : "    ");
            return;
        }
        for (const std::string& n : names) {
            int r = parse_reg(n);
            if (r < 0) std::cout << "Registro desconocido: " << n << "\n";
            else std::cout << n << " = " << hex32(cpu_.x[r]) << " (" << int32_t(cpu_.x[r]) << ")\n";
        }
    }

    void mem(uint32_t start, uint32_t end) {
        if (end < start) throw SimError("rango inválido");
        std::cout << "Memoria (" << hex32(start) << "-" << hex32(end) << "):";
        std::cout << std::uppercase << std::hex << std::setfill('0');
        for (uint64_t a = start; a <= end; ++a) {
            if ((a - start) % 16 == 0 && end - start >= 16) std::cout << "\n" << hex32(uint32_t(a)) << ":";
            std::cout << " " << std::setw(2) << int(cpu_.mem.read8(uint32_t(a)));
        }
        std::cout << std::dec << std::nouppercase << std::setfill(' ') << "\n";
    }

    void words(uint32_t addr, uint32_t n) {
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t a = addr + 4 * i, v = cpu_.mem.read32(a);
            std::cout << hex32(a) << ": " << hex32(v) << "  (" << int32_t(v) << ")\n";
        }
    }

    void disasm(uint32_t addr, uint32_t n) {
        for (uint32_t i = 0; i < n; ++i) {
            std::cout << (addr + 4 * i == cpu_.pc ? "-> " : "   ");
            print_inst(addr + 4 * i);
        }
    }
};

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Uso: " << argv[0] << " <programa.bin | programa.elf>\n";
        return 1;
    }
    Shell shell(argv[1]);
    try {
        shell.load();
    } catch (const SimError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    std::cout << "Escribe 'help' para ver los comandos.\n";

    std::string line;
    while (std::cout << "> " << std::flush, std::getline(std::cin, line))
        if (!shell.command(line)) break;
    std::cout << "¡Hasta la próxima!\n";
    return 0;
}
