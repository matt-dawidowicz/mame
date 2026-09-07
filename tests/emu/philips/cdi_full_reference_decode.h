// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz
#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <zlib.h>

template <std::size_t N> std::vector<uint8_t> cdi_full_bytes(char const (&encoded)[N])
{
	std::vector<uint8_t> bytes;
	bytes.reserve((N - 1) * 3 / 4);
	uint32_t bits = 0;
	unsigned count = 0;
	for (std::size_t i = 0; i + 1 < N; ++i)
	{
		char const c = encoded[i];
		if (c == '=')
			break;
		int const value = c >= 'A' && c <= 'Z'	 ? c - 'A'
						  : c >= 'a' && c <= 'z' ? c - 'a' + 26
						  : c >= '0' && c <= '9' ? c - '0' + 52
						  : c == '+'			 ? 62
						  : c == '/'			 ? 63
												 : -1;
		if (value < 0)
			throw std::runtime_error("invalid base64 reference");
		bits = (bits << 6) | unsigned(value);
		count += 6;
		if (count >= 8)
		{
			count -= 8;
			bytes.push_back(uint8_t(bits >> count));
		}
	}
	return bytes;
}

template <std::size_t N> std::vector<uint8_t> cdi_full_rgb(char const (&encoded)[N], unsigned length)
{
	auto const compressed = cdi_full_bytes(encoded);
	std::vector<uint8_t> pixels(length);
	uLongf size = pixels.size();
	if (uncompress(pixels.data(), &size, compressed.data(), compressed.size()) != Z_OK || size != length)
		throw std::runtime_error("invalid full-size reference length");
	return pixels;
}
