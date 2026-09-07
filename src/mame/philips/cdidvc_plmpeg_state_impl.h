// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz
#pragma once

// Include only in the translation unit owning PL_MPEG_IMPLEMENTATION.
#include "cdidvc_plmpeg_state.h"
#include <cmath>
#include <cstring>
#include <type_traits>

namespace cdi_dvc
{
namespace
{
class decoder_state_io
{
  public:
	decoder_state_io(uint8_t *out, uint8_t const *in, std::size_t size)
		: output(out), input(in), capacity(size), reading(in != nullptr)
	{
	}
	template <typename T> void value(T &v)
	{
		static_assert(std::is_arithmetic_v<T> && sizeof(T) <= 8);
		using bits_type = std::conditional_t<
			sizeof(T) == 8, uint64_t,
			std::conditional_t<sizeof(T) == 4, uint32_t, std::conditional_t<sizeof(T) == 2, uint16_t, uint8_t>>>;
		static_assert(sizeof(bits_type) == sizeof(T));
		if (!fits(sizeof(T)))
			return;
		bits_type bits = 0;
		if (!reading)
			std::memcpy(&bits, &v, sizeof(v));
		for (unsigned i = 0; i < sizeof(T); ++i)
		{
			if (reading)
				bits |= bits_type(input[position + i]) << (i * 8);
			else
				output[position + i] = uint8_t(bits >> (i * 8));
		}
		if (reading)
			std::memcpy(&v, &bits, sizeof(v));
		if constexpr (std::is_floating_point_v<T>)
			ok &= std::isfinite(v);
		position += sizeof(T);
	}
	template <typename T, std::size_t N> void value(T (&v)[N])
	{
		for (auto &item : v)
			value(item);
	}
	void raw(uint8_t *bytes, std::size_t length)
	{
		if (!fits(length))
			return;
		if (length)
		{
			if (reading)
				std::memcpy(bytes, input + position, length);
			else
				std::memcpy(output + position, bytes, length);
		}
		position += length;
	}
	bool fits(std::size_t length)
	{
		ok &= position <= capacity && length <= capacity - position;
		return ok;
	}
	bool finished() const
	{
		return ok && (!reading || position == capacity);
	}
	uint8_t *output;
	uint8_t const *input;
	std::size_t capacity, position = 0;
	bool reading, ok = true;
};

void decoder_buffer_state(decoder_state_io &io, plm_buffer_t *b)
{
	if (!b || b->mode != PLM_BUFFER_MODE_RING || !b->free_when_done || b->load_callback || b->seek_callback ||
		b->tell_callback || b->load_callback_user_data)
	{
		io.ok = false;
		return;
	}
	if (io.reading && (b->length || b->bit_index))
	{
		io.ok = false;
		return;
	}
	uint64_t bit_index = b->bit_index, total_size = b->total_size;
	uint64_t length = b->length;
	int discard = b->discard_read_bytes, ended = b->has_ended;
	io.value(bit_index);
	io.value(total_size);
	io.value(length);
	io.value(discard);
	io.value(ended);
	if (!io.ok || length > io.capacity || !io.fits(length) || bit_index > length * 8 || total_size > length ||
		(discard != 0 && discard != 1) || (ended != 0 && ended != 1))
	{
		io.ok = false;
		return;
	}
	if (io.reading)
	{
		if (length > b->capacity)
		{
			auto *bytes = static_cast<uint8_t *>(PLM_MALLOC(std::size_t(length)));
			if (!bytes)
			{
				io.ok = false;
				return;
			}
			PLM_FREE(b->bytes);
			b->bytes = bytes;
			b->capacity = std::size_t(length);
		}
		if (length)
			std::memcpy(b->bytes, io.input + io.position, std::size_t(length));
		b->length = std::size_t(length);
		b->bit_index = bit_index;
		b->total_size = total_size;
		b->discard_read_bytes = discard;
		b->has_ended = ended;
		io.position += length;
	}
	else
		io.raw(b->bytes, length);
}

void audio_state(decoder_state_io &io, plm_audio_t &d)
{
	uint32_t magic = 0x31415644; // DVA1
	io.value(magic);
	if (magic != 0x31415644)
	{
		io.ok = false;
		return;
	}
#define DVC_AUDIO_STATE_FIELDS(X)                                                                                      \
	X(time)                                                                                                            \
	X(samples_decoded)                                                                                                 \
	X(samplerate_index) X(bitrate_index) X(version) X(layer) X(mode) X(emphasis) X(bound) X(v_pos)                     \
		X(next_frame_data_size) X(has_header)
#define DVC_FIELD(name) io.value(d.name);
	DVC_AUDIO_STATE_FIELDS(DVC_FIELD)
#undef DVC_FIELD
#undef DVC_AUDIO_STATE_FIELDS
	constexpr unsigned quantizers = sizeof(PLM_AUDIO_QUANT_TAB) / sizeof(PLM_AUDIO_QUANT_TAB[0]);
	for (auto &channel : d.allocation)
		for (auto &allocation : channel)
		{
			uint8_t index = 0;
			if (!io.reading && allocation)
			{
				for (unsigned i = 0; i < quantizers; ++i)
					if (allocation == &PLM_AUDIO_QUANT_TAB[i])
						index = i + 1;
				if (!index)
					io.ok = false;
			}
			io.value(index);
			if (index > quantizers)
			{
				io.ok = false;
				return;
			}
			if (io.reading)
				allocation = index ? &PLM_AUDIO_QUANT_TAB[index - 1] : nullptr;
		}
	io.value(d.scale_factor_info);
	io.value(d.scale_factor);
	io.value(d.sample);
	io.value(d.samples.time);
	io.value(d.samples.count);
#ifdef PLM_AUDIO_SEPARATE_CHANNELS
	io.value(d.samples.left);
	io.value(d.samples.right);
#else
	io.value(d.samples.interleaved);
#endif
	io.value(d.D);
	io.value(d.V);
	io.value(d.U);
	if (d.samplerate_index < 0 || d.samplerate_index > 3 || d.bitrate_index < 0 || d.bitrate_index > 13 ||
		d.version < 0 || d.version > 3 || d.layer < 0 || d.layer > 3 || d.mode < 0 || d.mode > 3 || d.bound < 0 ||
		d.bound > 32 || d.v_pos < 0 || d.v_pos >= 1024 || d.next_frame_data_size < 0 || d.next_frame_data_size > 4096 ||
		(d.has_header && d.samplerate_index == 3) || (d.has_header != 0 && d.has_header != 1) ||
		d.samples.count != PLM_AUDIO_SAMPLES_PER_FRAME)
	{
		io.ok = false;
		return;
	}
	decoder_buffer_state(io, d.buffer);
}

void video_state(decoder_state_io &io, plm_video_t &d)
{
	uint32_t magic = 0x31565644; // DVV1
	io.value(magic);
	if (magic != 0x31565644)
	{
		io.ok = false;
		return;
	}
#define DVC_VIDEO_STATE_FIELDS(X)                                                                                      \
	X(framerate)                                                                                                       \
	X(pixel_aspect_ratio)                                                                                              \
	X(time) X(frames_decoded) X(width) X(height) X(mb_width) X(mb_height) X(mb_size) X(luma_width) X(luma_height)      \
		X(chroma_width) X(chroma_height) X(start_code) X(picture_type) X(motion_forward.full_px)                       \
			X(motion_forward.is_set) X(motion_forward.r_size) X(motion_forward.h) X(motion_forward.v)                  \
				X(motion_backward.full_px) X(motion_backward.is_set) X(motion_backward.r_size) X(motion_backward.h)    \
					X(motion_backward.v) X(has_sequence_header) X(quantizer_scale) X(slice_begin)                      \
						X(macroblock_address) X(mb_row) X(mb_col) X(macroblock_type) X(macroblock_intra)               \
							X(dc_predictor) X(block_data) X(intra_quant_matrix) X(non_intra_quant_matrix)              \
								X(has_reference_frame) X(assume_no_b_frames)
#define DVC_FIELD(name) io.value(d.name);
	DVC_VIDEO_STATE_FIELDS(DVC_FIELD)
#undef DVC_FIELD
#undef DVC_VIDEO_STATE_FIELDS
	if (!io.ok || (d.has_sequence_header != 0 && d.has_sequence_header != 1) ||
		(d.has_reference_frame != 0 && d.has_reference_frame != 1) ||
		(d.assume_no_b_frames != 0 && d.assume_no_b_frames != 1))
	{
		io.ok = false;
		return;
	}
	if (d.has_sequence_header)
	{
		if (d.width <= 0 || d.width > 384 || d.height <= 0 || d.height > 288 || d.mb_width != (d.width + 15) / 16 ||
			d.mb_height != (d.height + 15) / 16 || d.mb_size != d.mb_width * d.mb_height ||
			d.luma_width != d.mb_width * 16 || d.luma_height != d.mb_height * 16 || d.chroma_width != d.mb_width * 8 ||
			d.chroma_height != d.mb_height * 8 || d.framerate <= 0)
		{
			io.ok = false;
			return;
		}
		std::size_t const luma = std::size_t(d.luma_width) * d.luma_height;
		std::size_t const chroma = std::size_t(d.chroma_width) * d.chroma_height;
		std::size_t const frame_size = luma + 2 * chroma;
		plm_frame_t *frames[] = {&d.frame_current, &d.frame_forward, &d.frame_backward};
		uint8_t slots[3]{};
		unsigned seen = 0;
		for (unsigned i = 0; i < 3; ++i)
		{
			if (!io.reading)
			{
				unsigned found = 3;
				for (unsigned slot = 0; slot < 3; ++slot)
					if (frames[i]->y.data == d.frames_data + slot * frame_size)
						found = slot;
				if (found == 3 || frames[i]->cr.data != frames[i]->y.data + luma ||
					frames[i]->cb.data != frames[i]->y.data + luma + chroma)
				{
					io.ok = false;
					return;
				}
				slots[i] = found;
			}
			io.value(slots[i]);
			io.value(frames[i]->time);
			if (slots[i] > 2 || (seen & (1U << slots[i])))
			{
				io.ok = false;
				return;
			}
			seen |= 1U << slots[i];
		}
		if (!io.fits(frame_size * 3))
			return;
		if (io.reading)
		{
			d.frames_data = static_cast<uint8_t *>(PLM_MALLOC(frame_size * 3));
			if (!d.frames_data)
			{
				io.ok = false;
				return;
			}
			for (unsigned i = 0; i < 3; ++i)
				plm_video_init_frame(&d, frames[i], d.frames_data + slots[i] * frame_size);
		}
		io.raw(d.frames_data, frame_size * 3);
	}
	decoder_buffer_state(io, d.buffer);
}
} // anonymous namespace

std::size_t plmpeg_audio_snapshot_write(plm_audio_t const *decoder, uint8_t *out, std::size_t capacity)
{
	if (!decoder || !out)
		return 0;
	decoder_state_io io(out, nullptr, capacity);
	plm_audio_t state = *decoder;
	audio_state(io, state);
	return io.finished() ? io.position : 0;
}

std::size_t plmpeg_video_snapshot_write(plm_video_t const *decoder, uint8_t *out, std::size_t capacity)
{
	if (!decoder || !out)
		return 0;
	decoder_state_io io(out, nullptr, capacity);
	plm_video_t state = *decoder;
	video_state(io, state);
	return io.finished() ? io.position : 0;
}

bool plmpeg_audio_snapshot_read(plm_audio_t *decoder, uint8_t const *data, std::size_t length)
{
	if (!decoder || !data)
		return false;
	decoder_state_io io(nullptr, data, length);
	audio_state(io, *decoder);
	return io.finished();
}

bool plmpeg_video_snapshot_read(plm_video_t *decoder, uint8_t const *data, std::size_t length)
{
	if (!decoder || !data || decoder->frames_data)
		return false;
	decoder_state_io io(nullptr, data, length);
	video_state(io, *decoder);
	return io.finished();
}
} // namespace cdi_dvc
