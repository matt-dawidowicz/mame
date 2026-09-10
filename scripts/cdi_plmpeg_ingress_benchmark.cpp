// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#define PLM_NO_STDIO
#define PL_MPEG_IMPLEMENTATION
#include "../3rdparty/pl_mpeg/pl_mpeg.h"

namespace
{
constexpr std::size_t PACKET_BYTES = 2324;
constexpr std::size_t PACKETS_PER_ROUND = 4096;
constexpr unsigned ROUNDS = 5;
constexpr unsigned EQUIVALENCE_CYCLES = 257;

std::array<uint8_t, PACKET_BYTES> make_packet()
{
	std::array<uint8_t, PACKET_BYTES> packet{};
	uint32_t state = 0x4d504547U;
	for (uint8_t &byte : packet)
	{
		state = state * 1664525U + 1013904223U;
		byte = uint8_t(state >> 24);
	}
	return packet;
}

double median(std::vector<double> values)
{
	std::sort(values.begin(), values.end());
	return values[values.size() / 2];
}

template <std::size_t Chunk>
void write_packet(plm_buffer_t *buffer, std::array<uint8_t, PACKET_BYTES> const &packet)
{
	for (std::size_t offset = 0; offset < PACKET_BYTES; offset += Chunk)
	{
		std::size_t const count = std::min<std::size_t>(Chunk, PACKET_BYTES - offset);
		plm_buffer_write(buffer, const_cast<uint8_t *>(packet.data() + offset), count);
	}
}

bool logical_buffers_equal(plm_buffer_t const *a, plm_buffer_t const *b)
{
	std::size_t const a_offset = a->bit_index >> 3;
	std::size_t const b_offset = b->bit_index >> 3;
	std::size_t const a_remaining = a->length - a_offset;
	std::size_t const b_remaining = b->length - b_offset;
	if (a_remaining != b_remaining || (a->bit_index & 7U) != (b->bit_index & 7U))
		return false;
	return a_remaining == 0
		|| std::memcmp(a->bytes + a_offset, b->bytes + b_offset, a_remaining) == 0;
}

bool verify_byte_word_equivalence(std::array<uint8_t, PACKET_BYTES> const &packet)
{
	plm_buffer_t *byte_buffer = plm_buffer_create_with_capacity(256 * 1024);
	plm_buffer_t *word_buffer = plm_buffer_create_with_capacity(256 * 1024);
	if (!byte_buffer || !word_buffer)
		std::abort();

	bool equivalent = true;
	for (unsigned cycle = 0; cycle < EQUIVALENCE_CYCLES && equivalent; ++cycle)
	{
		write_packet<1>(byte_buffer, packet);
		write_packet<2>(word_buffer, packet);
		equivalent = logical_buffers_equal(byte_buffer, word_buffer);
		if (!equivalent)
			break;

		// Force both dynamic rings through the same consumed/refill transition
		// exercised by the timed benchmark.  The physical backing layout may be
		// compacted at different individual write calls; the logical unread byte
		// stream must nevertheless remain exactly identical.
		plm_buffer_skip(byte_buffer, PACKET_BYTES * 8);
		plm_buffer_skip(word_buffer, PACKET_BYTES * 8);
		equivalent = logical_buffers_equal(byte_buffer, word_buffer);
	}

	plm_buffer_destroy(byte_buffer);
	plm_buffer_destroy(word_buffer);
	return equivalent;
}

template <std::size_t Chunk>
double run_once(std::array<uint8_t, PACKET_BYTES> const &packet, volatile std::size_t &sink)
{
	plm_buffer_t *buffer = plm_buffer_create_with_capacity(256 * 1024);
	if (!buffer)
		std::abort();

	auto const start = std::chrono::steady_clock::now();
	for (std::size_t packet_index = 0; packet_index < PACKETS_PER_ROUND; ++packet_index)
	{
		write_packet<Chunk>(buffer, packet);

		// Model a decoder consuming the complete PES payload before the next one
		// arrives.  The following write must therefore perform the same dynamic
		// buffer discard/rewind work as normal DVC pump/refill traffic.
		plm_buffer_skip(buffer, PACKET_BYTES * 8);
		sink += plm_buffer_get_remaining(buffer);
	}
	auto const finish = std::chrono::steady_clock::now();
	plm_buffer_destroy(buffer);

	return std::chrono::duration<double, std::nano>(finish - start).count()
		/ double(PACKET_BYTES * PACKETS_PER_ROUND);
}

template <std::size_t Chunk>
double run_rounds(std::array<uint8_t, PACKET_BYTES> const &packet, volatile std::size_t &sink)
{
	std::vector<double> values;
	values.reserve(ROUNDS);
	for (unsigned round = 0; round < ROUNDS; ++round)
		values.push_back(run_once<Chunk>(packet, sink));
	return median(values);
}
}

int main()
{
	auto const packet = make_packet();
	volatile std::size_t sink = 0;

	if (!verify_byte_word_equivalence(packet))
	{
		std::cerr << "byte_word_equivalence=FAIL\n";
		return EXIT_FAILURE;
	}

	// Warm-up keeps allocation/page-fault noise out of the medians.
	(void)run_once<1>(packet, sink);
	(void)run_once<2>(packet, sink);
	(void)run_once<PACKET_BYTES>(packet, sink);

	double const byte_ns = run_rounds<1>(packet, sink);
	double const word_ns = run_rounds<2>(packet, sink);
	double const packet_ns = run_rounds<PACKET_BYTES>(packet, sink);

	std::cout << "CD-i PL_MPEG ingress benchmark\n"
		<< "byte_word_equivalence=pass\n"
		<< "equivalence_cycles=" << EQUIVALENCE_CYCLES << '\n'
		<< "payload_bytes_per_packet=" << PACKET_BYTES << '\n'
		<< "packets_per_round=" << PACKETS_PER_ROUND << '\n'
		<< "rounds=" << ROUNDS << '\n'
		<< "byte_ns_per_payload_byte=" << byte_ns << '\n'
		<< "word_ns_per_payload_byte=" << word_ns << '\n'
		<< "packet_ns_per_payload_byte=" << packet_ns << '\n'
		<< "word_speedup=" << (byte_ns / word_ns) << "x\n"
		<< "packet_speedup=" << (byte_ns / packet_ns) << "x\n"
		<< "sink=" << sink << '\n';
	return EXIT_SUCCESS;
}
