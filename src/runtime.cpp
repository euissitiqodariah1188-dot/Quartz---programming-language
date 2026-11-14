#include "../include/quartz.h"
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace quartz {

// ============================================================
//  FATAL ERROR
// ============================================================
namespace internal {
[[noreturn]] void fatal_error(const char* file, int line,
                               const char* func, const char* msg) noexcept {
    std::fprintf(stderr,
        "\n[QUARTZ FATAL ERROR]\n"
        "  File: %s\n"
        "  Line: %d\n"
        "  Func: %s\n"
        "  Msg:  %s\n",
        file, line, func, msg);
    std::fflush(stderr);
    std::abort();
}
} // namespace internal

// ============================================================
//  QuartzValue::to_string
// ============================================================
std::string QuartzValue::to_string() const {
    switch (kind) {
        case Kind::NIL:     return "nil";
        case Kind::BOOL:    return data.b ? "true" : "false";
        case Kind::INT64:   return std::to_string(data.i);
        case Kind::FLOAT64: {
            // Smart float formatting: strip trailing zeros
            char buf[64];
            if (std::isinf(data.f))  return data.f > 0 ? "inf" : "-inf";
            if (std::isnan(data.f))  return "NaN";
            int n = std::snprintf(buf, sizeof(buf), "%.15g", data.f);
            return std::string(buf, static_cast<size_t>(n));
        }
        case Kind::STRING:  return str_data;
        case Kind::ARRAY: {
            std::string out = "[";
            for (size_t i = 0; i < arr_data.size(); ++i) {
                if (i > 0) out += ", ";
                out += arr_data[i].to_string();
            }
            out += "]";
            return out;
        }
        case Kind::DICT: {
            std::string out = "{";
            bool first = true;
            for (const auto& kv : dict_data) {
                if (!first) out += ", ";
                out += kv.first + ": " + kv.second.to_string();
                first = false;
            }
            out += "}";
            return out;
        }
        case Kind::POINTER: {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "0x%016lx",
                reinterpret_cast<unsigned long>(data.ptr));
            return std::string(buf);
        }
        case Kind::FUNCTION: return "<function>";
        default: return "<unknown>";
    }
}

// ============================================================
//  QuartzValue::to_numeric
// ============================================================
double QuartzValue::to_numeric() const noexcept {
    switch (kind) {
        case Kind::INT64:   return static_cast<double>(data.i);
        case Kind::FLOAT64: return data.f;
        case Kind::BOOL:    return data.b ? 1.0 : 0.0;
        case Kind::STRING:
            try { return std::stod(str_data); } catch (...) { return 0.0; }
        default: return 0.0;
    }
}

// ============================================================
//  QuartzValue::to_bool
// ============================================================
bool QuartzValue::to_bool() const noexcept {
    switch (kind) {
        case Kind::NIL:     return false;
        case Kind::BOOL:    return data.b;
        case Kind::INT64:   return data.i != 0;
        case Kind::FLOAT64: return data.f != 0.0 && !std::isnan(data.f);
        case Kind::STRING:  return !str_data.empty();
        case Kind::ARRAY:   return !arr_data.empty();
        case Kind::DICT:    return !dict_data.empty();
        case Kind::POINTER: return data.ptr != nullptr;
        default: return false;
    }
}

} // namespace quartz
