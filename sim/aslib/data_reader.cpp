#include "data_reader.hpp"
#include "common.hpp"
#include "parser_utils.hpp"
#include <string_view>

namespace as {
// FIXME: This is very lazy, untested and doesn't have any error handling
// For now it's good enough tho
auto parse_line(std::string_view line) -> std::optional<std::pair<IData, IData>> {
    line = as::trim_line(line);

    auto pos = line.find(':');
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }

    auto left = as::trim_line(line.substr(0, pos));
    auto right = as::trim_line(line.substr(pos + 1));

    const auto address = as::parse_num(left);
    if (!address) {
        return std::nullopt;
    }

    const auto value = as::parse_num(right);
    if (!value) {
        return std::nullopt;
    }

    return std::make_pair(*address, *value);
}


auto read_data(const std::filesystem::path& path) -> std::expected<sim::data_memory_container_t, std::string_view> {
    auto file = as::open_file(path);
    if (!file) {
        return std::unexpected(file.error());
    }

    const auto lines = as::get_lines(*file);

    file->close();

    sim::data_memory_container_t data_memory{};

    for (std::string_view line : lines) {
        const auto record_or_none = parse_line(line);
        if (!record_or_none) {
            return std::unexpected(std::format("Failed to parse line: '{}'", line));
        }

        const auto [address, value] = *record_or_none;
        data_memory[address] = value;
    }

    return data_memory;
}
}
