// Rene: a small dynamically-typed scripting language, interpreted by C++.
//
// Usage:
//   rene                 start the interactive REPL
//   rene <file>.rene     run a Rene program
//   rene --version       print the version
//   rene --help          print usage

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "config.hpp"
#include "errors.hpp"
#include "interpreter.hpp"
#include "lexer.hpp"
#include "parser.hpp"

namespace {

// Exit codes follow the BSD sysexits convention.
constexpr int EXIT_USAGE = 64;
constexpr int EXIT_SYNTAX = 65;
constexpr int EXIT_NO_INPUT = 66;
constexpr int EXIT_RUNTIME = 70;

void printUsage() {
    std::cout << "Usage:\n"
              << "  rene                 start the interactive REPL\n"
              << "  rene <file>" << rene::config::FILE_EXTENSION << "     run a "
              << rene::config::LANGUAGE_NAME << " program\n"
              << "  rene --version       print the version\n"
              << "  rene --help          print this message\n";
}

// Lexes, parses and runs `source`. Returns a process exit code.
int runSource(rene::Interpreter& interpreter, const std::string& source) {
    try {
        rene::Lexer lexer(source);
        rene::Parser parser(lexer.tokenize());
        interpreter.run(parser.parse());
        return 0;
    } catch (const rene::ReneError& error) {
        std::cout.flush();
        std::cerr << error.format() << '\n';
        return error.isSyntaxError() ? EXIT_SYNTAX : EXIT_RUNTIME;
    }
}

int runFile(const std::string& path) {
    namespace fs = std::filesystem;
    if (fs::path(path).extension() != rene::config::FILE_EXTENSION) {
        std::cerr << "error: '" << path << "' is not a " << rene::config::LANGUAGE_NAME << " file (expected a '"
                  << rene::config::FILE_EXTENSION << "' extension)\n";
        return EXIT_USAGE;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "error: cannot open '" << path << "'\n";
        return EXIT_NO_INPUT;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Windows editors often save UTF-8 files with a byte-order mark; skip it.
    if (source.rfind("\xEF\xBB\xBF", 0) == 0) source.erase(0, 3);

    rene::Interpreter interpreter;
    return runSource(interpreter, source);
}

int runRepl() {
    std::cout << rene::config::LANGUAGE_NAME << " " << rene::config::VERSION
              << " REPL - type 'exit' to quit\n";
    rene::Interpreter interpreter;  // shared across lines so variables persist
    std::string line;
    while (true) {
        std::cout << "rene> " << std::flush;
        if (!std::getline(std::cin, line) || line == "exit") break;
        if (line.empty()) continue;
        runSource(interpreter, line);
    }
    std::cout << '\n';
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 1) return runRepl();
    if (argc > 2) {
        printUsage();
        return EXIT_USAGE;
    }

    std::string arg = argv[1];
    if (arg == "--help" || arg == "-h") {
        printUsage();
        return 0;
    }
    if (arg == "--version" || arg == "-v") {
        std::cout << rene::config::LANGUAGE_NAME << " " << rene::config::VERSION << '\n';
        return 0;
    }
    return runFile(arg);
}
