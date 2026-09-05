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
		ext_lib("utf8proc"),
		"ocore_" .. _OPTIONS["osd"],
	}

	includedirs {
		MAME_DIR .. "3rdparty/catch/single_include",
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/devices/machine",
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
		MAME_DIR .. "tests/emu/machine/scc68070.cpp",
		MAME_DIR .. "tests/emu/philips/cdicdic.cpp",
		MAME_DIR .. "tests/emu/philips/cdicdic_memory.cpp",
		MAME_DIR .. "tests/emu/philips/mcd212_video.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_timing.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_invariants.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_audio_format.cpp",
		MAME_DIR .. "tests/emu/philips/cdi_audio_arithmetic.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_audio_reference.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_audio_replay.cpp",
		MAME_DIR .. "tests/emu/philips/cdislavehle_pointer.cpp",
		MAME_DIR .. "tests/emu/philips/cdislavehle_commands.cpp",
		MAME_DIR .. "tests/emu/philips/cdislavehle_transport.cpp",
		MAME_DIR .. "tests/emu/philips/cdislavehle_response_ready.cpp",
		MAME_DIR .. "tests/emu/philips/cdi_hardening.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_timestamp_format.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_pes_format.cpp",
		MAME_DIR .. "tests/emu/philips/cdidvc_dclk_wrap.cpp",
		MAME_DIR .. "tests/emu/philips/cdimono2.cpp",
		MAME_DIR .. "tests/lib/util/corestr.cpp",
		MAME_DIR .. "tests/lib/util/options.cpp",
		MAME_DIR .. "tests/emu/attotime.cpp",
		MAME_DIR .. "tests/emu/video/rgbutil.cpp",
	}

-- Full-machine integration fixtures use a separate executable.  Keeping them
-- out of mametests preserves the lightweight helper-only suite and prevents
-- helper translation units that embed third-party implementations (notably
-- PL_MPEG) from colliding with the production device objects.
if _OPTIONS["with-emulator"] then
	project("cdiintegrationtests")
		uuid ("a709a76c-88da-4f83-a9e9-f391d68cf850")
		kind "ConsoleApp"

		flags {
			"Symbols",
		}

		if _OPTIONS["SEPARATE_BIN"]~="1" then
			targetdir(MAME_DIR)
		end

		configuration { "Release" }
			targetsuffix ""
		configuration { "Debug" }
			targetsuffix "d"
		configuration { "mingw*" or "vs*" }
			targetextension ".exe"
		configuration { }

		includedirs {
			MAME_DIR .. "3rdparty/catch/single_include",
			MAME_DIR .. "src/osd",
			MAME_DIR .. "src/emu",
			MAME_DIR .. "src/devices",
			MAME_DIR .. "src/frontend/mame",
			MAME_DIR .. "src/mame",
			MAME_DIR .. "src/mame/philips",
			MAME_DIR .. "src/lib",
			MAME_DIR .. "src/lib/util",
			MAME_DIR .. "3rdparty",
			GEN_DIR .. "emu",
			GEN_DIR .. "mame/layout",
			ext_includedir("expat"),
			ext_includedir("zlib"),
			ext_includedir("flac"),
		}

		files {
			MAME_DIR .. "tests/main.cpp",
			MAME_DIR .. "tests/emu/philips/cdi_dvc_dma_test_support.cpp",
			MAME_DIR .. "src/osd/interface/inputseq.cpp",
			MAME_DIR .. "src/osd/interface/nethandler.cpp",
		}

		links {
			"emu",
			"optional",
			"formats",
			"dasm",
			"ocore_" .. _OPTIONS["osd"],
			"utils",
			ext_lib("expat"),
			ext_lib("zlib"),
			ext_lib("zstd"),
			ext_lib("flac"),
			ext_lib("utf8proc"),
			ext_lib("jpeg"),
			"softfloat3",
			"wdlfft",
			"ymfm",
			"7z",
		}

		if (_OPTIONS["SOURCES"] ~= nil) or (_OPTIONS["SOURCEFILTER"] ~= nil) then
			links {
				"mame_" .. _OPTIONS["subtarget"],
			}
		else
			links {
				"philips",
			}
		end
end