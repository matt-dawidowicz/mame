-- license:BSD-3-Clause
-- copyright-holders:MAMEdev Team

---------------------------------------------------------------------------
--
--   tests.lua
--
--   Rules for building tests
--
---------------------------------------------------------------------------

project("mametests")
	uuid ("66d4c639-196b-4065-a411-7ee9266564f5")
	kind "ConsoleApp"

	flags {
		"Symbols", -- always include minimum symbols for executables
	}

	if _OPTIONS["SEPARATE_BIN"]~="1" then
		targetdir(MAME_DIR)
	end

	configuration { "Release" }
		targetsuffix ""
		if _OPTIONS["PROFILE"] then
			targetsuffix "p"
		end

	configuration { "Debug" }
		targetsuffix "d"
		if _OPTIONS["PROFILE"] then
			targetsuffix "dp"
		end

	configuration { "mingw*" or "vs*" }
		targetextension ".exe"

	configuration { }

	links {
		"utils",
		ext_lib("expat"),
		ext_lib("zlib"),
		"ocore_" .. _OPTIONS["osd"],
	}

	includedirs {
		MAME_DIR .. "3rdparty/catch/single_include",
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "src/mame/philips",
		ext_includedir("expat"),
		ext_includedir("zlib"),
	}

	files {
		MAME_DIR .. "src/emu/video/rgbutil.cpp",
		MAME_DIR .. "src/emu/video/rgbutil.h",
	}

	files {
		MAME_DIR .. "tests/main.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_timing.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_invariants.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_audio_format.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_audio_replay.cpp",
		MAME_DIR .. "tests/emu/philips/cdislavehle_pointer.cpp",
		MAME_DIR .. "tests/emu/philips/cdislavehle_transport.cpp",
		MAME_DIR .. "tests/emu/philips/cdislavehle_response_ready.cpp",
		MAME_DIR .. "tests/emu/philips/cdi_hardening.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_timestamp_format.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_pes_format.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_dclk_wrap.cpp",
		MAME_DIR .. "tests/lib/util/corestr.cpp",
		MAME_DIR .. "tests/lib/util/options.cpp",
		MAME_DIR .. "tests/emu/attotime.cpp",
		MAME_DIR .. "tests/emu/video/rgbutil.cpp",
	}
