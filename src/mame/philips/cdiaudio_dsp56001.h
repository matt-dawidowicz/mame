// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIAUDIO_DSP56001_H
#define MAME_PHILIPS_CDIAUDIO_DSP56001_H

#pragma once

#include <cstdint>

namespace cdi_audio
{

// Motorola/NXP DSP56000 Family Manual, sections 3.2.2-3.3, documents the
// arithmetic geometry of the DSP56001 used by the VMPEG FMA control path:
// 24-bit data words, a 24 x 24 fractional multiplier producing a 48-bit
// product, and 56-bit A/B accumulators with eight extension bits.
//
// These are architectural facts about the DSP56001 core.  They do not identify
// Philips' attenuation coefficient representation, prove which DSP instruction
// sequence the VMPEG firmware uses, or attribute these widths to CDIC silicon.
constexpr uint8_t DSP56001_WORD_BITS = 24;
constexpr uint8_t DSP56001_PRODUCT_BITS = 48;
constexpr uint8_t DSP56001_ACCUMULATOR_BITS = 56;
constexpr uint8_t DSP56001_ACCUMULATOR_EXTENSION_BITS = 8;
constexpr int32_t DSP56001_WORD_MIN = -(int32_t(1) << 23);
constexpr int32_t DSP56001_WORD_MAX = (int32_t(1) << 23) - 1;

// Generic exact integer implementation of round-to-nearest-even.  The DSP56001
// manual calls this convergent rounding and exposes it through RND/MPYR/MACR.
// Keeping the shift explicit is important: the DSP scaling-mode bits can move
// the architectural rounding position, and the CD-i FMA firmware instruction
// sequence has not yet been recovered well enough to select one position here.
constexpr int64_t round_shift_nearest_even(int64_t value, uint8_t shift)
{
	if (!shift)
		return value;
	if (shift >= 64)
		return 0;

	uint64_t const magnitude = value < 0
		? uint64_t(-(value + 1)) + 1
		: uint64_t(value);
	uint64_t quotient = magnitude >> shift;
	uint64_t const remainder_mask = (uint64_t(1) << shift) - 1;
	uint64_t const remainder = magnitude & remainder_mask;
	uint64_t const half = uint64_t(1) << (shift - 1);
	if (remainder > half || (remainder == half && (quotient & 1)))
		++quotient;

	int64_t const rounded = int64_t(quotient);
	return value < 0 ? -rounded : rounded;
}

// Unscaled DSP56001 accumulator layout places the 24-bit LSP below the 24-bit
// MSP.  This helper therefore models the documented convergent reduction of a
// signed accumulator value to MSP precision.  It is a discrimination primitive,
// not a claim that the VMPEG attenuation routine executes RND at this point.
constexpr int64_t dsp56001_round_unscaled_accumulator_to_word(int64_t accumulator)
{
	return round_shift_nearest_even(accumulator, DSP56001_WORD_BITS);
}

// The DSP56001 data shifter/limiter substitutes the signed 24-bit extrema when
// a full-accumulator move cannot be represented in a 24-bit destination.  The
// accumulator itself is not modified.  Whether a particular VMPEG firmware move
// selects the full accumulator (limited) or an individual register (unlimited)
// remains instruction-stream evidence that this helper intentionally does not
// guess.
constexpr int32_t dsp56001_limit_word(int64_t value)
{
	return int32_t(value < DSP56001_WORD_MIN
		? DSP56001_WORD_MIN
		: value > DSP56001_WORD_MAX ? DSP56001_WORD_MAX : value);
}

} // namespace cdi_audio

#endif // MAME_PHILIPS_CDIAUDIO_DSP56001_H
