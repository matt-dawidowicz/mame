// license:BSD-3-Clause
// copyright-holders:Matt Jordan
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

inline std::filesystem::path cdi_test_asset_path(char const *name)
{
	std::filesystem::path const relative = std::filesystem::path("tests") / "emu" / "philips" / "data" / name;
	std::filesystem::path const beside_source = std::filesystem::path(__FILE__).parent_path() / "data" / name;
	for (auto const &candidate : {relative, beside_source})
		if (std::filesystem::exists(candidate))
			return candidate;
	throw std::runtime_error(std::string("CD-i test asset not found: ") + name);
}

inline std::vector<uint8_t> cdi_load_test_asset(char const *name)
{
	auto const path = cdi_test_asset_path(name);
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream)
		throw std::runtime_error("Unable to open CD-i test asset: " + path.string());
	auto const end = stream.tellg();
	if (end < 0)
		throw std::runtime_error("Unable to size CD-i test asset: " + path.string());
	std::vector<uint8_t> data(static_cast<std::size_t>(end));
	stream.seekg(0);
	if (!data.empty() && !stream.read(reinterpret_cast<char *>(data.data()), data.size()))
		throw std::runtime_error("Unable to read CD-i test asset: " + path.string());
	return data;
}
