#pragma once

#include <charconv>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <system_error>

inline std::optional<std::size_t> ExecutionTestGroup(int argc, char** argv) {
    if (argc == 1) return std::nullopt;
    if (argc != 2) throw std::invalid_argument("expected one test group in [0, 31]");
    const std::string_view argument(argv[1]);
    std::size_t group = 0;
    const auto parsed = std::from_chars(argument.data(), argument.data() + argument.size(), group);
    if (parsed.ec != std::errc{} || parsed.ptr != argument.data() + argument.size() || group >= 32u) throw std::invalid_argument("expected one test group in [0, 31]");
    return group;
}
