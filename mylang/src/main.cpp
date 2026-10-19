// Entry point: `mylang script.my` runs a file, `mylang` alone starts a REPL.
// `--vm` runs programs on the bytecode VM instead of the tree-walking interpreter,
// and `--dump` (which implies --vm) also prints the compiled bytecode before running it.
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#include "interpreter.hpp"
#include "vm/vm.hpp"

using Runner = std::function<int(const std::string&)>;

static int runFile(const std::string& path, const Runner& run) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Could not open file: " << path << "\n";
        return 74;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return run(buffer.str());
}

static void repl(const Runner& run) {
    std::cout << "mylang REPL. Type 'exit' to quit.\n";
    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        size_t last = line.find_last_not_of(" \t\r");
        if (last == std::string::npos) continue;
        line.erase(last + 1);
        if (line == "exit" || line == "quit") break;
        // Convenience: allow typing `print 1 + 2` without the final semicolon.
        if (line.back() != ';' && line.back() != '}') line += ';';
        run(line);  // errors are printed; the session keeps going
    }
}

int main(int argc, char** argv) {
    bool useVm = false;
    bool dump = false;
    std::string path;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--vm") {
            useVm = true;
        } else if (arg == "--dump") {
            useVm = dump = true;
        } else if (path.empty() && arg.rfind("--", 0) != 0) {
            path = arg;
        } else {
            std::cerr << "Usage: mylang [--vm] [--dump] [script.my]\n";
            return 64;
        }
    }

    Interpreter interpreter;
    vm::VM machine(dump);
    Runner run = [&](const std::string& source) {
        return useVm ? machine.run(source) : interpreter.run(source);
    };

    if (!path.empty()) return runFile(path, run);
    repl(run);
    return 0;
}
