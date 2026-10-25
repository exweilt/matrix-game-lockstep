// MatrixGame - SR2 Planetary battles engine
// Copyright (C) 2012, Elemental Games, Katauri Interactive, CHK-Games
// Licensed under GPLv2 or any later version
// Refer to the LICENSE file included

#include "MatrixGameDll.hpp"
#include "MatrixFormGame.hpp"
#include "MatrixGame.h"
#include "3g.hpp"
#include "Tracer.hpp"

#include <time.h>
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>

#pragma comment(lib, "dbghelp.lib")

SMGDRobotInterface g_RobotInterface;
SMGDRangersInterface *g_RangersInterface = nullptr;
int g_ExitState = 0;

static void MatrixGameDllLogPath(wchar_t path[MAX_PATH])
{
    HMODULE module = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&MatrixGameDllTrace),
        &module);

    DWORD n = GetModuleFileNameW(module, path, MAX_PATH);
    wchar_t* slash = (n > 0 && n < MAX_PATH) ? wcsrchr(path, L'\\') : nullptr;
    if (slash != nullptr)
        lstrcpynW(slash + 1, L"MatrixGame_dll.log", static_cast<int>(MAX_PATH - (slash + 1 - path)));
    else
        lstrcpynW(path, L"MatrixGame_dll.log", MAX_PATH);
}

void MatrixGameDllTrace(const char* text)
{
    wchar_t path[MAX_PATH];
    MatrixGameDllLogPath(path);

    HANDLE file = CreateFileW(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[1200];
    int prefix = snprintf(
        line,
        sizeof(line),
        "%02u:%02u:%02u.%03u ",
        static_cast<unsigned>(st.wHour),
        static_cast<unsigned>(st.wMinute),
        static_cast<unsigned>(st.wSecond),
        static_cast<unsigned>(st.wMilliseconds));
    if (prefix < 0 || prefix >= static_cast<int>(sizeof(line)) - 3)
        prefix = 0;

    const char* message = text != nullptr ? text : "";
    size_t room = sizeof(line) - static_cast<size_t>(prefix) - 3;
    size_t len = strlen(message);
    if (len > room)
        len = room;
    memcpy(line + prefix, message, len);
    line[prefix + len] = '\r';
    line[prefix + len + 1] = '\n';

    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(prefix + len + 2), &written, nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
}

void MatrixGameDllTraceFrameDelta(int delta_ms)
{
    static HANDLE file = INVALID_HANDLE_VALUE;
    if (file == INVALID_HANDLE_VALUE)
    {
        wchar_t path[MAX_PATH];
        MatrixGameDllLogPath(path);
        file = CreateFileW(
            path,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return;

        const char header[] = "client logic dt\r\n";
        DWORD written = 0;
        WriteFile(file, header, sizeof(header) - 1, &written, nullptr);
    }

    char line[16];
    int n = snprintf(line, sizeof(line), "%d\r\n", delta_ms);
    if (n <= 0)
        return;

    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(n), &written, nullptr);
}

void MatrixGameDllTracef(const char* fmt, ...)
{
    char buf[900];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    MatrixGameDllTrace(buf);
}

static constexpr int kBreadcrumbCount = 48;
static constexpr int kBreadcrumbLen = 180;
static char g_breadcrumbs[kBreadcrumbCount][kBreadcrumbLen];
static volatile long g_breadcrumb_next = 0;
static volatile long g_crash_logged = 0;
static PVOID g_crash_veh = nullptr;

void MatrixGameDllBreadcrumb(const char* text)
{
    const long slot_index = InterlockedIncrement(&g_breadcrumb_next) - 1;
    int slot = static_cast<int>(slot_index % kBreadcrumbCount);
    if (slot < 0)
        slot += kBreadcrumbCount;

    const char* message = text != nullptr ? text : "";
    size_t length = strlen(message);
    if (length > kBreadcrumbLen - 1)
        length = kBreadcrumbLen - 1;
    memcpy(g_breadcrumbs[slot], message, length);
    g_breadcrumbs[slot][length] = '\0';
}

void MatrixGameDllBreadcrumbf(const char* fmt, ...)
{
    char buf[kBreadcrumbLen];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    MatrixGameDllBreadcrumb(buf);
}

static void MatrixGameDllDumpBreadcrumbs(void)
{
    const long total = g_breadcrumb_next;
    if (total <= 0) {
        MatrixGameDllTrace("breadcrumbs: (none)");
        return;
    }

    const int count = total < kBreadcrumbCount ? static_cast<int>(total) : kBreadcrumbCount;
    const int first = total < kBreadcrumbCount ? 0 : static_cast<int>(total % kBreadcrumbCount);
    MatrixGameDllTrace("breadcrumbs (oldest first):");
    for (int i = 0; i < count; ++i) {
        const int slot = (first + i) % kBreadcrumbCount;
        MatrixGameDllTracef("  %s", g_breadcrumbs[slot]);
    }
}

static void MatrixGameDllDumpDtrace(void)
{
    try {
        const std::string trace = generate_trace_text();
        const char* cursor = trace.c_str();
        while (cursor != nullptr && *cursor != '\0') {
            const char* end = strchr(cursor, '\n');
            char line[900];
            const size_t len = end != nullptr ? static_cast<size_t>(end - cursor) : strlen(cursor);
            const size_t copy = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
            memcpy(line, cursor, copy);
            line[copy] = '\0';
            if (line[0] != '\0')
                MatrixGameDllTrace(line);
            if (end == nullptr)
                break;
            cursor = end + 1;
        }
    }
    catch (...) {
        MatrixGameDllTrace("dtrace stack: unavailable");
    }
}

void MatrixGameDllDumpContext(const char* reason)
{
    MatrixGameDllTrace("---- client context ----");
    MatrixGameDllTrace(reason != nullptr ? reason : "");
    MatrixGameDllDumpBreadcrumbs();
    MatrixGameDllDumpDtrace();
}

static const char* MatrixGameDllExceptionName(DWORD code)
{
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT: return "FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW: return "FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK: return "FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW: return "FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW: return "INT_OVERFLOW";
        case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
        case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION: return "INVALID_DISPOSITION";
        case EXCEPTION_BREAKPOINT: return "BREAKPOINT";
        case 0xC0000374: return "HEAP_CORRUPTION";
        case 0xC0000409: return "STACK_BUFFER_OVERRUN";
        default: return "UNKNOWN";
    }
}

static bool MatrixGameDllIsFatalSeh(DWORD code)
{
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_DATATYPE_MISALIGNMENT:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_INVALID_OPERATION:
        case EXCEPTION_FLT_OVERFLOW:
        case EXCEPTION_FLT_STACK_CHECK:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_OVERFLOW:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_INVALID_DISPOSITION:
        case EXCEPTION_BREAKPOINT:
        case 0xC0000374:
        case 0xC0000409:
            return true;
        default:
            return false;
    }
}

static void MatrixGameDllLogNativeStack(EXCEPTION_POINTERS* info)
{
    if (info == nullptr || info->ContextRecord == nullptr)
        return;
    if (info->ExceptionRecord != nullptr && info->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
        MatrixGameDllTrace("native stack: skipped (stack overflow, no room to walk)");
        return;
    }

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    const BOOL symbols = SymInitialize(process, nullptr, TRUE);

    CONTEXT context = *info->ContextRecord;
    STACKFRAME64 frame{};
#if defined(_M_IX86)
    const DWORD machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Offset = context.Esp;
#elif defined(_M_X64)
    const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
#else
    MatrixGameDllTrace("native stack: unsupported architecture");
    return;
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    MatrixGameDllTrace("native stack (faulting instruction first):");
#if defined(_M_IX86)
    // A call through a null pointer lands with Eip == 0, and StackWalk64 then
    // reports no frames. The return address is the dword the call pushed.
    if (context.Eip == 0 && context.Esp != 0) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<void*>(context.Esp), &mbi, sizeof(mbi)) == sizeof(mbi) &&
            mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0) {
            const DWORD caller = *reinterpret_cast<DWORD*>(context.Esp);
            MatrixGameDllTracef("  null call, return address=%08lX", static_cast<unsigned long>(caller));
        }
    }
#endif
    for (int i = 0; i < 40; ++i) {
        if (!StackWalk64(
                machine, process, GetCurrentThread(), &frame, &context, nullptr, SymFunctionTableAccess64,
                SymGetModuleBase64, nullptr))
            break;

        const DWORD64 pc = frame.AddrPC.Offset;
        if (pc == 0)
            break;

        char module_path[MAX_PATH] = "?";
        const DWORD64 module_base = SymGetModuleBase64(process, pc);
        if (module_base != 0)
            GetModuleFileNameA(reinterpret_cast<HMODULE>(module_base), module_path, MAX_PATH);
        const char* module_name = strrchr(module_path, '\\');
        module_name = module_name != nullptr ? module_name + 1 : module_path;

        char symbol[300] = "";
        if (symbols) {
            alignas(SYMBOL_INFO) char symbol_storage[sizeof(SYMBOL_INFO) + 256] = {};
            SYMBOL_INFO* info_sym = reinterpret_cast<SYMBOL_INFO*>(symbol_storage);
            info_sym->SizeOfStruct = sizeof(SYMBOL_INFO);
            info_sym->MaxNameLen = 255;
            DWORD64 displacement = 0;
            if (SymFromAddr(process, pc, &displacement, info_sym)) {
                snprintf(
                    symbol, sizeof(symbol), " %s+0x%llX", info_sym->Name,
                    static_cast<unsigned long long>(displacement));
            }

            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD line_displacement = 0;
            if (SymGetLineFromAddr64(process, pc, &line_displacement, &line) && line.FileName != nullptr) {
                const size_t used = strlen(symbol);
                snprintf(
                    symbol + used, sizeof(symbol) - used, " (%s:%lu)", line.FileName,
                    static_cast<unsigned long>(line.LineNumber));
            }
        }

        const unsigned long long offset = module_base != 0 ? pc - module_base : pc;
        MatrixGameDllTracef(
            "  %s+0x%llX%s", module_name, offset, symbol);
    }
}

static void MatrixGameDllLogFatalSeh(EXCEPTION_POINTERS* info)
{
    if (InterlockedExchange(&g_crash_logged, 1) != 0)
        return;

    DWORD code = 0;
    void* address = nullptr;
    if (info != nullptr && info->ExceptionRecord != nullptr) {
        code = info->ExceptionRecord->ExceptionCode;
        address = info->ExceptionRecord->ExceptionAddress;
    }

    MatrixGameDllTrace("==== fatal exception ====");
    MatrixGameDllTracef(
        "%s code=0x%08lX address=%p", MatrixGameDllExceptionName(code), static_cast<unsigned long>(code), address);
    if (code == EXCEPTION_ACCESS_VIOLATION && info != nullptr && info->ExceptionRecord != nullptr &&
        info->ExceptionRecord->NumberParameters >= 2) {
        MatrixGameDllTracef(
            "access violation %s %p",
            info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
            reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
    }
    else if (code == EXCEPTION_IN_PAGE_ERROR && info != nullptr && info->ExceptionRecord != nullptr &&
             info->ExceptionRecord->NumberParameters >= 2) {
        MatrixGameDllTracef(
            "in-page error %s %p",
            info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
            reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
    }

    MatrixGameDllDumpBreadcrumbs();
    MatrixGameDllLogNativeStack(info);
    MatrixGameDllDumpDtrace();
    MatrixGameDllTrace("==== end fatal exception ====");
}

static LONG CALLBACK MatrixGameDllVectoredCrash(EXCEPTION_POINTERS* info)
{
    if (info == nullptr || info->ExceptionRecord == nullptr)
        return EXCEPTION_CONTINUE_SEARCH;

    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (!MatrixGameDllIsFatalSeh(code))
        return EXCEPTION_CONTINUE_SEARCH;
    // A debugger uses breakpoints. Log them only when nobody is attached (failed ASSERT).
    if (code == EXCEPTION_BREAKPOINT && IsDebuggerPresent())
        return EXCEPTION_CONTINUE_SEARCH;

    MatrixGameDllLogFatalSeh(info);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void MatrixGameDllInstallCrashHandler(void)
{
    if (g_crash_veh != nullptr)
        return;
    g_crash_veh = AddVectoredExceptionHandler(1, MatrixGameDllVectoredCrash);
}

BOOL APIENTRY DllMain(
    [[maybe_unused]] HANDLE hModule,
    DWORD ul_reason_for_call,
    [[maybe_unused]] LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
        MatrixGameDllInstallCrashHandler();
    return TRUE;
}

void __stdcall Init(SMGDRangersInterface *ri) {
    g_RangersInterface = ri;
}

void __stdcall Deinit() {
    g_RangersInterface = NULL;
}

int __stdcall Support() {
    // g_D3D = Direct3DCreate9(D3D_SDK_VERSION);
    g_D3D = Direct3DCreate9(31);

    if (g_D3D == NULL)
        return SUPE_DIRECTX;

    D3DCAPS9 caps;
    if (D3D_OK != g_D3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps)) {
        g_D3D->Release();
        g_D3D = NULL;
        return SUPE_DIRECTX;
    }

    if (caps.MaxSimultaneousTextures < 2)
        return SUPE_VIDEOHARDWARE;

    if (caps.MaxTextureWidth < 2048)
        return SUPE_VIDEOHARDWARE;
    if (caps.MaxTextureHeight < 2048)
        return SUPE_VIDEOHARDWARE;

    if (caps.MaxStreams == 0)
        return SUPE_VIDEODRIVER;
    g_D3D->Release();
    g_D3D = NULL;
    return SUPE_OK;
}

int __stdcall Run(HINSTANCE hinst, HWND hwnd, wchar *map, SRobotsSettings *settings, wchar *lang, wchar *txt_start,
                  wchar *txt_win, wchar *txt_loss, wchar *planet, SRobotGameState *rgs) {
    uint32_t seed = (unsigned)time(nullptr);
    MatrixGameDllInstallCrashHandler();
    MatrixGameDllTracef(
        "Run: enter hwnd=%p crash_handler=%s", hwnd, g_crash_veh != nullptr ? "on" : "OFF");

    CGame::Init(hinst, hwnd, map, seed, settings, lang, txt_start, txt_win, txt_loss, planet);
    MatrixGameDllTrace("Run: init done");
    
    CFormMatrixGame formgame;

    CGame::RunGameLoop(&formgame);

    CGame::SaveResult(rgs);
    CGame::SafeFree();

    ClipCursor(nullptr);

    // 0 tells the host to leave to the desktop. Closing the window returns to its menu.
    if (FLAG(g_Flags, GFLAG_APPCLOSE) && g_ExitState == 0)
        g_ExitState = 1;

    int result = 0;
    if (FLAG(g_Flags, GFLAG_EXITLOOP) || FLAG(g_Flags, GFLAG_APPCLOSE))
        result = g_ExitState;

    MatrixGameDllTracef(
        "Run: return %d flags=0x%lX exit=%d",
        result,
        static_cast<unsigned long>(g_Flags),
        g_ExitState);
    return result;
}

MATRIXGAMEDLL_API SMGDRobotInterface *__cdecl GetRobotInterface(void) {
    ZeroMemory(&g_RobotInterface, sizeof(SMGDRobotInterface));
    g_RobotInterface.m_Init = &Init;
    g_RobotInterface.m_Deinit = &Deinit;
    g_RobotInterface.m_Support = &Support;
    g_RobotInterface.m_Run = &Run;
    return &g_RobotInterface;
}
