#define NOMINMAX
#include <windows.h>
#include <thread>
#include <atomic>
#include <random>
#include <string>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "gamesense_ui.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <mutex>
#include <chrono>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib")

// Resource IDs
#define IDI_ICON1 101

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// Global state
std::atomic<bool> g_running(true);
std::atomic<bool> g_autoclicker_active(false);
std::atomic<bool> g_mouselock_active(false);
std::atomic<bool> g_autobolt_active(false);
std::atomic<bool> g_autobolt_executing(false);
std::atomic<bool> g_autorotate_active(false);
std::atomic<bool> g_autorotate_executing(false);
std::mutex g_config_mutex;
std::mutex g_serial_mutex;

// Arduino connection
HANDLE g_serialPort = INVALID_HANDLE_VALUE;
std::atomic<bool> g_arduino_connected(false);
std::atomic<bool> g_arduino_clicking(false);

struct Config {
    // Autoclicker (Software)
    int sw_cps = 100;
    bool sw_use_jitter = false;
    int sw_jitter_range = 2;
    int sw_mouse_button = 0;
    int sw_hotkey = VK_F6;
    
    // Autoclicker (Arduino)
    int ar_cps = 100;
    bool ar_use_jitter = false;
    int ar_jitter_range = 2;
    int ar_mouse_button = 0;
    int ar_hotkey = 'Q';
    char ar_com_port[16] = "COM3";
    
    // Healing
    int ml_target_x = 1500;
    int ml_target_y = 250;
    float ml_lock_duration = 0.3f;
    int ml_hotkey = 'C';
    
    // Auto Bolt
    int ab_hotkey = 'V';
    
    // Auto Rotate
    int autorotate_target_x = 800;
    int autorotate_target_y = 400;
    int autorotate_hotkey = 'X';
        
    // UI
    bool capturing_hotkey = false;
    int capturing_hotkey_for = 0;  // 0=sw, 1=ar, 2=ml, 3=ab, 4=rotate
    int capture_wait_frames = 0;
    int hotkey_cooldown_frames = 0;
} g_config;

// Direct3D
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void EnableDarkTitleBar(HWND hwnd) {
    BOOL useDark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
}

void SaveConfig() {
    FILE* f = nullptr;
    fopen_s(&f, "configs.ini", "w");
    if (f) {
        fprintf(f, "[SoftwareAutoClicker]\n");
        fprintf(f, "CPS=%d\n", g_config.sw_cps);
        fprintf(f, "UseJitter=%d\n", g_config.sw_use_jitter ? 1 : 0);
        fprintf(f, "JitterRange=%d\n", g_config.sw_jitter_range);
        fprintf(f, "MouseButton=%d\n", g_config.sw_mouse_button);
        fprintf(f, "Hotkey=%d\n", g_config.sw_hotkey);
        
        fprintf(f, "\n[ArduinoAutoClicker]\n");
        fprintf(f, "CPS=%d\n", g_config.ar_cps);
        fprintf(f, "UseJitter=%d\n", g_config.ar_use_jitter ? 1 : 0);
        fprintf(f, "JitterRange=%d\n", g_config.ar_jitter_range);
        fprintf(f, "MouseButton=%d\n", g_config.ar_mouse_button);
        fprintf(f, "Hotkey=%d\n", g_config.ar_hotkey);
        fprintf(f, "COMPort=%s\n", g_config.ar_com_port);
        
        fprintf(f, "\n[Healing]\n");
        fprintf(f, "TargetX=%d\n", g_config.ml_target_x);
        fprintf(f, "TargetY=%d\n", g_config.ml_target_y);
        fprintf(f, "LockDuration=%.2f\n", g_config.ml_lock_duration);
        fprintf(f, "Hotkey=%d\n", g_config.ml_hotkey);
        
        fprintf(f, "\n[AutoBolt]\n");
        fprintf(f, "Hotkey=%d\n", g_config.ab_hotkey);
        
        fprintf(f, "\n[AutoRotate]\n");
        fprintf(f, "TargetX=%d\n", g_config.autorotate_target_x);
        fprintf(f, "TargetY=%d\n", g_config.autorotate_target_y);
        fprintf(f, "Hotkey=%d\n", g_config.autorotate_hotkey);
        
        fclose(f);
    }
}

void LoadConfig() {
    FILE* f = nullptr;
    fopen_s(&f, "configs.ini", "r");
    if (f) {
        char line[256];
        std::string section;
        
        while (fgets(line, sizeof(line), f)) {
            std::string str(line);
            if (str[0] == '[') {
                size_t end = str.find(']');
                if (end != std::string::npos) {
                    section = str.substr(1, end - 1);
                }
                continue;
            }
            
            int temp;
            if (section == "SoftwareAutoClicker") {
                if (sscanf_s(line, "CPS=%d", &temp) == 1) g_config.sw_cps = temp;
                if (sscanf_s(line, "UseJitter=%d", &temp) == 1) g_config.sw_use_jitter = temp != 0;
                if (sscanf_s(line, "JitterRange=%d", &temp) == 1) g_config.sw_jitter_range = temp;
                if (sscanf_s(line, "MouseButton=%d", &temp) == 1) g_config.sw_mouse_button = temp;
                if (sscanf_s(line, "Hotkey=%d", &temp) == 1) g_config.sw_hotkey = temp;
            }
            else if (section == "ArduinoAutoClicker") {
                if (sscanf_s(line, "CPS=%d", &temp) == 1) g_config.ar_cps = temp;
                if (sscanf_s(line, "UseJitter=%d", &temp) == 1) g_config.ar_use_jitter = temp != 0;
                if (sscanf_s(line, "JitterRange=%d", &temp) == 1) g_config.ar_jitter_range = temp;
                if (sscanf_s(line, "MouseButton=%d", &temp) == 1) g_config.ar_mouse_button = temp;
                if (sscanf_s(line, "Hotkey=%d", &temp) == 1) g_config.ar_hotkey = temp;
                sscanf_s(line, "COMPort=%s", g_config.ar_com_port, (unsigned)sizeof(g_config.ar_com_port));
            }
            else if (section == "Healing") {
                if (sscanf_s(line, "TargetX=%d", &temp) == 1) g_config.ml_target_x = temp;
                if (sscanf_s(line, "TargetY=%d", &temp) == 1) g_config.ml_target_y = temp;
                float ftemp;
                if (sscanf_s(line, "LockDuration=%f", &ftemp) == 1) g_config.ml_lock_duration = ftemp;
                if (sscanf_s(line, "Hotkey=%d", &temp) == 1) g_config.ml_hotkey = temp;
            }
            else if (section == "AutoBolt") {
                if (sscanf_s(line, "Hotkey=%d", &temp) == 1) g_config.ab_hotkey = temp;
            }
            else if (section == "AutoRotate") {
                if (sscanf_s(line, "TargetX=%d", &temp) == 1) g_config.autorotate_target_x = temp;
                if (sscanf_s(line, "TargetY=%d", &temp) == 1) g_config.autorotate_target_y = temp;
                if (sscanf_s(line, "Hotkey=%d", &temp) == 1) g_config.autorotate_hotkey = temp;
            }
        }
        fclose(f);
    }
}

// ============================================================================
// ARDUINO COMMUNICATION
// ============================================================================

bool OpenSerialPort(const char* portName) {
    std::lock_guard<std::mutex> lock(g_serial_mutex);
    std::string fullPortName = std::string("\\\\.\\") + portName;
    
    g_serialPort = CreateFileA(
        fullPortName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr
    );
    
    if (g_serialPort == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    
    if (!GetCommState(g_serialPort, &dcbSerialParams)) {
        CloseHandle(g_serialPort);
        g_serialPort = INVALID_HANDLE_VALUE;
        return false;
    }
    
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    
    if (!SetCommState(g_serialPort, &dcbSerialParams)) {
        CloseHandle(g_serialPort);
        g_serialPort = INVALID_HANDLE_VALUE;
        return false;
    }
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    
    if (!SetCommTimeouts(g_serialPort, &timeouts)) {
        CloseHandle(g_serialPort);
        g_serialPort = INVALID_HANDLE_VALUE;
        g_arduino_connected = false;  // FIX: Set connected flag
        return false;
    }
    
    return true;
}

void CloseSerialPort() {
    std::lock_guard<std::mutex> lock(g_serial_mutex);
    
    if (g_serialPort != INVALID_HANDLE_VALUE) {
        CloseHandle(g_serialPort);
        g_serialPort = INVALID_HANDLE_VALUE;
    }
}

bool SendCommand(const char* command) {
    std::lock_guard<std::mutex> lock(g_serial_mutex);
    
    if (g_serialPort == INVALID_HANDLE_VALUE) return false;
    
    std::string cmd = std::string(command) + "\n";
    DWORD bytesWritten;
    return WriteFile(g_serialPort, cmd.c_str(), cmd.length(), &bytesWritten, nullptr);
}

std::string ReadResponse() {
    std::lock_guard<std::mutex> lock(g_serial_mutex);
    
    if (g_serialPort == INVALID_HANDLE_VALUE) return "";
    
    std::string response;
    char buffer[256];
    DWORD bytesRead;
    
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(200)) {
        if (ReadFile(g_serialPort, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                response += buffer;
                
                if (response.find('\n') != std::string::npos) {
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return response;
}

Config GetConfigSnapshot() {
    std::lock_guard<std::mutex> lock(g_config_mutex);
    return g_config;
}

void ArduinoConnectionThread() {
    while (g_running) {
        Config cfg = GetConfigSnapshot();
        
        if (g_serialPort == INVALID_HANDLE_VALUE) {
            if (OpenSerialPort(cfg.ar_com_port)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                
                SendCommand("PING");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string response = ReadResponse();
                
                if (response.find("PONG") != std::string::npos) {
                    g_arduino_connected = true;
                    
                    char cmd[64];
                    sprintf_s(cmd, "CPS:%d", cfg.ar_cps);
                    SendCommand(cmd);
                    
                    sprintf_s(cmd, "JITTER:%d", cfg.ar_use_jitter ? 1 : 0);
                    SendCommand(cmd);
                    
                    sprintf_s(cmd, "JITTER_RANGE:%d", cfg.ar_jitter_range);
                    SendCommand(cmd);
                    
                    sprintf_s(cmd, "BUTTON:%d", cfg.ar_mouse_button);
                    SendCommand(cmd);
                } else {
                    CloseSerialPort();
                }
            }
        } else {
            // ... heartbeat code ...
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

// ============================================================================
// SOFTWARE AUTOCLICKER
// ============================================================================

void SoftwareAutoClickerThread() {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    using clock = std::chrono::high_resolution_clock;
    using microseconds = std::chrono::microseconds;
    
    while (g_running) {
        if (!g_autoclicker_active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        Config cfg = GetConfigSnapshot();
        
        int target_delay_us = (1000000 / cfg.sw_cps);
        
        if (cfg.sw_use_jitter) {
            int jitter_us = cfg.sw_jitter_range * 1000;
            int lower_bound = std::max(100, target_delay_us - jitter_us);
            int upper_bound = target_delay_us + jitter_us;
            std::uniform_int_distribution<> jitter_dist(lower_bound, upper_bound);
            target_delay_us = jitter_dist(gen);
        }
        
        if (target_delay_us < 100) target_delay_us = 100;
        
        auto expected_time = clock::now() + microseconds(target_delay_us);
        
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_MOUSE;
        switch (cfg.sw_mouse_button) {
            case 0: inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; break;
            case 1: inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; break;
            case 2: inputs[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN; break;
        }
        
        inputs[1].type = INPUT_MOUSE;
        switch (cfg.sw_mouse_button) {
            case 0: inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP; break;
            case 1: inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP; break;
            case 2: inputs[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP; break;
        }
        
        SendInput(2, inputs, sizeof(INPUT));
        
        auto now = clock::now();
        auto time_to_wait = std::chrono::duration_cast<microseconds>(expected_time - now);
        
        if (time_to_wait.count() > 0) {
            std::this_thread::sleep_for(time_to_wait);
        }
    }
}

// ============================================================================
// Healing
// ============================================================================

void WiggleCursor() {
    mouse_event(MOUSEEVENTF_MOVE, 1, 0, 0, 0);
    mouse_event(MOUSEEVENTF_MOVE, -1, 0, 0, 0);
}

void HealingThread() {
    while (g_running) {
        if (!g_mouselock_active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        SetCursorPos(g_config.ml_target_x, g_config.ml_target_y);
        WiggleCursor();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ============================================================================
// AUTO BOLT (Spam V 3 times)
// ============================================================================

void AutoBoltThread() {
    while (g_running) {
        if (!g_autobolt_active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        if (g_autobolt_executing) {
            g_autobolt_active = false;
            continue;
        }
        
        g_autobolt_executing = true;
        g_autobolt_active = false; 
        
        BYTE vScanCode = MapVirtualKey('V', MAPVK_VK_TO_VSC);
        
        for (int i = 0; i < 3; i++) {
            keybd_event('V', vScanCode, 0, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            
            keybd_event('V', vScanCode, KEYEVENTF_KEYUP, 0);
            
            if (i < 2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        g_autobolt_executing = false;
    }
}

// ============================================================================
// AUTO ROTATE (Press Z, click rotate pos, press C for mouse locker, repeat)
// ============================================================================

void AutoRotateThread() {
    while (g_running) {
        if (!g_autorotate_active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        if (g_autorotate_executing) {
            g_autorotate_active = false;
            continue;
        }
        
        g_autorotate_executing = true;
        
        // Loop while active
        while (g_autorotate_active) {
            // EMERGENCY STOP - Press ESC to stop
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                g_autorotate_active = false;
                break;
            }
            
            // Step 1: Press Z
            BYTE zScanCode = MapVirtualKey('Z', MAPVK_VK_TO_VSC);
            keybd_event('Z', zScanCode, 0, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            keybd_event('Z', zScanCode, KEYEVENTF_KEYUP, 0);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Step 2: Teleport to rotate position and click
            SetCursorPos(g_config.autorotate_target_x, g_config.autorotate_target_y);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            
            // Left click
            INPUT inputs[2] = {};
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            inputs[1].type = INPUT_MOUSE;
            inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(2, inputs, sizeof(INPUT));
            
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            
            // Step 3: Press C to activate mouse locker
            BYTE cScanCode = MapVirtualKey('C', MAPVK_VK_TO_VSC);
            keybd_event('C', cScanCode, 0, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            keybd_event('C', cScanCode, KEYEVENTF_KEYUP, 0);
            
            // Step 4: Wait 0.125 seconds (125ms) - check for stop every 25ms
            for (int i = 0; i < 5; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                
                // Check if ESC or toggle key pressed during wait
                if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) || 
                    (GetAsyncKeyState(g_config.autorotate_hotkey) & 0x8000)) {
                    g_autorotate_active = false;
                    break;
                }
            }
        }
        
        g_autorotate_executing = false;
    }
}

// ============================================================================
// HOTKEY THREAD
// ============================================================================

void HotkeyThread() {
    bool last_sw_state = false;
    bool last_ar_state = false;
    bool last_ml_state = false;
    bool last_ab_state = false;
    bool last_autorotate_state = false;
    
    auto last_ml_time = std::chrono::steady_clock::now();
    
    while (g_running) {
        Config cfg = GetConfigSnapshot();
        
        if (cfg.hotkey_cooldown_frames > 0) {
            {
                std::lock_guard<std::mutex> lock(g_config_mutex);
                g_config.hotkey_cooldown_frames--;
            }
            
            last_sw_state = false;
            last_ar_state = false;
            last_ml_state = false;
            last_ab_state = false;
            last_autorotate_state = false;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        
        bool sw_current = (GetAsyncKeyState(cfg.sw_hotkey) & 0x8000) != 0;
        if (sw_current && !last_sw_state) {
            g_autoclicker_active = !g_autoclicker_active;
        }
        last_sw_state = sw_current;
        
        bool ar_current = (GetAsyncKeyState(cfg.ar_hotkey) & 0x8000) != 0;
        if (ar_current && !last_ar_state && g_arduino_connected) {
            g_arduino_clicking = !g_arduino_clicking;
            SendCommand(g_arduino_clicking ? "START" : "STOP");
        }
        last_ar_state = ar_current;
        
        bool ml_current = (GetAsyncKeyState(cfg.ml_hotkey) & 0x8000) != 0;
        if (ml_current && !last_ml_state) {
            g_mouselock_active = true;
            last_ml_time = std::chrono::steady_clock::now();
        }
        last_ml_state = ml_current;
        
        if (g_mouselock_active) {
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - last_ml_time).count();
            if (elapsed >= cfg.ml_lock_duration) {
                g_mouselock_active = false;
            }
        }
        
        bool ab_current = (GetAsyncKeyState(cfg.ab_hotkey) & 0x8000) != 0;
        if (ab_current && !last_ab_state && !g_autobolt_executing) {
            g_autobolt_active = true;
        }
        last_ab_state = ab_current;
        
        bool autorotate_current = (GetAsyncKeyState(cfg.autorotate_hotkey) & 0x8000) != 0;
        if (autorotate_current && !last_autorotate_state && !g_autorotate_executing) {
            g_autorotate_active = !g_autorotate_active;
        }
        last_autorotate_state = autorotate_current;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

const char* GetKeyName(int vk) {
    static char keyName[32];
    switch (vk) {
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        case VK_SPACE: return "SPACE";
        case VK_SHIFT: return "SHIFT";
        case VK_CONTROL: return "CTRL";
        case VK_MENU: return "ALT";
        case VK_TAB: return "TAB";
        case VK_XBUTTON1: return "Mouse 4";
        case VK_XBUTTON2: return "Mouse 5";
        case VK_LBUTTON: return "Left Click";
        case VK_RBUTTON: return "Right Click";
        case VK_MBUTTON: return "Middle Click";
        default:
            if (vk >= 0x41 && vk <= 0x5A) {
                sprintf_s(keyName, "%c", (char)vk);
                return keyName;
            }
            if (vk >= 0x30 && vk <= 0x39) {
                sprintf_s(keyName, "%c", (char)vk);
                return keyName;
            }
            sprintf_s(keyName, "Key %d", vk);
            return keyName;
    }
}

int CaptureVirtualKey()
{
    for (int vk = 0x01; vk <= 0x06; ++vk) {
        if (GetAsyncKeyState(vk) & 0x8000)
            return vk;
    }

    for (int vk = 0x08; vk <= 0xFE; ++vk) {
        if (GetAsyncKeyState(vk) & 0x8000)
            return vk;
    }

    return 0;
}

// ============================================================================
// MAIN
// ============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    timeBeginPeriod(1);
    
    // Load config
    LoadConfig();
    
    // Create window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"RobloxTools", nullptr };
    
    // Load icon from resource - Windows will handle cleanup automatically
    wc.hIcon = (HICON)LoadImage(
        wc.hInstance,
        MAKEINTRESOURCE(IDI_ICON1),
        IMAGE_ICON,
        0, 0,
        LR_DEFAULTSIZE | LR_SHARED
    );
    wc.hIconSm = (HICON)LoadImage(
        wc.hInstance,
        MAKEINTRESOURCE(IDI_ICON1),
        IMAGE_ICON,
        0, 0,
        LR_DEFAULTSIZE | LR_SHARED
    );
    
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName, L"Booga Closets",
        WS_OVERLAPPEDWINDOW,
        100, 100, 750, 600, nullptr, nullptr, wc.hInstance, nullptr);

    EnableDarkTitleBar(hwnd);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);
    
    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    GameSenseUI::SetupGamesenseStyle();
    
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    
    // Start threads
    std::thread arduino_thread(ArduinoConnectionThread);
    std::thread sw_autoclicker_thread(SoftwareAutoClickerThread);
    std::thread mouselock_thread(HealingThread);
    std::thread autobolt_thread(AutoBoltThread);
    std::thread autorotate_thread(AutoRotateThread);
    std::thread hotkey_thread(HotkeyThread);
    
    // Main loop
    bool show_app = true;
    ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT) {
        if (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }
        
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        if (show_app) {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            
            ImGui::Begin("Booga Closets", &show_app, 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoCollapse);
            
            if (ImGui::BeginTabBar("MainTabs")) {
                
                // ================================================================
                // SOFTWARE AUTOCLICKER TAB
                // ================================================================
                if (ImGui::BeginTabItem("Software Autoclicker")) {
                    ImGui::Spacing();
                    ImGui::Text("Status: ");
                    ImGui::SameLine();
                    if (g_autoclicker_active) {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[ACTIVE]");
                    } else {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[IDLE]");
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // Read old values outside lock for comparison
                    int old_sw_cps = g_config.sw_cps;
                    bool old_sw_jitter = g_config.sw_use_jitter;
                    int old_sw_jitter_range = g_config.sw_jitter_range;
                    int old_sw_button = g_config.sw_mouse_button;
                    
                    {
                        std::lock_guard<std::mutex> lock(g_config_mutex);
                        ImGui::InputInt("Clicks Per Second", &g_config.sw_cps);
                        ImGui::Spacing();
                        ImGui::Checkbox("Use Jitter", &g_config.sw_use_jitter);
                        
                        if (g_config.sw_use_jitter) {
                            ImGui::SliderInt("Jitter Range (ms)", &g_config.sw_jitter_range, 1, 20);
                        }
                        
                        ImGui::Spacing();
                        ImGui::Text("Mouse Button:");
                        ImGui::RadioButton("Left Click##sw", &g_config.sw_mouse_button, 0);
                        ImGui::SameLine();
                        ImGui::RadioButton("Right Click##sw", &g_config.sw_mouse_button, 1);
                        ImGui::SameLine();
                        ImGui::RadioButton("Middle Click##sw", &g_config.sw_mouse_button, 2);
                    }
                    
                    if (old_sw_cps != g_config.sw_cps || 
                        old_sw_jitter != g_config.sw_use_jitter ||
                        old_sw_jitter_range != g_config.sw_jitter_range ||
                        old_sw_button != g_config.sw_mouse_button) {
                        SaveConfig();
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    char sw_hotkey_text[128];
                    sprintf_s(sw_hotkey_text, "Toggle Hotkey: %s", GetKeyName(g_config.sw_hotkey));
                    if (ImGui::Button(sw_hotkey_text, ImVec2(200, 0))) {
                        std::lock_guard<std::mutex> lock(g_config_mutex);
                        g_config.capturing_hotkey = true;
                        g_config.capturing_hotkey_for = 0;
                        g_config.capture_wait_frames = 0;
                    }

                    if (g_config.capturing_hotkey && g_config.capturing_hotkey_for == 0) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.00f), "Press any key...");
                        
                        if (g_config.capture_wait_frames < 10) {
                            std::lock_guard<std::mutex> lock(g_config_mutex);
                            g_config.capture_wait_frames++;
                        } else {
                            int vk = CaptureVirtualKey();
                            if (vk != 0) {
                                std::lock_guard<std::mutex> lock(g_config_mutex);
                                g_config.sw_hotkey = vk;
                                g_config.capturing_hotkey = false;
                                g_config.capturing_hotkey_for = 0;
                                g_config.capture_wait_frames = 0;
                                g_config.hotkey_cooldown_frames = 60;
                                SaveConfig();
                            }
                        }
                    }
                    
                    ImGui::EndTabItem();
                }
                
                // ================================================================
                // ARDUINO AUTOCLICKER TAB
                // ================================================================
                if (ImGui::BeginTabItem("Arduino Autoclicker")) {
                    ImGui::Spacing();
                    ImGui::Text("Arduino Status: ");
                    ImGui::SameLine();
                    if (g_arduino_connected) {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[CONNECTED]");
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[DISCONNECTED]");
                    }
                    
                    ImGui::SameLine();
                    ImGui::Text(" | Clicking: ");
                    ImGui::SameLine();
                    if (g_arduino_clicking) {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[ACTIVE]");
                    } else {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[IDLE]");
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // Read old values outside lock
                    int old_ar_cps = g_config.ar_cps;
                    bool old_ar_jitter = g_config.ar_use_jitter;
                    int old_ar_jitter_range = g_config.ar_jitter_range;
                    int old_ar_button = g_config.ar_mouse_button;
                    char old_com_port[16];
                    strcpy_s(old_com_port, g_config.ar_com_port);
                    
                    {
                        std::lock_guard<std::mutex> lock(g_config_mutex);
                        ImGui::InputText("COM Port", g_config.ar_com_port, sizeof(g_config.ar_com_port));
                        ImGui::Spacing();
                        ImGui::InputInt("Clicks Per Second##ar", &g_config.ar_cps);
                        ImGui::Spacing();
                        ImGui::Checkbox("Use Jitter##ar", &g_config.ar_use_jitter);
                        
                        if (g_config.ar_use_jitter) {
                            ImGui::SliderInt("Jitter Range (ms)##ar", &g_config.ar_jitter_range, 1, 20);
                        }
                        
                        ImGui::Spacing();
                        ImGui::Text("Mouse Button:");
                        ImGui::RadioButton("Left Click##ar", &g_config.ar_mouse_button, 0);
                        ImGui::SameLine();
                        ImGui::RadioButton("Right Click##ar", &g_config.ar_mouse_button, 1);
                        ImGui::SameLine();
                        ImGui::RadioButton("Middle Click##ar", &g_config.ar_mouse_button, 2);
                    }
                    
                    bool config_changed = (old_ar_cps != g_config.ar_cps || 
                                        old_ar_jitter != g_config.ar_use_jitter ||
                                        old_ar_jitter_range != g_config.ar_jitter_range ||
                                        old_ar_button != g_config.ar_mouse_button ||
                                        strcmp(old_com_port, g_config.ar_com_port) != 0);
                    
                    if (config_changed) {
                        SaveConfig();
                        
                        if (g_arduino_connected) {
                            char cmd[64];
                            sprintf_s(cmd, "CPS:%d", g_config.ar_cps);
                            SendCommand(cmd);
                            
                            sprintf_s(cmd, "JITTER:%d", g_config.ar_use_jitter ? 1 : 0);
                            SendCommand(cmd);
                            
                            sprintf_s(cmd, "JITTER_RANGE:%d", g_config.ar_jitter_range);
                            SendCommand(cmd);
                            
                            sprintf_s(cmd, "BUTTON:%d", g_config.ar_mouse_button);
                            SendCommand(cmd);
                        }
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    char ar_hotkey_text[128];
                    sprintf_s(ar_hotkey_text, "Toggle Hotkey: %s", GetKeyName(g_config.ar_hotkey));
                    if (ImGui::Button(ar_hotkey_text, ImVec2(200, 0))) {
                        std::lock_guard<std::mutex> lock(g_config_mutex);
                        g_config.capturing_hotkey = true;
                        g_config.capturing_hotkey_for = 0;
                        g_config.capture_wait_frames = 0;
                    }
                    
                    if (g_config.capturing_hotkey && g_config.capturing_hotkey_for == 0) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.00f), "Press any key...");
                        
                        if (g_config.capture_wait_frames < 10) {
                            std::lock_guard<std::mutex> lock(g_config_mutex);
                            g_config.capture_wait_frames++;
                        } else {
                            int vk = CaptureVirtualKey();
                            if (vk != 0) {
                                std::lock_guard<std::mutex> lock(g_config_mutex);
                                g_config.ar_hotkey = vk;
                                g_config.capturing_hotkey = false;
                                g_config.capturing_hotkey_for = 0;
                                g_config.capture_wait_frames = 0;
                                g_config.hotkey_cooldown_frames = 60;
                                SaveConfig();
                            }
                        }
                    }
                    
                    ImGui::EndTabItem();
                }
                
                // ================================================================
                // HEALING TAB
                // ================================================================
                if (ImGui::BeginTabItem("Healing")) {
                    ImGui::Spacing();
                    ImGui::Text("Status: ");
                    ImGui::SameLine();
                    if (g_mouselock_active) {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[ACTIVE]");
                    } else {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[IDLE]");
                    }
                    
                    ImGui::Spacing();
                    ImGui::TextWrapped("Locks mouse to target position for healing.");
                    ImGui::Spacing();
                    
                    int old_ml_x = g_config.ml_target_x;
                    int old_ml_y = g_config.ml_target_y;
                    float old_ml_duration = g_config.ml_lock_duration;
                    
                    {
                        std::lock_guard<std::mutex> lock(g_config_mutex);
                        ImGui::InputInt("Target X", &g_config.ml_target_x);
                        ImGui::InputInt("Target Y", &g_config.ml_target_y);
                        ImGui::SliderFloat("Lock Duration (seconds)", &g_config.ml_lock_duration, 0.1f, 2.0f);
                    }
                    
                    if (old_ml_x != g_config.ml_target_x || 
                        old_ml_y != g_config.ml_target_y ||
                        old_ml_duration != g_config.ml_lock_duration) {
                        SaveConfig();
                    }
                    
                    ImGui::Spacing();
                    
                    POINT cursorPos;
                    GetCursorPos(&cursorPos);
                    ImGui::Text("Current mouse: (%d, %d)", cursorPos.x, cursorPos.y);
                    
                    if (ImGui::Button("Set to Current Position")) {
                        std::lock_guard<std::mutex> lock(g_config_mutex);
                        g_config.ml_target_x = cursorPos.x;
                        g_config.ml_target_y = cursorPos.y;
                        SaveConfig();
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    char ml_hotkey_text[128];
                    sprintf_s(ml_hotkey_text, "Activate Hotkey: %s", GetKeyName(g_config.ml_hotkey));
                    if (ImGui::Button(ml_hotkey_text, ImVec2(200, 0))) {
                        std::lock_guard<std::mutex> lock(g_config_mutex);
                        g_config.capturing_hotkey = true;
                        g_config.capturing_hotkey_for = 0;
                        g_config.capture_wait_frames = 0;
                    }
                    
                    if (g_config.capturing_hotkey && g_config.capturing_hotkey_for == 0) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.00f), "Press any key...");
                        
                        if (g_config.capture_wait_frames < 10) {
                            std::lock_guard<std::mutex> lock(g_config_mutex);
                            g_config.capture_wait_frames++;
                        } else {
                            int vk = CaptureVirtualKey();
                            if (vk != 0) {
                                std::lock_guard<std::mutex> lock(g_config_mutex);
                                g_config.ml_hotkey = vk;
                                g_config.capturing_hotkey = false;
                                g_config.capturing_hotkey_for = 0;
                                g_config.capture_wait_frames = 0;
                                g_config.hotkey_cooldown_frames = 60;
                                SaveConfig();
                            }
                        }
                    }
                    
                    ImGui::EndTabItem();
                }
                
                // ================================================================
                // AUTO BOLT TAB
                // ================================================================
                if (ImGui::BeginTabItem("Auto Bolt")) {
                    ImGui::Spacing();
                    ImGui::Text("Status: ");
                    ImGui::SameLine();
                    if (g_autobolt_executing) {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[EXECUTING]");
                    } else if (g_autobolt_active) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[READY]");
                    } else {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[IDLE]");
                    }
                    
                    ImGui::Spacing();
                    ImGui::TextWrapped("Presses 'Z' key then clicks at healing position. One-shot trigger.");
                    ImGui::Spacing();
                    
                    char ab_hotkey_text[128];
                    sprintf_s(ab_hotkey_text, "Trigger: %s", GetKeyName(g_config.ab_hotkey));
                    if (ImGui::Button(ab_hotkey_text, ImVec2(200, 0))) {
                        std::lock_guard<std::mutex> lock(g_config_mutex);
                        g_config.capturing_hotkey = true;
                        g_config.capturing_hotkey_for = 1;
                        g_config.capture_wait_frames = 0;
                    }
                    
                    ImGui::SameLine();
                    if (ImGui::Button("Test Now", ImVec2(120, 0))) {
                        g_autobolt_active = true;
                    }
                    
                    if (g_config.capturing_hotkey && g_config.capturing_hotkey_for == 1) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.00f), "Press any key...");
                        
                        if (g_config.capture_wait_frames < 10) {
                            std::lock_guard<std::mutex> lock(g_config_mutex);
                            g_config.capture_wait_frames++;
                        } else {
                            int vk = CaptureVirtualKey();
                            if (vk != 0) {
                                std::lock_guard<std::mutex> lock(g_config_mutex);
                                g_config.ab_hotkey = vk;
                                g_config.capturing_hotkey = false;
                                g_config.capturing_hotkey_for = 0;
                                g_config.capture_wait_frames = 0;
                                g_config.hotkey_cooldown_frames = 60;
                                SaveConfig();
                            }
                        }
                    }
                    
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "Note: Uses Healing position");
                    
                    ImGui::EndTabItem();
                }
                
                // ================================================================
                // AUTO ROTATE TAB
                // ================================================================
                if (ImGui::BeginTabItem("Auto Rotate")) {
                    ImGui::Spacing();
                    ImGui::Text("Status: ");
                    ImGui::SameLine();
                    if (g_autorotate_active) {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[ACTIVE]");
                    } else {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[IDLE]");
                    }
                    
                    ImGui::Spacing();
                    ImGui::TextWrapped("Presses 'Z', clicks at rotate position, then presses 'C' to activate healing. Repeats every 0.125 seconds.");
                    ImGui::Spacing();
                    
                    ImGui::Text("Rotate Position");
                    ImGui::Spacing();
                    
                    int old_rot_x = g_config.autorotate_target_x;
                    int old_rot_y = g_config.autorotate_target_y;
                    
                    {
                        std::lock_guard<std::mutex> lock(g_config_mutex);
                        ImGui::InputInt("X Position##rot", &g_config.autorotate_target_x);
                        ImGui::InputInt("Y Position##rot", &g_config.autorotate_target_y);
                    }
                    
                    if (old_rot_x != g_config.autorotate_target_x || 
                        old_rot_y != g_config.autorotate_target_y) {
                        SaveConfig();
                    }
                    
                    ImGui::Spacing();
                    
                    POINT cursorPos2;
                    GetCursorPos(&cursorPos2);
                    ImGui::Text("Current mouse: (%d, %d)", cursorPos2.x, cursorPos2.y);
                    
                    if (ImGui::Button("Set to Current Position##rot")) {
                        std::lock_guard<std::mutex> lock(g_config_mutex);
                        g_config.autorotate_target_x = cursorPos2.x;
                        g_config.autorotate_target_y = cursorPos2.y;
                        SaveConfig();
                    }
                    
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "Note: Uses Healing position as second point");
                    ImGui::Spacing();
                    
                    char rot_hotkey_text[128];
                    sprintf_s(rot_hotkey_text, "Toggle: %s", GetKeyName(g_config.autorotate_hotkey));
                    if (ImGui::Button(rot_hotkey_text, ImVec2(200, 0))) {
                        std::lock_guard<std::mutex> lock(g_config_mutex);
                        g_config.capturing_hotkey = true;
                        g_config.capturing_hotkey_for = 2;
                        g_config.capture_wait_frames = 0;
                    }
                    
                    ImGui::SameLine();
                    if (ImGui::Button("Test Now##rot", ImVec2(120, 0))) {
                        g_autorotate_active = !g_autorotate_active;
                    }
                    
                    if (g_config.capturing_hotkey && g_config.capturing_hotkey_for == 2) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.00f), "Press any key...");
                        
                        if (g_config.capture_wait_frames < 10) {
                            std::lock_guard<std::mutex> lock(g_config_mutex);
                            g_config.capture_wait_frames++;
                        } else {
                            int vk = CaptureVirtualKey();
                            if (vk != 0) {
                                std::lock_guard<std::mutex> lock(g_config_mutex);
                                g_config.autorotate_hotkey = vk;
                                g_config.capturing_hotkey = false;
                                g_config.capturing_hotkey_for = 0;
                                g_config.capture_wait_frames = 0;
                                g_config.hotkey_cooldown_frames = 60;
                                SaveConfig();
                            }
                        }
                    }
                    
                    ImGui::EndTabItem();
                }
                
                ImGui::EndTabBar();
            }
            
            ImGui::End();
        }

        // Render
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 
            clear_color.x, clear_color.y, clear_color.z, clear_color.w 
        };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        
        g_pSwapChain->Present(1, 0);
        
        if (!show_app) break;
    }
    
    g_running = false;
    g_autoclicker_active = false;
    g_arduino_clicking = false;
    g_mouselock_active = false;
    g_autobolt_active = false;
    g_autobolt_executing = false;
    g_autorotate_active = false;
    g_autorotate_executing = false;
    
    SendCommand("STOP");
    SaveConfig();
    
    if (arduino_thread.joinable()) arduino_thread.join();
    if (sw_autoclicker_thread.joinable()) sw_autoclicker_thread.join();
    if (mouselock_thread.joinable()) mouselock_thread.join();
    if (autobolt_thread.joinable()) autobolt_thread.join();
    if (autorotate_thread.joinable()) autorotate_thread.join();
    if (hotkey_thread.joinable()) hotkey_thread.join();
    
    CloseSerialPort();
    
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    
    timeEndPeriod(1);
    
    return 0;
}

// D3D Helper Functions
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    
    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext
    );
    
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
            &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext
        );
    
    if (res != S_OK)
        return false;
    
    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    
    if (FAILED(hr) || pBackBuffer == nullptr) {
        return;
    }
    
    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
    
    if (FAILED(hr)) {
        g_mainRenderTargetView = nullptr;
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { 
        g_mainRenderTargetView->Release(); 
        g_mainRenderTargetView = nullptr; 
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    
    switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED)
                return 0;
            if (g_pd3dDevice != nullptr) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}