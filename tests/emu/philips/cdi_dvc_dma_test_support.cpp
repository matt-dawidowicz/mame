// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// Build the live fixture and its synthetic game driver in this translation
// unit so the driver-list shim can reference the anonymous-namespace driver
// without exporting test-only symbols from the fixture.
#include "cdi_dvc_dma_integration.cpp"

#include "drivenum.h"

std::size_t const driver_list::s_driver_count = 1;
game_driver const * const driver_list::s_drivers_sorted[] =
{
	&GAME_NAME(cdidmaint)
};

const char *emulator_info::get_appname() { return "MAME"; }
const char *emulator_info::get_appname_lower() { return "mame"; }
const char *emulator_info::get_configname() { return "mame"; }
const char *emulator_info::get_copyright() { return "MAME test fixture"; }
const char *emulator_info::get_copyright_info() { return "MAME test fixture"; }
const char *emulator_info::get_bare_build_version() { return "test"; }
const char *emulator_info::get_build_version() { return "test"; }
void emulator_info::display_ui_chooser(running_machine &machine) { }
int emulator_info::start_frontend(emu_options &options, osd_interface &osd, std::vector<std::string> &args) { return EMU_ERR_NONE; }
int emulator_info::start_frontend(emu_options &options, osd_interface &osd, int argc, char *argv[]) { return EMU_ERR_NONE; }
bool emulator_info::draw_user_interface(running_machine &machine) { return false; }
void emulator_info::periodic_check() { }
bool emulator_info::frame_hook() { return false; }
void emulator_info::sound_hook(const std::map<std::string, std::vector<std::pair<const float *, int>>> &sound) { }
void emulator_info::layout_script_cb(layout_file &file, const char *script) { }
bool emulator_info::standalone() { return true; }
