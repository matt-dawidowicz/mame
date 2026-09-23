// license:BSD-3-Clause
// copyright-holders:Matt Jordan
#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <zlib.h>

inline std::filesystem::path cdi_fixture_root()
{
	if (char const *const override_root = std::getenv("CDI_FIXTURE_ROOT"))
		return override_root;
	return std::filesystem::path("tests") / "emu" / "philips" / "fixtures";
}

inline std::vector<uint8_t> cdi_fixture_read_uncached(std::string const &relative)
{
	std::filesystem::path const path = cdi_fixture_root() / relative;
	std::ifstream file(path, std::ios::binary);
	if (!file)
		throw std::runtime_error("missing CD-i certification fixture: " + path.string());
	file.seekg(0, std::ios::end);
	std::streamoff const size = file.tellg();
	if (size < 0)
		throw std::runtime_error("cannot size CD-i certification fixture: " + path.string());
	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> data(static_cast<std::size_t>(size));
	if (size && !file.read(reinterpret_cast<char *>(data.data()), size))
		throw std::runtime_error("cannot read CD-i certification fixture: " + path.string());
	return data;
}

inline std::vector<uint8_t> const &cdi_fixture(std::string const &relative)
{
	static std::map<std::string, std::vector<uint8_t>> cache;
	auto const found = cache.find(relative);
	if (found != cache.end())
		return found->second;
	return cache.emplace(relative, cdi_fixture_read_uncached(relative)).first->second;
}

inline std::vector<uint8_t> cdi_fixture_inflate(std::string const &relative, unsigned size)
{
	auto const &compressed = cdi_fixture(relative);
	std::vector<uint8_t> result(size);
	uLongf length = size;
	if (uncompress(result.data(), &length, compressed.data(), compressed.size()) != Z_OK || length != size)
		throw std::runtime_error("cannot inflate CD-i certification fixture: " + relative);
	return result;
}
