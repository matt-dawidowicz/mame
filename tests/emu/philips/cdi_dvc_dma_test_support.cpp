// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#if defined(_WIN32)
#include <windows.h>

#include <cstdint>
#include <cstdio>
#endif

// Build the live fixture and its synthetic game driver in this translation
// unit so the driver-list shim can reference the anonymous-namespace driver
// without exporting test-only symbols from the fixture.
#include "cdi_dvc_dma_integration.cpp"

#include "drivenum.h"

#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
namespace
{

LONG CALLBACK cdi_dma_exception_trace(EXCEPTION_POINTERS *exception)
{
	if (!exception || !exception->ExceptionRecord || !exception->ContextRecord
			|| exception->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
		return EXCEPTION_CONTINUE_SEARCH;

	HMODULE const main_module = GetModuleHandleW(nullptr);
	std::uintptr_t const main_base = reinterpret_cast<std::uintptr_t>(main_module);

	std::fprintf(stderr,
			"\nCDI_DMA_EXCEPTION code=0x%08lx address=%p main_base=%p\n",
			static_cast<unsigned long>(exception->ExceptionRecord->ExceptionCode),
			exception->ExceptionRecord->ExceptionAddress,
			main_module);

	HMODULE const ntdll = GetModuleHandleW(L"ntdll.dll");
	using lookup_function_type = PRUNTIME_FUNCTION (WINAPI *)(DWORD64, PDWORD64, PUNWIND_HISTORY_TABLE);
	using virtual_unwind_type = PEXCEPTION_ROUTINE (WINAPI *)(
			DWORD, DWORD64, DWORD64, PRUNTIME_FUNCTION, PCONTEXT,
			PVOID *, PDWORD64, PKNONVOLATILE_CONTEXT_POINTERS);

	auto const lookup_function = ntdll
			? reinterpret_cast<lookup_function_type>(GetProcAddress(ntdll, "RtlLookupFunctionEntry"))
			: nullptr;
	auto const virtual_unwind = ntdll
			? reinterpret_cast<virtual_unwind_type>(GetProcAddress(ntdll, "RtlVirtualUnwind"))
			: nullptr;

	if (!lookup_function || !virtual_unwind)
	{
		std::fprintf(stderr, "CDI_DMA_EXCEPTION unwind APIs unavailable\n");
		std::fflush(stderr);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	CONTEXT context = *exception->ContextRecord;
	HANDLE const process = GetCurrentProcess();

	for (unsigned frame = 0; frame < 32 && context.Rip; ++frame)
	{
		std::uintptr_t const pc = static_cast<std::uintptr_t>(context.Rip);
		if (main_base && pc >= main_base)
		{
			std::fprintf(stderr,
					"CDI_DMA_FRAME %02u pc=%p rva=0x%llx\n",
					frame,
					reinterpret_cast<void *>(pc),
					static_cast<unsigned long long>(pc - main_base));
		}
		else
		{
			std::fprintf(stderr,
					"CDI_DMA_FRAME %02u pc=%p external\n",
					frame,
					reinterpret_cast<void *>(pc));
		}

		DWORD64 image_base = 0;
		PRUNTIME_FUNCTION const runtime_function =
				lookup_function(context.Rip, &image_base, nullptr);
		if (runtime_function)
		{
			PVOID handler_data = nullptr;
			DWORD64 establisher_frame = 0;
			virtual_unwind(
					UNW_FLAG_NHANDLER,
					image_base,
					context.Rip,
					runtime_function,
					&context,
					&handler_data,
					&establisher_frame,
					nullptr);
		}
		else
		{
			DWORD64 return_pc = 0;
			SIZE_T bytes_read = 0;
			if (!context.Rsp
					|| !ReadProcessMemory(
							process,
							reinterpret_cast<LPCVOID>(context.Rsp),
							&return_pc,
							sizeof(return_pc),
							&bytes_read)
					|| bytes_read != sizeof(return_pc))
				break;

			context.Rip = return_pc;
			context.Rsp += sizeof(return_pc);
		}
	}

	std::fflush(stderr);
	return EXCEPTION_CONTINUE_SEARCH;
}

class cdi_dma_exception_trace_registration
{
public:
	cdi_dma_exception_trace_registration()
		: m_handle(AddVectoredExceptionHandler(1, cdi_dma_exception_trace))
	{
	}

	~cdi_dma_exception_trace_registration()
	{
		if (m_handle)
			RemoveVectoredExceptionHandler(m_handle);
	}

private:
	PVOID m_handle;
};

cdi_dma_exception_trace_registration s_cdi_dma_exception_trace_registration;

} // anonymous namespace
#endif

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
