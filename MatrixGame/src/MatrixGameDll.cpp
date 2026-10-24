// MatrixGame - SR2 Planetary battles engine
// Copyright (C) 2012, Elemental Games, Katauri Interactive, CHK-Games
// Licensed under GPLv2 or any later version
// Refer to the LICENSE file included

#include "MatrixGameDll.hpp"
#include "MatrixFormGame.hpp"
#include "MatrixGame.h"
#include "3g.hpp"

#include <time.h>
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

SMGDRobotInterface g_RobotInterface;
SMGDRangersInterface *g_RangersInterface = nullptr;
int g_ExitState = 0;

void MatrixGameDllTrace(const char* text)
{
    wchar_t path[MAX_PATH];
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

void MatrixGameDllTracef(const char* fmt, ...)
{
    char buf[900];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    MatrixGameDllTrace(buf);
}

long __stdcall ExceptionHandler(PEXCEPTION_POINTERS info)
{
    DWORD code = 0;
    void* address = nullptr;
    if (info != nullptr && info->ExceptionRecord != nullptr)
    {
        code = info->ExceptionRecord->ExceptionCode;
        address = info->ExceptionRecord->ExceptionAddress;
    }
    MatrixGameDllTracef("Unhandled exception code=0x%08lX address=%p", code, address);
    if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2)
    {
        MatrixGameDllTracef(
            "Access violation %s %p",
            info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
            reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

BOOL APIENTRY DllMain(
    [[maybe_unused]] HANDLE hModule,
    DWORD ul_reason_for_call,
    [[maybe_unused]] LPVOID lpReserved)
{
    SetUnhandledExceptionFilter(ExceptionHandler);
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
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
    MatrixGameDllTracef("Run: enter hwnd=%p", hwnd);

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
