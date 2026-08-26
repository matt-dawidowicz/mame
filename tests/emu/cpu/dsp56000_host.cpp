// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz

#include "catch.hpp"
#include "dsp56000host.h"

TEST_CASE("DSP56000 CVR keeps six vector bits", "[emu][cpu][dsp56000][host]")
{
	dsp56000_host_interface h;
	h.reset();
	h.write(dsp56000_host_interface::CVR, 0xa0);
	REQUIRE(h.read(dsp56000_host_interface::CVR) == 0xa0);
	REQUIRE(h.host_command_vector() == 0x20);
	h.write(dsp56000_host_interface::CVR, 0xff);
	REQUIRE(h.read(dsp56000_host_interface::CVR) == 0xbf);
	REQUIRE(h.host_command_vector() == 0x3f);
}