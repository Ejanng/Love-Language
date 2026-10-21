#pragma once

#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace rene {

class Interpreter;
struct Callable;
struct ListObject;

// A runtime value: nil, bool, number, string, list or function.
using Value = std::variant<std::monostate, bool, double, std::string,
                           std::shared_ptr<ListObject>, std::shared_ptr<Callable>>;

struct ListObject {
    std::vector<Value> items;
};

struct Callable {
    virtual ~Callable() = default;
    virtual std::string name() const = 0;
    virtual int minArity() const = 0;
    virtual int maxArity() const = 0;
    virtual Value call(Interpreter& interpreter, std::vector<Value>& args, int line) = 0;
};

inline bool isNil(const Value& v) { return std::holds_alternative<std::monostate>(v); }
inline bool isNumber(const Value& v) { return std::holds_alternative<double>(v); }
inline bool isString(const Value& v) { return std::holds_alternative<std::string>(v); }
inline bool isList(const Value& v) { return std::holds_alternative<std::shared_ptr<ListObject>>(v); }
inline bool isCallable(const Value& v) { return std::holds_alternative<std::shared_ptr<Callable>>(v); }

// Only nil and false are falsy.
inline bool isTruthy(const Value& v) {
    if (isNil(v)) return false;
    if (auto b = std::get_if<bool>(&v)) return *b;
    return true;
}

inline std::string typeName(const Value& v) {
    switch (v.index()) {
        case 0: return "nil";
        case 1: return "bool";
        case 2: return "number";
        case 3: return "string";
        case 4: return "list";
        default: return "function";
    }
}

inline std::string formatNumber(double d) {
    if (std::isfinite(d) && std::floor(d) == d && std::fabs(d) < 1e15) {
        return std::to_string(static_cast<long long>(d));
    }
    std::ostringstream out;
    out << std::setprecision(14) << d;
    return out.str();
}

// `quoteStrings` is used when printing strings nested inside lists.
inline std::string stringify(const Value& v, bool quoteStrings = false) {
    switch (v.index()) {
        case 0: return "nil";
        case 1: return std::get<bool>(v) ? "true" : "false";
        case 2: return formatNumber(std::get<double>(v));
        case 3: return quoteStrings ? "\"" + std::get<std::string>(v) + "\"" : std::get<std::string>(v);
        case 4: {
            std::string out = "[";
            const auto& items = std::get<std::shared_ptr<ListObject>>(v)->items;
            for (size_t i = 0; i < items.size(); ++i) {
                if (i > 0) out += ", ";
                out += stringify(items[i], true);
            }
            return out + "]";
        }
        default: return "<fn " + std::get<std::shared_ptr<Callable>>(v)->name() + ">";
    }
}

}  // namespace rene
