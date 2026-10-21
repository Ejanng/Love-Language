#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace rene {

// Every error the language reports (syntax or runtime) carries a kind and a line.
class ReneError : public std::runtime_error {
public:
    ReneError(std::string kind, int line, const std::string& message)
        : std::runtime_error(message), kind_(std::move(kind)), line_(line) {}

    const std::string& kind() const { return kind_; }
    int line() const { return line_; }

    std::string format() const {
        return "[line " + std::to_string(line_) + "] " + kind_ + ": " + what();
    }

    bool isSyntaxError() const { return kind_ == "SyntaxError"; }

private:
    std::string kind_;
    int line_;
};

}  // namespace rene
