/*
 * network.cpp
 *
 * Copyright (C) 2002 Matt Ownby
 *
 * This file is part of DAPHNE, a laserdisc arcade game emulator
 *
 * DAPHNE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * DAPHNE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h> // for lousy random number generation
#include <sys/types.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#ifdef MAC_OSX
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/host_priv.h>
#include <mach/machine.h>
#include <carbon/carbon.h>
#endif

#ifdef WIN32
#include <windows.h>
#include <VersionHelpers.h>
#endif

#ifdef LINUX
#include <sys/utsname.h> // MATT : I'm not sure if this is good for UNIX in general so I put it here
#include <sys/sysinfo.h>
#endif

#ifdef UNIX
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h> // for DNS
#include <sys/time.h>
#include <unistd.h> // for write
#endif

#include <zlib.h> // for crc32 calculation
#include "../io/error.h"
#include "../hypseus.h"
#include "network.h"

#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <cstdio>
#endif

static void safe_copy(char* dst, size_t dstsz, const char* src) {
    if (!dst || dstsz == 0) return;
    if (!src) { dst[0] = '\0'; return; }
#if defined(_WIN32) || defined(_WIN64)
    strncpy_s(dst, dstsz, src, _TRUNCATE);
#else
    std::strncpy(dst, src, dstsz - 1);
    dst[dstsz - 1] = '\0';
#endif
}

#if defined(__linux__)
static bool read_cpuinfo_field(const char* key, char* out, size_t outsz) {
    FILE* f = std::fopen("/proc/cpuinfo", "r");
    if (!f) return false;

    char line[512];
    const size_t klen = std::strlen(key);

    while (std::fgets(line, sizeof(line), f))
    {
        // Match "key : value" (allow spaces before ':')
        if (std::strncmp(line, key, klen) == 0)
        {
            const char* p = line + klen;
            while (*p == ' ' || *p == '\t') ++p;
            if (*p != ':') continue;
            ++p;
            while (*p == ' ' || *p == '\t') ++p;

            // Strip newline
            size_t len = std::strlen(p);
            while (len && (p[len - 1] == '\n' || p[len - 1] == '\r')) --len;

            if (len == 0) { std::fclose(f); return false; }

            char tmp[512];
            if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
            std::memcpy(tmp, p, len);
            tmp[len] = '\0';

            safe_copy(out, outsz, tmp);
            std::fclose(f);
            return true;
        }
    }

    std::fclose(f);
    return false;
}
#endif

bool g_send_data_to_server = false; // whether user allows us to send data to
                                   // server

////////////////////

// this is not the server you are looking for
void net_server_send() { g_send_data_to_server = false; }

int g_sockfd = -1;          // our socket file descriptor
struct net_packet g_packet; // what we're gonna send

void net_set_gamename(char *gamename)
{
    strncpy(g_packet.gamename, gamename, sizeof(g_packet.gamename)-1);
}

void net_set_ldpname(char *ldpname)
{
    strncpy(g_packet.ldpname, ldpname, sizeof(g_packet.ldpname)-1);
}

#if defined(_MSC_VER) && defined(_M_IX86)
// some code I found to calculate cpu mhz
_inline unsigned __int64 GetCycleCount(void) { _asm _emit 0x0F _asm _emit 0x31 }
#endif

// gets the cpu's memory, rounds to nearest 64 megs of RAM
unsigned int get_sys_mem()
{
    unsigned int result    = 0;
    unsigned int mod       = 0;
    unsigned long long mem = 0;
#ifdef LINUX
    struct sysinfo info;
    sysinfo(&info);
    mem = info.totalram * (unsigned long long)info.mem_unit;
#endif

#ifdef WIN32
    MEMORYSTATUSEX memstat;
    memstat.dwLength = sizeof(memstat);
    GlobalMemoryStatusEx(&memstat);
    mem = (unsigned long long)memstat.ullTotalPhys;
#endif

    result = (mem / 1024 / 1024) + 32; // for rounding
    mod    = result % 64;
    result -= mod;

    return result;
}

char *get_video_description()
{
    static char result[NET_LONGSTRSIZE] = {"Unknown video"};

#ifdef LINUX
#if defined(NATIVE_ARM)
     FILE *F;
     char video[64];
     const char *s = "cat /proc/cpuinfo | grep Hardware | sed -e 's/^.*: //' | head -1";
     F = popen(s, "r");
     if (F)
     {
            if (fscanf(F, "%s", video) == 1)
                strcpy(result, video);

            pclose(F);
     }
#elif defined(NATIVE_CPU_X86)
    FILE *F;
    // PCI query fix by Arnaud G. Gibert
    const char *s = "lspci | grep -i \"VGA compatible controller\" | awk -F ': "
                    "' '{print $2}'";
    F = popen(s, "r");
    if (F) {
        unsigned int len = fread(result, 1, 79, F);
        if (len > 1) result[len - 1] = 0; // make sure string is null terminated
        pclose(F);
    }
#endif
#endif

#ifdef WIN32
    typedef BOOL(WINAPI * infoproc)(PVOID, DWORD, PVOID, DWORD);
    infoproc pEnumDisplayDevices;
    HINSTANCE hInstUser32;
    DISPLAY_DEVICE DispDev;
    bool bEnumDisplayOk = true; // if it's ok to enumerate the display device
                                // (winNT may crash when doing this)

    // The call to EnumDisplayDevicesA may crash under WindowsNT.
    // Therefore we acquire the Windows version here, and if it's NT,
    // skip the following code.

    OSVERSIONINFOEX osvi;
    BOOL bOsVersionInfoEx;

    // Try calling GetVersionEx using the OSVERSIONINFOEX structure.
    // If that fails, try using the OSVERSIONINFO structure.

    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    if (!(bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO *)&osvi))) {
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

        // if GetVersionEx fails under all circumstances, then we will err on
        // the side of caution and not enumerate
        if (!GetVersionEx((OSVERSIONINFO *)&osvi)) {
            bEnumDisplayOk = false;
        }
    }

    // if it's Windows NT, then don't enumerate the display
    if ((osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osvi.dwMajorVersion <= 4)) {
        bEnumDisplayOk = false;
    }

    // if it's ok to enumerate the display device
    if (bEnumDisplayOk) {
        hInstUser32 = LoadLibrary("user32");
        if (hInstUser32) {
            pEnumDisplayDevices =
                (infoproc)GetProcAddress(hInstUser32, "EnumDisplayDevicesA");
            if (pEnumDisplayDevices) {
                ZeroMemory(&DispDev, sizeof(DISPLAY_DEVICE));
                DispDev.cb = sizeof(DISPLAY_DEVICE);
                if ((*pEnumDisplayDevices)(NULL, 0, &DispDev, 0)) {
                    strncpy(result, (char *)DispDev.DeviceString, sizeof(result) - 1);
                }
            }
            FreeLibrary(hInstUser32);
        }
    }
// else we can't enumerate
#endif

    return result;
}

char* get_cpu_name() {
    // Keep your original buffer size macro; this just uses it.
    static char result[NET_LONGSTRSIZE] = { 0 };
    safe_copy(result, sizeof(result), "UnknownCPU");

    // --- Windows ---
#if defined(_WIN32) || defined(_WIN64)

    // x86/x64: CPUID brand string (48 bytes across 0x80000002..4)
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
    {
        int regs[4] = { 0,0,0,0 };
        char brand[64] = { 0 };

        auto do_cpuid = [&](int leaf, int* r)
            {
#if defined(_MSC_VER)
                __cpuid(r, leaf);
#elif defined(__GNUC__) || defined(__clang__)
                __asm__ __volatile__(
                    "cpuid"
                    : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3])
                    : "a"(leaf), "c"(0)
                );
#else
                r[0] = r[1] = r[2] = r[3] = 0;
#endif
            };

        do_cpuid(0x80000000, regs);
        const unsigned int maxExt = (unsigned int)regs[0];

        if (maxExt >= 0x80000004)
        {
            int b[4];
            do_cpuid(0x80000002, b); std::memcpy(brand + 0, b, 16);
            do_cpuid(0x80000003, b); std::memcpy(brand + 16, b, 16);
            do_cpuid(0x80000004, b); std::memcpy(brand + 32, b, 16);
            brand[48] = '\0';

            // Trim leading spaces
            const char* p = brand;
            while (*p == ' ') ++p;
            if (*p) safe_copy(result, sizeof(result), p);
            return result;
        }
    }
#endif

    // Non-x86: fall back to architecture label
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64: safe_copy(result, sizeof(result), "x64"); break;
        case PROCESSOR_ARCHITECTURE_INTEL: safe_copy(result, sizeof(result), "x86"); break;
        case PROCESSOR_ARCHITECTURE_ARM:   safe_copy(result, sizeof(result), "ARM"); break;
        case PROCESSOR_ARCHITECTURE_ARM64: safe_copy(result, sizeof(result), "ARM64"); break;
        default:                           safe_copy(result, sizeof(result), "UnknownArch"); break;
    }
    return result;

    // --- macOS ---
#elif defined(__APPLE__)

    {
        char brand[256] = { 0 };
        size_t sz = sizeof(brand);
        if (sysctlbyname("machdep.cpu.brand_string", brand, &sz, nullptr, 0) == 0 && brand[0])
        {
            safe_copy(result, sizeof(result), brand);
            return result;
        }
    }

    // Fallback: machine type
    {
        char machine[256] = { 0 };
        size_t sz = sizeof(machine);
        if (sysctlbyname("hw.machine", machine, &sz, nullptr, 0) == 0 && machine[0])
        {
            safe_copy(result, sizeof(result), machine);
            return result;
        }
    }

    return result;

    // --- Linux ---
#elif defined(__linux__)

    // Prefer full model name on x86; ARM often uses "Model name", "Processor", or "Hardware".
    if (read_cpuinfo_field("model name", result, sizeof(result))) return result;
    if (read_cpuinfo_field("Model name", result, sizeof(result))) return result;
    if (read_cpuinfo_field("Processor", result, sizeof(result))) return result;
    if (read_cpuinfo_field("Hardware", result, sizeof(result))) return result;

    return result;

#else
    return result;
#endif
}

char *get_os_description()
{
    static char result[NET_LONGSTRSIZE] = {"Unknown OS"};

#ifdef LINUX
    struct utsname buf;
    int uname_result       = uname(&buf);

    // if uname did not return any error ...
    if (uname_result == 0) {
        strcpy(result, "Linux ");
        snprintf(&result[6], sizeof(result) - 6, "%s", buf.release);
    }

#endif

#ifdef WIN32
    strcpy(result, "Unknown Windows");

    if (IsWindows10OrGreater()) {
        strcpy(result, "Windows 10/11");
    } else if (IsWindows7OrGreater()) {
        strcpy(result, "Windows 7/8");
    } else if (IsWindowsVistaOrGreater()) {
        strcpy(result, "Windows Vista");
    } else if (IsWindowsXPOrGreater()) {
        strcpy(result, "Windows XP/2000");
    }
#endif

#ifdef MAC_OSX
    strcpy(result, "Mac OSX");
#endif

    return result;
}

char *get_sdl_compile()
{
    static char result[NET_LONGSTRSIZE] = {0};

    SDL_version compiled;
    SDL_version imgCompiled;
    SDL_version ttfCompiled;
    SDL_version mixCompiled;

    SDL_VERSION(&compiled);
    SDL_IMAGE_VERSION(&imgCompiled);
    SDL_TTF_VERSION(&ttfCompiled);
    SDL_MIXER_VERSION(&mixCompiled);

    snprintf(result, sizeof(result),
         "(CC) SDL: %d.%d.%d, "
         "IMG: %d.%d.%d, "
         "TTF: %d.%d.%d, "
         "MIX: %d.%d.%d",
         compiled.major, compiled.minor, compiled.patch,
         imgCompiled.major, imgCompiled.minor, imgCompiled.patch,
         ttfCompiled.major, ttfCompiled.minor, ttfCompiled.patch,
         mixCompiled.major, mixCompiled.minor, mixCompiled.patch);

    return result;
}

char *get_sdl_linked()
{
    static char result[NET_LONGSTRSIZE] = {0};

    SDL_version linked;

    SDL_GetVersion(&linked);
    const SDL_version* imgLinked = IMG_Linked_Version();
    const SDL_version* ttfLinked = TTF_Linked_Version();
    const SDL_version* mixLinked = Mix_Linked_Version();

    snprintf(result, sizeof(result),
         "(LD) SDL: %d.%d.%d, "
         "IMG: %d.%d.%d, "
         "TTF: %d.%d.%d, "
         "MIX: %d.%d.%d",
         linked.major, linked.minor, linked.patch,
         imgLinked->major, imgLinked->minor, imgLinked->patch,
         ttfLinked->major, ttfLinked->minor, ttfLinked->patch,
         mixLinked->major, mixLinked->minor, mixLinked->patch);

    return result;
}

char *get_build_time()
{
   static char result[NET_LONGSTRSIZE] = {0};
   static const char *built = __DATE__ " " __TIME__;

   snprintf(result, sizeof(result), "Compiled: %s", built);

   return result;
}

// DBX: Pretty certain MPO's server doesn't want these
// Disabled but rip it out for the paranoid....
void net_send_data_to_server()
{
    return;
}
