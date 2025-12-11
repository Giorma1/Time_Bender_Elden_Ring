#include <Windows.h>
#include <Xinput.h>
#include <string>
#include <vector>
#include <algorithm>
#include "pch.h"
#include "ModUtils.h"
#include "InputTranslation.h"

#pragma comment(lib, "Xinput9_1_0.lib")

using namespace ModUtils;
using namespace mINI;

// ---------- TIMESCALE CONFIG ----------
float g_timescaleF1 = 0.1f;
float g_timescaleF2 = 0.3f;
float g_timescaleF3 = 0.8f;
float g_normalTimescale = 1.0f;

// ---------- KEYBIND STRUCT ----------
struct Keybind {
    std::vector<unsigned short> keys;
    bool isControllerKeybind;
};

// Timescale keybinds
std::vector<Keybind> timescaleF1Keybinds;
std::vector<Keybind> timescaleF2Keybinds;
std::vector<Keybind> timescaleF3Keybinds;
std::vector<Keybind> normalTimescaleKeybinds;

// ---------- HELPERS ----------
std::string GetDLLFolder(HINSTANCE hModule) {
    char path[MAX_PATH];
    GetModuleFileNameA(hModule, path, MAX_PATH);
    std::string full(path);
    size_t pos = full.find_last_of("\\/");
    return full.substr(0, pos);
}

// Split string helper
std::vector<std::string> splitString(const std::string& str, const std::string& delimiter) {
    size_t pos = 0;
    std::string s = str;
    std::vector<std::string> list;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        list.push_back(s.substr(0, pos));
        s.erase(0, pos + delimiter.size());
    }
    list.push_back(s);
    return list;
}

// Translate INI string to Keybinds
std::vector<Keybind> TranslateInput(const std::string& inputString) {
    std::vector<Keybind> keybinds;
    std::string str = inputString;
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);

    std::vector<std::string> splitOnComma = splitString(str, ",");
    for (auto& keybind : splitOnComma) {
        std::vector<std::string> splitOnPlus = splitString(keybind, "+");
        std::vector<unsigned short> keys;
        bool isController = false;

        for (auto& k : splitOnPlus) {
            auto search = keycodes.find(k);
            if (search != keycodes.end()) {
                keys.push_back(search->second);
                isController = false;
            }
            else if (controllerKeycodes.find(k) != controllerKeycodes.end()) {
                keys.push_back(controllerKeycodes.at(k));
                isController = true;
            }
        }
        keybinds.push_back({ keys, isController });
    }
    return keybinds;
}

// RT + controller combo check
bool IsControllerComboPressed(const Keybind& kb) {
    if (!kb.isControllerKeybind) return false;

    XINPUT_STATE state{};
    if (XInputGetState(0, &state) != ERROR_SUCCESS) return false;

    bool rtHeld = state.Gamepad.bRightTrigger > 30;
    for (auto btn : kb.keys) {
        if (!(state.Gamepad.wButtons & btn)) return false;
    }
    return rtHeld;
}

// Read keybinds and timescale values from INI
void ReadConfig(const std::string& iniPath) {
    INIFile config(iniPath);
    INIStructure ini;

    if (config.read(ini)) {
        // Timescale values
        if (ini["Timescale"].has("F1_Value")) g_timescaleF1 = std::stof(ini["Timescale"]["F1_Value"]);
        if (ini["Timescale"].has("F2_Value")) g_timescaleF2 = std::stof(ini["Timescale"]["F2_Value"]);
        if (ini["Timescale"].has("F3_Value")) g_timescaleF3 = std::stof(ini["Timescale"]["F3_Value"]);
        if (ini["Timescale"].has("Normal_Value")) g_normalTimescale = std::stof(ini["Timescale"]["Normal_Value"]);

        // Keybinds
        if (ini["Timescale"].has("F1_Keys")) timescaleF1Keybinds = TranslateInput(ini["Timescale"]["F1_Keys"]);
        if (ini["Timescale"].has("F2_Keys")) timescaleF2Keybinds = TranslateInput(ini["Timescale"]["F2_Keys"]);
        if (ini["Timescale"].has("F3_Keys")) timescaleF3Keybinds = TranslateInput(ini["Timescale"]["F3_Keys"]);
        if (ini["Timescale"].has("Normal_Keys")) normalTimescaleKeybinds = TranslateInput(ini["Timescale"]["Normal_Keys"]);
    }
    else {
        // Create defaults if INI does not exist
        ini["Timescale"]["F1_Value"] = "0.1";
        ini["Timescale"]["F1_Keys"] = "f1, lthumbpress+xa";

        ini["Timescale"]["F2_Value"] = "0.3";
        ini["Timescale"]["F2_Keys"] = "f2, lthumbpress+xy";

        ini["Timescale"]["F3_Value"] = "0.8";
        ini["Timescale"]["F3_Keys"] = "f3, lthumbpress+xx";

        ini["Timescale"]["Normal_Value"] = "1.0";
        ini["Timescale"]["Normal_Keys"] = "f4, lthumbpress+xb";

        config.write(ini, true);

        g_timescaleF1 = 0.1f; timescaleF1Keybinds = TranslateInput(ini["Timescale"]["F1_Keys"]);
        g_timescaleF2 = 0.3f; timescaleF2Keybinds = TranslateInput(ini["Timescale"]["F2_Keys"]);
        g_timescaleF3 = 0.8f; timescaleF3Keybinds = TranslateInput(ini["Timescale"]["F3_Keys"]);
        g_normalTimescale = 1.0f; normalTimescaleKeybinds = TranslateInput(ini["Timescale"]["Normal_Keys"]);
    }
}


// Check if any keybind in list is pressed
bool IsKeybindPressed(const std::vector<Keybind>& keybinds) {
    for (auto& kb : keybinds) {
        if (kb.isControllerKeybind) {
            if (IsControllerComboPressed(kb)) return true;
        }
        else {
            if (AreKeysPressed(kb.keys, false)) return true;
        }
    }
    return false;
}

// ---------- MAIN THREAD ----------
DWORD WINAPI MainThread(LPVOID lpParam) {
    HINSTANCE mod = (HINSTANCE)lpParam;
    std::string dllFolder = GetDLLFolder(mod);
    std::string iniPath = dllFolder + "\\timescale_keybinds.ini";

    Log("Loading config: %s", iniPath.c_str());
    ReadConfig(iniPath);

    // ---------- FIND TIMESCALE POINTER ----------
    std::string aob = "48 8b 05 ?? ?? ?? ?? f3 0f 10 88 cc 02 00 00";
    uintptr_t instr = AobScan(aob);
    if (!instr) { Log("AOB NOT FOUND!"); return 0; }

    int32_t rel = *(int32_t*)(instr + 3);
    uintptr_t* pMgrPtr = (uintptr_t*)(instr + 7 + rel);
    while (*pMgrPtr == 0) Sleep(100);

    float* pTimescale = (float*)(*pMgrPtr + 0x2CC);
    Log("Timescale float at: 0x%llX", (uintptr_t)pTimescale);

    // ---------- MAIN LOOP ----------
    while (true) {
        if (IsKeybindPressed(timescaleF1Keybinds)) *pTimescale = g_timescaleF1;
        if (IsKeybindPressed(timescaleF2Keybinds)) *pTimescale = g_timescaleF2;
        if (IsKeybindPressed(timescaleF3Keybinds)) *pTimescale = g_timescaleF3;
        if (IsKeybindPressed(normalTimescaleKeybinds)) *pTimescale = g_normalTimescale;

        Sleep(10);
    }

    return 0;
}

// ---------- DLL ENTRY ----------
BOOL WINAPI DllMain(HINSTANCE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(mod);
        CreateThread(nullptr, 0, MainThread, mod, 0, nullptr);
    }
    return TRUE;
}
