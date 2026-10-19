// Entry point: `mylang script.my` runs a file, `mylang` alone starts a REPL.
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "interpreter.hpp"

static int runFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Could not open file: " << path << "\n";
        return 74;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    Interpreter interpreter;
    return interpreter.run(buffer.str());
}

static void repl() {
    Interpreter interpreter;
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
        interpreter.run(line);  // errors are printed; the session keeps going
    }
}

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "Usage: mylang [script.my]\n";
        return 64;
    }
    if (argc == 2) return runFile(argv[1]);
    repl();
    return 0;
}
