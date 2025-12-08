#include <Windows.h>
#include <string>
#include "pch.h"
#include "ModUtils.h"

using namespace ModUtils;
using namespace mINI;

// Timescale values loaded from INI
float g_timescaleF1 = 0.1f;
float g_timescaleF2 = 0.3f;
float g_timescaleF3 = 0.8f;
float g_normalTimescale = 1.0f;

// Helper function to get the folder where this DLL resides
std::string GetDLLFolder(HINSTANCE hModule)
{
    char path[MAX_PATH];
    GetModuleFileNameA(hModule, path, MAX_PATH);
    std::string fullPath(path);
    size_t pos = fullPath.find_last_of("\\/");
    return fullPath.substr(0, pos); // folder containing DLL
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
    HINSTANCE mod = (HINSTANCE)lpParam;
    std::string dllFolder = GetDLLFolder(mod);
    std::string modsFolder = dllFolder;

    // Create mods folder if it doesn't exist
    CreateDirectoryA(modsFolder.c_str(), nullptr);

    // Full path to config.ini
    std::string iniPath = modsFolder + "\\config.ini";

    Log("Loading config from: %s", iniPath.c_str());

    INIStructure config;
    INIFile file(iniPath);

    if (file.read(config)) {
        Log("Config loaded successfully.");

        g_timescaleF1 = std::stof(config["Timescale"]["F1"]);
        g_timescaleF2 = std::stof(config["Timescale"]["F2"]);
        g_timescaleF3 = std::stof(config["Timescale"]["F3"]);
        g_normalTimescale = std::stof(config["Timescale"]["Normal"]);
    }
    else {
        Log("Config missing! Creating default config.ini...");

        config["Timescale"]["F1"] = "0.1";
        config["Timescale"]["F2"] = "0.3";
        config["Timescale"]["F3"] = "0.8";
        config["Timescale"]["Normal"] = "1.0";

        file.generate(config);
    }

    // ---------- FIND TIMESCALE MANAGER ----------
    Log("Locating TimescaleManager...");

    std::string aobTimescale =
        "48 8b 05 ?? ?? ?? ?? f3 0f 10 88 cc 02 00 00";

    uintptr_t instrAddr = AobScan(aobTimescale);
    if (!instrAddr) {
        Log("Timescale AOB NOT FOUND!");
        return 0;
    }

    Log("Timescale AOB found at: 0x%llX", instrAddr);

    int32_t rel = *(int32_t*)(instrAddr + 3);
    uintptr_t* pTimescaleMgrPtr = (uintptr_t*)(instrAddr + 7 + rel);

    while (*pTimescaleMgrPtr == 0)
        Sleep(100);

    uintptr_t timescaleManager = *pTimescaleMgrPtr;
    float* pTimescale = (float*)(timescaleManager + 0x2CC);

    Log("Timescale float at: 0x%llX", (uintptr_t)pTimescale);
    Log("Hotkeys: F1/F2/F3 for custom timescales, F4 resets normal.");

    // ---------- MAIN LOOP ----------
    while (true)
    {
        if (GetAsyncKeyState(VK_F1) & 1) {
            *pTimescale = g_timescaleF1;
            Log("Timescale set to F1: %.2f", g_timescaleF1);
        }

        if (GetAsyncKeyState(VK_F2) & 1) {
            *pTimescale = g_timescaleF2;
            Log("Timescale set to F2: %.2f", g_timescaleF2);
        }

        if (GetAsyncKeyState(VK_F3) & 1) {
            *pTimescale = g_timescaleF3;
            Log("Timescale set to F3: %.2f", g_timescaleF3);
        }

        if (GetAsyncKeyState(VK_F4) & 1) {
            *pTimescale = g_normalTimescale;
            Log("Timescale reset to Normal: %.2f", g_normalTimescale);
        }

        Sleep(10);
    }

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE mod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(mod);
        CreateThread(nullptr, 0, MainThread, mod, 0, nullptr);
    }
    return TRUE;
}
