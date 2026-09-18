#pragma once

#include "sim.hpp"
#include <filesystem>
namespace as {
auto read_data(const std::filesystem::path& path) -> std::expected<sim::data_memory_container_t, std::string_view>;
}
