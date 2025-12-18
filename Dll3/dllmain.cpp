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

// ---------- ACTION TYPE ----------
enum class ActionType {
    Toggle,
    Hold
};

ActionType g_actionType = ActionType::Toggle;

// ---------- TIMESCALE CONFIG ----------
float g_timescaleF1 = 0.1f;
float g_timescaleF2 = 0.3f;
float g_timescaleF3 = 0.8f;
float g_normalTimescale = 1.0f;

// ---------- HOLD STATE ----------
bool g_f1Held = false;
bool g_f2Held = false;
bool g_f3Held = false;

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

// ---------- CONFIG ----------
void ReadConfig(const std::string& iniPath) {
    INIFile config(iniPath);
    INIStructure ini;

    if (config.read(ini)) {

        if (ini["Timescale"].has("Action_Type")) {
            std::string mode = ini["Timescale"]["Action_Type"];
            std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
            g_actionType = (mode == "hold") ? ActionType::Hold : ActionType::Toggle;
        }

        if (ini["Timescale"].has("F1_Value")) g_timescaleF1 = std::stof(ini["Timescale"]["F1_Value"]);
        if (ini["Timescale"].has("F2_Value")) g_timescaleF2 = std::stof(ini["Timescale"]["F2_Value"]);
        if (ini["Timescale"].has("F3_Value")) g_timescaleF3 = std::stof(ini["Timescale"]["F3_Value"]);
        if (ini["Timescale"].has("Normal_Value")) g_normalTimescale = std::stof(ini["Timescale"]["Normal_Value"]);

        if (ini["Timescale"].has("F1_Keys")) timescaleF1Keybinds = TranslateInput(ini["Timescale"]["F1_Keys"]);
        if (ini["Timescale"].has("F2_Keys")) timescaleF2Keybinds = TranslateInput(ini["Timescale"]["F2_Keys"]);
        if (ini["Timescale"].has("F3_Keys")) timescaleF3Keybinds = TranslateInput(ini["Timescale"]["F3_Keys"]);
        if (ini["Timescale"].has("Normal_Keys")) normalTimescaleKeybinds = TranslateInput(ini["Timescale"]["Normal_Keys"]);
    }
}

// ---------- INPUT CHECK (FIXED) ----------
bool IsKeybindPressed(const std::vector<Keybind>& keybinds, bool held) {
    for (auto& kb : keybinds) {
        if (kb.isControllerKeybind) {
            if (IsControllerComboPressed(kb)) return true;
        }
        else {
            if (AreKeysPressed(kb.keys, held)) return true;
        }
    }
    return false;
}

// ---------- MAIN THREAD ----------
DWORD WINAPI MainThread(LPVOID lpParam) {
    HINSTANCE mod = (HINSTANCE)lpParam;
    std::string iniPath = GetDLLFolder(mod) + "\\timescale_keybinds.ini";

    ReadConfig(iniPath);

    std::string aob = "48 8b 05 ?? ?? ?? ?? f3 0f 10 88 cc 02 00 00";
    uintptr_t instr = AobScan(aob);
    if (!instr) return 0;

    int32_t rel = *(int32_t*)(instr + 3);
    uintptr_t* pMgrPtr = (uintptr_t*)(instr + 7 + rel);
    while (*pMgrPtr == 0) Sleep(100);

    float* pTimescale = (float*)(*pMgrPtr + 0x2CC);

    while (true) {

        bool wantHeld = (g_actionType == ActionType::Hold);

        bool f1 = IsKeybindPressed(timescaleF1Keybinds, wantHeld);
        bool f2 = IsKeybindPressed(timescaleF2Keybinds, wantHeld);
        bool f3 = IsKeybindPressed(timescaleF3Keybinds, wantHeld);
        bool normal = IsKeybindPressed(normalTimescaleKeybinds, false);

        if (g_actionType == ActionType::Toggle) {

            if (f1) *pTimescale = g_timescaleF1;
            if (f2) *pTimescale = g_timescaleF2;
            if (f3) *pTimescale = g_timescaleF3;
            if (normal) *pTimescale = g_normalTimescale;

        }
        else { // HOLD MODE (FIXED)

            if (f1) { *pTimescale = g_timescaleF1; g_f1Held = true; }
            else if (g_f1Held) { *pTimescale = g_normalTimescale; g_f1Held = false; }

            if (f2) { *pTimescale = g_timescaleF2; g_f2Held = true; }
            else if (g_f2Held) { *pTimescale = g_normalTimescale; g_f2Held = false; }

            if (f3) { *pTimescale = g_timescaleF3; g_f3Held = true; }
            else if (g_f3Held) { *pTimescale = g_normalTimescale; g_f3Held = false; }
        }

        Sleep(10);
    }
}

// ---------- DLL ENTRY ----------
BOOL WINAPI DllMain(HINSTANCE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(mod);
        CreateThread(nullptr, 0, MainThread, mod, 0, nullptr);
    }
    return TRUE;
}
