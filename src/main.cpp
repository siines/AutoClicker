#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Add Common Controls v6 manifest
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windowsx.h> // For GET_X_LPARAM, etc.
#include <commctrl.h> // For SetWindowSubclass
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <map> // For VK code to string mapping
#include <sstream> // For formatting hotkey string
#include <future> // For std::async
#include <algorithm> // For std::min and std::max
#include <cmath> // For animation easing functions
#include <mutex>
#include <fstream> // For file I/O operations
#include <direct.h> // For directory operations
#include <ShlObj.h> // For getting special folder paths
#include <shellapi.h> // For Shell_NotifyIcon

// Include resource header
#include "resource.h"



#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// Global Variables
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static HWND                     g_hwnd = nullptr;
static int                      g_windowWidth = 400;
static int                      g_windowHeight = 400;

// Application State (Thread-Safe)
std::atomic<bool> g_running = false;
std::atomic<int>  g_clickInterval = 100; // Milliseconds
std::atomic<bool> g_lockCursor = false;
std::atomic<int>  g_toggleHotkey = VK_F6; // Default hotkey F6
std::atomic<bool> g_appShouldClose = false;
std::atomic<bool> g_immediateQuit = false; // Flag for immediate quit
std::atomic<bool> g_isSettingHotkey = false;
std::atomic<bool> g_isSettingMouseButton = false;
POINT             g_lockedCursorPos = {0, 0}; // Cursor lock position
std::thread*      g_pClickerThread = nullptr; // Thread pointer for better control
std::atomic<bool> g_startWithWindows = false;
std::atomic<bool> g_startMinimized = false;
std::atomic<bool> g_startToTray = false;
std::atomic<bool> g_minimizeOnClose = false; // New global variable

// System Tray Icon definitions
#define WM_TRAYICON (WM_USER + 1)
NOTIFYICONDATA g_nid = {}; // Tray icon data structure
bool g_isMinimizedToTray = false; // Track if currently minimized to tray

// Old color themes removed - will be set directly in style

// Legacy variables (some may be used in UI rendering)
bool waitingForKey = false;
char keyName[32] = "F6";
float clickFeedbackTimer = 0.0f;
float clickFeedbackDuration = 0.5f;
bool isPositionLocked = false;
std::atomic<bool> clickerActive = false;
std::mutex mtx;
POINT currentCursorPos = { 0, 0 };
int toggleKey = VK_F6;

// Timestamps to prevent mouse click detection as keypress
std::atomic<DWORD> g_hotkeyButtonPressTime = 0;
std::atomic<DWORD> g_keyboardKeyButtonPressTime = 0;

// Add these variables to the global variables section
float g_hotkeyReadyProgress = 0.0f;
float g_keyboardKeyReadyProgress = 0.0f;
bool g_hotkeyReadyToListen = false;
bool g_keyboardKeyReadyToListen = false;

// Add these variables to the global variables section after the existing ready state variables
std::atomic<DWORD> g_lastHotkeySetTime = 0;
std::atomic<DWORD> g_lastKeyboardKeySetTime = 0;
const DWORD KEY_ACTIVATION_COOLDOWN = 500; // ms to prevent activation after setting

// Mouse button selection
enum MouseButton {
    LEFT_BUTTON = 0,
    RIGHT_BUTTON = 1,
    MIDDLE_BUTTON = 2
};

MouseButton selectedMouseButton = LEFT_BUTTON;
const char* mouseButtonNames[] = { "Left Button", "Right Button", "Middle Button" };

// Keyboard input support
enum InputType {
    MOUSE_INPUT = 0,
    KEYBOARD_INPUT = 1
};

InputType selectedInputType = MOUSE_INPUT;
const char* inputTypeNames[] = { "Mouse", "Keyboard" };

// Key to send when in keyboard mode
int selectedKeyboardKey = 'A';
char keyboardKeyName[32] = "A";
bool waitingForKeyboardKey = false;

// Settings file structure and functions
struct AppSettings {
    int clickInterval = 100;
    bool lockCursor = false;
    int toggleHotkey = VK_F6;
    int mouseButton = 0; // 0: Left, 1: Right, 2: Middle
    int inputType = 0;   // 0: Mouse, 1: Keyboard
    int keyboardKey = 'A';
    bool startWithWindows = false;
    bool startMinimized = false;
    bool startToTray = false; // Only relevant if startMinimized is true
    bool minimizeOnClose = false; // New setting
};

// Get AppData folder path for settings
std::string GetSettingsPath() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::string appDataPath(path);
        std::string settingsFolder = appDataPath + "\\AutoClicker";
        
        // Create directory if it doesn't exist
        if (_mkdir(settingsFolder.c_str()) != 0 && errno != EEXIST) {
            return ""; // Failed to create directory
        }
        
        return settingsFolder + "\\settings.dat";
    }
    return ""; // Failed to get AppData path
}

// Forward declarations for settings functions
bool SaveSettings();
bool LoadSettings();

// Dragging related variables
static bool g_isDragging = false;
static int g_dragStartX = 0;
static int g_dragStartY = 0;
static int g_dragWindowStartX = 0;
static int g_dragWindowStartY = 0;

// Original window procedure
WNDPROC g_OriginalWndProc = nullptr;

// Forward declaration of ImGui's Win32 handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
std::string VKCodeToString(int vkCode);
void GetKeyName(int keyCode, char* outName, size_t nameSize);
void AutoClickerThread();
void RenderUIWindow();
static void HelpMarker(const char* desc);
bool SetStartupRegistry(bool enable); // Add declaration
void AddTrayIcon(HWND hwnd); // Add declaration
void RemoveTrayIcon(HWND hwnd); // Add declaration
bool IsRunningAsAdmin(); // Add declaration

// Helper function for color interpolation
inline ImVec4 LerpImVec4(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}



// Helper function to convert VK code to string
std::string VKCodeToString(int vkCode) {
    static std::map<int, std::string> vkMap = {
        {VK_F1, "F1"}, {VK_F2, "F2"}, {VK_F3, "F3"}, {VK_F4, "F4"},
        {VK_F5, "F5"}, {VK_F6, "F6"}, {VK_F7, "F7"}, {VK_F8, "F8"},
        {VK_F9, "F9"}, {VK_F10, "F10"}, {VK_F11, "F11"}, {VK_F12, "F12"},
        {VK_LBUTTON, "Left Mouse"}, {VK_RBUTTON, "Right Mouse"}, {VK_MBUTTON, "Middle Mouse"},
        {VK_XBUTTON1, "Mouse 4"}, {VK_XBUTTON2, "Mouse 5"},
        {VK_SHIFT, "Shift"}, {VK_LSHIFT, "Left Shift"}, {VK_RSHIFT, "Right Shift"},
        {VK_CONTROL, "Ctrl"}, {VK_LCONTROL, "Left Ctrl"}, {VK_RCONTROL, "Right Ctrl"},
        {VK_MENU, "Alt"}, {VK_LMENU, "Left Alt"}, {VK_RMENU, "Right Alt"},
        {VK_CAPITAL, "Caps Lock"}, {VK_NUMLOCK, "Num Lock"}, {VK_SCROLL, "Scroll Lock"},
        {VK_OEM_3, "~ `"}, {VK_OEM_PLUS, "= +"}, {VK_OEM_COMMA, ", <"}, {VK_OEM_MINUS, "- _"}, {VK_OEM_PERIOD, ". >"},
        {VK_OEM_1, "; :"}, {VK_OEM_2, "/ ?"}, {VK_OEM_4, "[ {"}, {VK_OEM_5, "\\ |"}, {VK_OEM_6, "] }"}, {VK_OEM_7, "' \""},
        {VK_SPACE, "Space"}, {VK_RETURN, "Enter"}, {VK_BACK, "Backspace"}, {VK_TAB, "Tab"}, {VK_ESCAPE, "Escape"},
        {VK_INSERT, "Insert"}, {VK_DELETE, "Delete"}, {VK_HOME, "Home"}, {VK_END, "End"}, {VK_PRIOR, "Page Up"}, {VK_NEXT, "Page Down"},
        {VK_UP, "Up Arrow"}, {VK_DOWN, "Down Arrow"}, {VK_LEFT, "Left Arrow"}, {VK_RIGHT, "Right Arrow"}
    };
    if (vkMap.count(vkCode)) return vkMap[vkCode];
    if ((vkCode >= 'A' && vkCode <= 'Z') || (vkCode >= '0' && vkCode <= '9')) return std::string(1, (char)vkCode);
    UINT scanCode = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
    LONG lParam = scanCode << 16;
    switch (vkCode) {
        case VK_RCONTROL: case VK_RMENU: case VK_INSERT: case VK_DELETE:
        case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
        case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT: case VK_NUMLOCK:
        case VK_DIVIDE: lParam |= (1 << 24); break;
    }
    char keyName[50];
    if (GetKeyNameTextA(lParam, keyName, sizeof(keyName))) return std::string(keyName);
    std::stringstream ss;
    ss << "VK 0x" << std::hex << vkCode;
    return ss.str();
}

// Helper to display a little (?) mark which shows a tooltip when hovered.
static void HelpMarker(const char* desc) {
    // Use a more reliable character combination for the help marker
    ImGui::TextDisabled("[?]");
    if (ImGui::IsItemHovered()) {
        // Calculate tooltip max width based on window width
        float maxWidth = ImGui::GetWindowWidth() * 0.8f;
        
        // Ensure tooltip stays within window
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        
        // If mouse is too close to right edge, force tooltip to the left
        if (mousePos.x > windowPos.x + windowSize.x * 0.7f) {
            ImGui::SetNextWindowPos(ImVec2(mousePos.x - 20, mousePos.y));
        }
        
        ImGui::BeginTooltip(); 
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f < maxWidth ? ImGui::GetFontSize() * 35.0f : maxWidth);
        ImGui::TextUnformatted(desc); 
        ImGui::PopTextWrapPos(); 
        ImGui::EndTooltip();
    }
}

// Save settings to file
bool SaveSettings() {
    std::string settingsPath = GetSettingsPath();
    if (settingsPath.empty()) return false;
    
    AppSettings settings;
    settings.clickInterval = g_clickInterval.load();
    settings.lockCursor = g_lockCursor.load();
    settings.toggleHotkey = g_toggleHotkey.load();
    settings.mouseButton = selectedMouseButton;
    settings.inputType = selectedInputType;
    settings.keyboardKey = selectedKeyboardKey;
    // Update settings struct with current global values before saving
    settings.startWithWindows = g_startWithWindows.load();
    settings.startMinimized = g_startMinimized.load();
    settings.startToTray = g_startToTray.load();
    settings.minimizeOnClose = g_minimizeOnClose.load(); // Save new setting
    
    std::ofstream file(settingsPath, std::ios::binary);
    if (!file.is_open()) return false;
    
    file.write(reinterpret_cast<const char*>(&settings), sizeof(AppSettings));
    return file.good();
}

// Load settings from file
bool LoadSettings() {
    std::string settingsPath = GetSettingsPath();
    if (settingsPath.empty()) return false;
    
    std::ifstream file(settingsPath, std::ios::binary);
    if (!file.is_open()) return false;
    
    AppSettings settings;
    file.read(reinterpret_cast<char*>(&settings), sizeof(AppSettings));
    
    if (file.good() || file.eof()) {
        g_clickInterval.store(settings.clickInterval);
        g_lockCursor.store(settings.lockCursor);
        g_toggleHotkey.store(settings.toggleHotkey);
        selectedMouseButton = static_cast<MouseButton>(settings.mouseButton);
        selectedInputType = static_cast<InputType>(settings.inputType);
        selectedKeyboardKey = settings.keyboardKey;
        // Load startup settings into global variables
        g_startWithWindows.store(settings.startWithWindows);
        g_startMinimized.store(settings.startMinimized);
        g_startToTray.store(settings.startToTray);
        g_minimizeOnClose.store(settings.minimizeOnClose); // Load new setting
        // g_startToTray = settings.startToTray;
        
        // Simple key name conversion without using VKCodeToString
        if (selectedKeyboardKey >= 'A' && selectedKeyboardKey <= 'Z') {
            keyboardKeyName[0] = (char)selectedKeyboardKey;
            keyboardKeyName[1] = '\0';
        } else if (selectedKeyboardKey >= VK_F1 && selectedKeyboardKey <= VK_F12) {
            sprintf_s(keyboardKeyName, sizeof(keyboardKeyName), "F%d", selectedKeyboardKey - VK_F1 + 1);
        } else {
            // Default fallback for other keys
            sprintf_s(keyboardKeyName, sizeof(keyboardKeyName), "Key %d", selectedKeyboardKey);
        }
        
        return true;
    }
    
    return false;
    }
    
    // Function to add/remove the application from Windows startup
    bool SetStartupRegistry(bool enable) {
        HKEY hKey = NULL;
        LONG lResult = RegOpenKeyEx(HKEY_CURRENT_USER,
                                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                    0, KEY_WRITE, &hKey);
    
        if (lResult != ERROR_SUCCESS) {
            // Handle error (e.g., log or display a message)
            return false;
        }
    
        if (enable) {
            wchar_t szPath[MAX_PATH];
            GetModuleFileName(NULL, szPath, MAX_PATH);
    
            lResult = RegSetValueEx(hKey,
                                    L"AutoClicker", // Application name in registry
                                    0, REG_SZ,
                                    (LPBYTE)szPath,
                                    (wcslen(szPath) + 1) * sizeof(wchar_t));
        } else {
            lResult = RegDeleteValue(hKey, L"AutoClicker");
            // Ignore ERROR_FILE_NOT_FOUND if deleting, it means it wasn't there
            if (lResult == ERROR_FILE_NOT_FOUND) {
                lResult = ERROR_SUCCESS;
            }
        }
    
        RegCloseKey(hKey);
    
        if (lResult != ERROR_SUCCESS) {
            // Handle error
            return false;
        }
    
        return true;
        }
        
        // Function to add the icon to the system tray
        void AddTrayIcon(HWND hwnd) {
            ZeroMemory(&g_nid, sizeof(g_nid));
            g_nid.cbSize = sizeof(NOTIFYICONDATA);
            g_nid.hWnd = hwnd;
            g_nid.uID = 1; // Unique ID for the icon
            g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            g_nid.uCallbackMessage = WM_TRAYICON; // Our custom message
            
            // Load the same icon used for the window
            HICON hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
            if (!hIcon) {
                // Fallback if resource loading fails (should match WinMain logic ideally)
                hIcon = (HICON)LoadImage(NULL, _T("cursor_208x256_1jZ_icon.ico"), IMAGE_ICON, 16, 16, LR_LOADFROMFILE | LR_DEFAULTCOLOR);
            }
            g_nid.hIcon = hIcon;
            
            // Tooltip text
            lstrcpy(g_nid.szTip, _T("AutoClicker"));
        
            // Add the icon
            Shell_NotifyIcon(NIM_ADD, &g_nid);
        
            // Clean up loaded icon if necessary
            if (hIcon && g_nid.hIcon != hIcon) { // Only destroy if LoadImage created a new one
                DestroyIcon(hIcon);
            }
            g_isMinimizedToTray = true; // Mark as minimized to tray
        }
        
        // Function to remove the icon from the system tray
        void RemoveTrayIcon(HWND hwnd) {
            g_nid.hWnd = hwnd; // Ensure hWnd is correct
            g_nid.uID = 1;     // Ensure ID is correct
            Shell_NotifyIcon(NIM_DELETE, &g_nid);
            g_isMinimizedToTray = false; // Mark as not minimized to tray
        }

        // Check if the current process is running with administrator privileges
        bool IsRunningAsAdmin() {
            BOOL isAdmin = FALSE;
            PSID adminGroup = NULL;

            // Create a well-known SID for the Built-in Administrators group
            SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
            if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, 
                                       DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
                // Check if the current token contains the administrators group
                CheckTokenMembership(NULL, adminGroup, &isAdmin);
                FreeSid(adminGroup);
            }

            return isAdmin == TRUE;
        }

// Autoclicker Thread Logic 
void AutoClickerThread() {
    bool hotkeyPressedLast = false;
    bool settingHotkeyLast = false;
    const int BASE_CHECK_INTERVAL = 10; // Check interval for responsiveness
    
    while (!g_appShouldClose && !g_immediateQuit) {
        // Check for immediate quit request
        if (g_immediateQuit.load()) {
            break;
        }
        
        // Hotkey toggle logic
        int currentHotkey = g_toggleHotkey.load();
        bool hotkeyCurrentlyPressed = (GetAsyncKeyState(currentHotkey) & 0x8000) != 0;
        
        // Prevent activation during hotkey setting or during cooldown period after setting
        bool inHotkeyCooldown = (GetTickCount() - g_lastHotkeySetTime < KEY_ACTIVATION_COOLDOWN);
        
        if (hotkeyCurrentlyPressed && !hotkeyPressedLast && !g_isSettingHotkey.load() && !inHotkeyCooldown) {
            g_running = !g_running;
            
            // Capture cursor position when starting
            if (g_running && g_lockCursor.load()) {
                GetCursorPos(&g_lockedCursorPos);
            }
            
            // Small delay to prevent key bounce
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        hotkeyPressedLast = hotkeyCurrentlyPressed;
        
        // Hotkey setting logic
        if (g_isSettingHotkey.load()) {
            if (!settingHotkeyLast) {
                // Just started setting hotkey
                settingHotkeyLast = true;
                
                // Instead of sleeping, we'll check the time elapsed since button press
                // We'll continue scanning for keypresses regardless, but ignore any within the threshold period
            }
            
            // Check if we should ignore keypresses due to the recent button click
            bool ignoreKeyPresses = !g_hotkeyReadyToListen;
            
            // Scan for keypresses - skip modifier keys
            for (int vk = 8; vk < 255; ++vk) {
                // Skip modifier keys
                if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || 
                    vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL || 
                    vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU || 
                    vk == VK_LWIN || vk == VK_RWIN || vk == VK_APPS) {
                    continue;
                }
                
                // Also skip left mouse button which might register as 'P'
                if (vk == VK_LBUTTON) {
                    continue;
                }
                
                // Check if key is pressed, ignoring during the threshold period after button click
                if (!ignoreKeyPresses && (GetAsyncKeyState(vk) & 0x8000)) {
                    // Don't set if ESC (used to cancel)
                    if (vk != VK_ESCAPE) {
                        g_toggleHotkey = vk;
                        // Record when the key was set
                        g_lastHotkeySetTime = GetTickCount();
                        SaveSettings(); // Save settings when hotkey changes
                    }
                    g_isSettingHotkey = false;
                    break;
                }
            }
            
            // Allow ESC to cancel without setting
            if (!ignoreKeyPresses && (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
                g_isSettingHotkey = false;
            }
        } else {
            settingHotkeyLast = false;
        }

        // Mouse button setting logic
        static bool settingMouseButtonLast = false;
        if (g_isSettingMouseButton.load()) {
            if (!settingMouseButtonLast) {
                // Just started setting mouse button
                settingMouseButtonLast = true;
            }
            
            // Check for mouse button presses
            for (int mb : {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2}) {
                if (GetAsyncKeyState(mb) & 0x8000) {
                    g_isSettingMouseButton = false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(250)); 
                    break;
                }
            }
            
            // Allow ESC to cancel without setting
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                g_isSettingMouseButton = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        } else {
            settingMouseButtonLast = false;
        }
        
        // Clicking logic when running
        if (g_running.load()) {
            POINT currentCursorPos; 
            RECT windowRect; 
            bool cursorOverWindow = false;
            
            GetCursorPos(&currentCursorPos);
            
            // Check if cursor is over app window
            if (g_hwnd) { 
                GetWindowRect(g_hwnd, &windowRect); 
                if (PtInRect(&windowRect, currentCursorPos)) 
                    cursorOverWindow = true; 
            }
            
            // Lock cursor if enabled
            if (g_lockCursor.load() && !cursorOverWindow) {
                if (currentCursorPos.x != g_lockedCursorPos.x || 
                    currentCursorPos.y != g_lockedCursorPos.y) {
                    SetCursorPos(g_lockedCursorPos.x, g_lockedCursorPos.y);
                }
            } 
            
            // Perform click if not over our window
            if (!cursorOverWindow) {
                POINT clickPos = g_lockCursor.load() ? g_lockedCursorPos : currentCursorPos;
                
                if (selectedInputType == MOUSE_INPUT) {
                    // Send mouse click events
                    INPUT input[2] = {};
                    input[0].type = INPUT_MOUSE;
                    input[1].type = INPUT_MOUSE;
                    
                    // Use selectedMouseButton instead of g_mouseButton
                    DWORD buttonDown = 0;
                    DWORD buttonUp = 0;
                    
                    switch (selectedMouseButton) {
                        case RIGHT_BUTTON:
                            buttonDown = MOUSEEVENTF_RIGHTDOWN;
                            buttonUp = MOUSEEVENTF_RIGHTUP;
                            break;
                        case MIDDLE_BUTTON:
                            buttonDown = MOUSEEVENTF_MIDDLEDOWN;
                            buttonUp = MOUSEEVENTF_MIDDLEUP;
                            break;
                        case LEFT_BUTTON:
                        default:
                            buttonDown = MOUSEEVENTF_LEFTDOWN;
                            buttonUp = MOUSEEVENTF_LEFTUP;
                            break;
                    }
                    
                    // Set the flags and send the input
                    input[0].mi.dwFlags = buttonDown;
                    input[1].mi.dwFlags = buttonUp;
                    
                    SendInput(2, input, sizeof(INPUT));
                }
                else if (selectedInputType == KEYBOARD_INPUT) {
                    // Enhanced keyboard input with scan codes for better compatibility
                    INPUT input[2] = {};
                    input[0].type = INPUT_KEYBOARD;
                    input[1].type = INPUT_KEYBOARD;
                    
                    // Get the scan code for the virtual key
                    UINT scanCode = MapVirtualKey(selectedKeyboardKey, MAPVK_VK_TO_VSC);
                    
                    // Key down
                    input[0].ki.wVk = selectedKeyboardKey;
                    input[0].ki.wScan = scanCode;
                    input[0].ki.dwFlags = 0;
                    
                    // Check if this is an extended key that needs the extended flag
                    switch (selectedKeyboardKey) {
                        case VK_RIGHT: case VK_LEFT: case VK_UP: case VK_DOWN:
                        case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
                        case VK_INSERT: case VK_DELETE:
                        case VK_DIVIDE: case VK_NUMLOCK:
                        case VK_RCONTROL: case VK_RMENU:
                            input[0].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
                            break;
                    }
                    
                    // Key up
                    input[1].ki.wVk = selectedKeyboardKey;
                    input[1].ki.wScan = scanCode;
                    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
                    
                    // Add extended key flag for key up as well if needed
                    if (input[0].ki.dwFlags & KEYEVENTF_EXTENDEDKEY) {
                        input[1].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
                    }
                    
                    // Send the key down event first
                    SendInput(1, &input[0], sizeof(INPUT));
                    
                    // Small delay between key down and key up for better compatibility
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    
                    // Send the key up event
                    SendInput(1, &input[1], sizeof(INPUT));
                }
                
                // Update click feedback timer
                clickFeedbackTimer = clickFeedbackDuration;
            }
            
            // Get sleep time from the configured interval
            int sleepTimeMs = g_clickInterval.load();
            int remainingTime = sleepTimeMs;
            
            // Wait between clicks (with responsiveness)
            while (remainingTime > 0 && g_running.load() && !g_immediateQuit.load()) {
                int waitTime = (remainingTime < BASE_CHECK_INTERVAL) ? remainingTime : BASE_CHECK_INTERVAL;
                std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
                remainingTime -= waitTime;
                
                // Check for hotkey during wait period
                bool hotkeyPressed = (GetAsyncKeyState(currentHotkey) & 0x8000) != 0;
                if (hotkeyPressed && !hotkeyPressedLast) {
                    g_running = false;
                    hotkeyPressedLast = true;
                    break;
                }
                hotkeyPressedLast = hotkeyPressed;
                
                // Re-enforce cursor position if locked and needed
                if (g_lockCursor.load()) {
                    GetCursorPos(&currentCursorPos);
                    if (currentCursorPos.x != g_lockedCursorPos.x || 
                        currentCursorPos.y != g_lockedCursorPos.y) {
                        
                        GetWindowRect(g_hwnd, &windowRect);
                        if (!PtInRect(&windowRect, currentCursorPos)) {
                            SetCursorPos(g_lockedCursorPos.x, g_lockedCursorPos.y);
                        }
                    }
                }
            }
        } else {
            // Not running, just check periodically
            std::this_thread::sleep_for(std::chrono::milliseconds(BASE_CHECK_INTERVAL));
        }
    }
}

// Updated UI to reflect simplified cursor lock system
void RenderUIWindow() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 windowPos = viewport->WorkPos;
    ImVec2 windowSize = viewport->WorkSize;
    float titleBarHeight = 30.0f;

    // Title Bar Window
    ImGui::SetNextWindowPos(windowPos);
    ImGui::SetNextWindowSize(ImVec2(windowSize.x, titleBarHeight));
    ImGui::SetNextWindowBgAlpha(1.0f);

    ImGuiWindowFlags titleBarFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive)); // Use theme's TitleBg color

    ImGui::Begin("CustomTitleBar", nullptr, titleBarFlags);
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // Title Text
    ImGui::SetCursorPosX(10);
    ImGui::SetCursorPosY((titleBarHeight - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::Text("AutoClicker");

    // Window Control Buttons
    float buttonHeight = titleBarHeight - 8;
    float buttonsTotalWidth = (45 * 3) + (ImGui::GetStyle().ItemSpacing.x * 2);
    ImGui::SameLine(ImGui::GetWindowWidth() - buttonsTotalWidth - 5);
    ImGui::SetCursorPosY((titleBarHeight - buttonHeight) * 0.5f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
    // Use transparent background for buttons, consistent hover/active from theme
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered)); // Subtle gray hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));   // Subtle gray active
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text)); // Use theme's text color

    // Minimize Button
    if (ImGui::Button("-", ImVec2(45, buttonHeight))) { ShowWindow(g_hwnd, SW_MINIMIZE); }
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);

    // Maximize Button
    WINDOWPLACEMENT wp; wp.length = sizeof(WINDOWPLACEMENT); GetWindowPlacement(g_hwnd, &wp);
    bool isMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
    const char* maxChar = isMaximized ? "=" : "[]";
    if (ImGui::Button(maxChar, ImVec2(45, buttonHeight))) { ShowWindow(g_hwnd, isMaximized ? SW_RESTORE : SW_MAXIMIZE); }
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);

    // Close Button
    // Use a distinct but theme-appropriate red for close button hover/active
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f)); // Slightly desaturated red hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.2f, 0.2f, 1.0f)); // Slightly darker red active
    if (ImGui::Button("X", ImVec2(45, buttonHeight))) { SendMessage(g_hwnd, WM_SYSCOMMAND, SC_CLOSE, 0); } // Send SC_CLOSE to trigger WndProc logic
    ImGui::PopStyleColor(2);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(3);

    ImGui::End(); // End Title Bar

    // Main Content Window
    ImVec2 contentPos = ImVec2(windowPos.x, windowPos.y + titleBarHeight);
    ImVec2 contentSize = ImVec2(windowSize.x, windowSize.y - titleBarHeight);

    ImGui::SetNextWindowPos(contentPos);
    ImGui::SetNextWindowSize(contentSize);

    ImGuiWindowFlags contentFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.0f, 15.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("MainContent", nullptr, contentFlags);
    ImGui::PopStyleVar(2);

    // Status Section
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));

    // Status indicator with no question mark
    ImVec4 statusColor = g_running.load() ? ImVec4(0.1f, 0.8f, 0.1f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    
    // Header with status
    ImGui::Text("Status:");
    ImGui::SameLine();
    
    std::string statusInfo;
    if (g_running.load()) {
        if (selectedInputType == MOUSE_INPUT) {
            statusInfo = "ACTIVE (Clicking with " + std::string(mouseButtonNames[selectedMouseButton]) + ")";
        } else {
            statusInfo = "ACTIVE (Pressing key " + std::string(keyboardKeyName) + ")";
        }
    } else {
        statusInfo = "STOPPED";
    }
    ImGui::TextColored(g_running.load() ? ImVec4(0.1f, 1.0f, 0.1f, 1.0f) : ImVec4(0.9f, 0.9f, 0.9f, 0.7f), 
                     "%s", statusInfo.c_str());

    // Add visual cooldown indicator in the status section if needed
    DWORD timeSinceHotkeySet = GetTickCount() - g_lastHotkeySetTime;
    if (timeSinceHotkeySet < KEY_ACTIVATION_COOLDOWN) {
        ImGui::Spacing();
        // More subtle progress bar
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.4f, 0.2f, 0.5f));
        
        // Calculate progress as a percentage (from 100% down to 0%)
        float cooldownProgress = 1.0f - ((float)timeSinceHotkeySet / KEY_ACTIVATION_COOLDOWN);
        
        // More subtle progress bar
        ImGui::ProgressBar(cooldownProgress, ImVec2(-1, 5), "");
        ImGui::TextDisabled("Hotkey available in %.1f sec", cooldownProgress * (KEY_ACTIVATION_COOLDOWN/1000.0f));
        
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Use Tab Bar for settings organization
    if (ImGui::BeginTabBar("SettingsTabs")) {

        // --- Clicking Tab ---
        if (ImGui::BeginTabItem("Clicking")) {
            ImGui::Spacing();

            // Click Interval
            int currentInterval = g_clickInterval.load();
            ImGui::SetNextItemWidth(100);
            if (ImGui::InputInt("Interval (ms)", &currentInterval, 10, 50)) {
                if (currentInterval < 10) currentInterval = 10;
                g_clickInterval.store(currentInterval);
                SaveSettings();
            }
            ImGui::SameLine(); HelpMarker("Delay between clicks/key presses (minimum 10ms)");
            ImGui::Spacing();

            // Input Type Selection
            ImGui::Text("Input Type:");
            ImGui::SameLine();
            if (ImGui::BeginCombo("##InputType", inputTypeNames[selectedInputType])) {
                for (int i = 0; i < 2; i++) {
                    bool isSelected = (selectedInputType == i);
                    if (ImGui::Selectable(inputTypeNames[i], isSelected)) {
                        selectedInputType = (InputType)i;
                        SaveSettings();
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine(); HelpMarker("Select whether to simulate mouse clicks or keyboard presses");
            ImGui::Spacing();

            // Mouse Button Selection (conditional)
            if (selectedInputType == MOUSE_INPUT) {
                ImGui::Text("Mouse Button:");
                ImGui::SameLine();
                if (ImGui::BeginCombo("##MouseButton", mouseButtonNames[selectedMouseButton])) {
                    for (int i = 0; i < 3; i++) {
                        bool isSelected = (selectedMouseButton == i);
                        if (ImGui::Selectable(mouseButtonNames[i], isSelected)) {
                            selectedMouseButton = (MouseButton)i;
                            SaveSettings();
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine(); HelpMarker("Select which mouse button to auto-click");
                ImGui::Spacing();
            }

            // Keyboard Key Selection (conditional)
            if (selectedInputType == KEYBOARD_INPUT) {
                ImGui::Text("Keyboard Key:");
                ImGui::SameLine();
                
                // Enhanced help marker with admin warning
                if (!IsRunningAsAdmin()) {
                    HelpMarker("For best compatibility with all applications (especially games), run AutoClicker as Administrator.\n\nWARNING: Not currently running as Administrator - keyboard input may not work with all applications, especially games and secure software.");
                } else {
                    HelpMarker("Running as Administrator - keyboard input should work with all applications including games.");
                }
                
                if (waitingForKeyboardKey) {
                    // (Keep the 'waiting for key' UI logic as it was)
                    DWORD elapsedTime = GetTickCount() - g_keyboardKeyButtonPressTime;
                    const DWORD transitionTime = 200;
                    if (elapsedTime < transitionTime) {
                        g_keyboardKeyReadyProgress = (float)elapsedTime / transitionTime;
                        g_keyboardKeyReadyToListen = false;
                        ImVec4 transitionColor = LerpImVec4(ImVec4(0.3f, 0.2f, 0.2f, 1.0f), ImVec4(0.2f, 0.3f, 0.2f, 1.0f), g_keyboardKeyReadyProgress);
                        ImGui::PushStyleColor(ImGuiCol_Button, transitionColor);
                        ImGui::Button("Initializing...", ImVec2(150, 0));
                        ImGui::PopStyleColor();
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));
                        ImGui::ProgressBar(g_keyboardKeyReadyProgress, ImVec2(50, 10), "");
                        ImGui::PopStyleColor();
                    } else {
                        g_keyboardKeyReadyProgress = 1.0f;
                        g_keyboardKeyReadyToListen = true;
                        float pulse = 0.2f + 0.1f * sinf(ImGui::GetTime() * 4.0f);
                        ImVec4 pulseColor = ImVec4(0.2f, 0.3f + pulse, 0.2f, 1.0f);
                        ImGui::PushStyleColor(ImGuiCol_Button, pulseColor);
                        ImGui::Button("Press any key", ImVec2(150, 0));
                        ImGui::PopStyleColor();
                        ImGui::SameLine(); ImGui::TextDisabled("ESC to cancel");
                    }
                } else {
                    if (ImGui::Button(keyboardKeyName, ImVec2(100, 0))) {
                        g_keyboardKeyButtonPressTime = GetTickCount();
                        g_keyboardKeyReadyProgress = 0.0f;
                        g_keyboardKeyReadyToListen = false;
                        waitingForKeyboardKey = true;
                    }
                    DWORD timeSinceKeySet = GetTickCount() - g_lastKeyboardKeySetTime;
                    if (timeSinceKeySet < KEY_ACTIVATION_COOLDOWN) {
                        ImGui::SameLine(); ImGui::TextColored(ImVec4(0.2f, 0.5f, 0.2f, 0.7f), "Key set");
                    } else {
                        ImGui::SameLine(); HelpMarker("Click to change the keyboard key to simulate");
                    }
                }
                ImGui::Spacing();
            }

            // Lock Cursor Option
            bool lockCursor = g_lockCursor.load();
            if (ImGui::Checkbox("Lock Cursor Position", &lockCursor)) {
                g_lockCursor.store(lockCursor);
                if (lockCursor && g_running.load()) GetCursorPos(&g_lockedCursorPos);
                SaveSettings();
            }
            ImGui::SameLine(); HelpMarker("Locks cursor at its current position when clicking starts");

            ImGui::EndTabItem();
        }

        // --- Hotkeys Tab ---
        if (ImGui::BeginTabItem("Hotkeys")) {
            ImGui::Spacing();

            // Toggle Hotkey
            ImGui::Text("Toggle Key:");
            ImGui::SameLine();
            std::string hotkey = VKCodeToString(g_toggleHotkey.load());
            if (g_isSettingHotkey.load()) {
                 // (Keep the 'waiting for key' UI logic as it was)
                DWORD elapsedTime = GetTickCount() - g_hotkeyButtonPressTime;
                const DWORD transitionTime = 200;
                if (elapsedTime < transitionTime) {
                    g_hotkeyReadyProgress = (float)elapsedTime / transitionTime;
                    g_hotkeyReadyToListen = false;
                    ImVec4 transitionColor = LerpImVec4(ImVec4(0.3f, 0.2f, 0.2f, 1.0f), ImVec4(0.2f, 0.3f, 0.2f, 1.0f), g_hotkeyReadyProgress);
                    ImGui::PushStyleColor(ImGuiCol_Button, transitionColor);
                    ImGui::Button("Initializing...", ImVec2(150, 0));
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));
                    ImGui::ProgressBar(g_hotkeyReadyProgress, ImVec2(50, 10), "");
                    ImGui::PopStyleColor();
                } else {
                    g_hotkeyReadyProgress = 1.0f;
                    g_hotkeyReadyToListen = true;
                    float pulse = 0.2f + 0.1f * sinf(ImGui::GetTime() * 4.0f);
                    ImVec4 pulseColor = ImVec4(0.2f, 0.3f + pulse, 0.2f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, pulseColor);
                    ImGui::Button("Press new key", ImVec2(150, 0));
                    ImGui::PopStyleColor();
                    ImGui::SameLine(); ImGui::TextDisabled("ESC to cancel");
                }
            } else {
                if (ImGui::Button(hotkey.c_str(), ImVec2(100, 0))) {
                    g_hotkeyButtonPressTime = GetTickCount();
                    g_hotkeyReadyProgress = 0.0f;
                    g_hotkeyReadyToListen = false;
                    g_isSettingHotkey = true;
                }
                DWORD timeSinceKeySet = GetTickCount() - g_lastHotkeySetTime;
                if (timeSinceKeySet < KEY_ACTIVATION_COOLDOWN) {
                    ImGui::SameLine(); ImGui::TextColored(ImVec4(0.2f, 0.5f, 0.2f, 0.7f), "Key set");
                } else {
                    ImGui::SameLine(); HelpMarker("Click to change the start/stop hotkey");
                }
            }

            ImGui::EndTabItem();
        }

        // --- Startup & Behavior Tab ---
        if (ImGui::BeginTabItem("Startup & Behavior")) {
            ImGui::Spacing();

            // Start with Windows
            bool startWin = g_startWithWindows.load();
            if (ImGui::Checkbox("Start with Windows", &startWin)) {
                g_startWithWindows.store(startWin);
                SetStartupRegistry(startWin);
                SaveSettings();
            }
            ImGui::SameLine(); HelpMarker("Automatically launch AutoClicker when Windows starts.");
            ImGui::Spacing();

            // Start Minimized
            bool startMin = g_startMinimized.load();
            if (ImGui::Checkbox("Start Minimized", &startMin)) {
                g_startMinimized.store(startMin);
                if (!startMin) g_startToTray.store(false); // Also disable tray if minimizing is disabled
                SaveSettings();
            }
            ImGui::SameLine(); HelpMarker("Start the application minimized to the taskbar or system tray.");
            ImGui::Spacing();

            // Start Minimized to System Tray (conditional)
            bool startTray = g_startToTray.load();
            bool startMinimizedEnabled = g_startMinimized.load();
            if (!startMinimizedEnabled) ImGui::BeginDisabled();
            if (ImGui::Checkbox("Start Minimized to System Tray", &startTray)) {
                if (startMinimizedEnabled) {
                    g_startToTray.store(startTray);
                    SaveSettings();
                }
            }
            if (!startMinimizedEnabled) ImGui::EndDisabled();
            ImGui::SameLine(); HelpMarker("Requires 'Start Minimized'. Hides the taskbar icon and minimizes to the system tray icon instead.");
            ImGui::Spacing();

            // Minimize to Tray on Close (New)
            bool minOnClose = g_minimizeOnClose.load();
            if (ImGui::Checkbox("Minimize to Tray on Close", &minOnClose)) {
                g_minimizeOnClose.store(minOnClose);
                SaveSettings();
            }
            ImGui::SameLine(); HelpMarker("Instead of quitting, minimize to the system tray when closing the window (using the 'X' button or Alt+F4).");

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Spacing(); // Add space before action buttons

    // Action buttons - Use default theme colors now
    
    if (ImGui::Button(g_running.load() ? "Stop" : "Start", ImVec2(120, 30))) {
        g_running = !g_running;
        
        // Capture cursor position when starting with lock enabled
        if (g_running && g_lockCursor.load()) {
            GetCursorPos(&g_lockedCursorPos);
        }
    }
    // ImGui::PopStyleColor(3); // Removed to use theme colors
    
    ImGui::SameLine();

    // Quit button - Use default theme colors now
    // Add a slight visual distinction for Quit if desired, e.g., by changing text color,
    // but for now, let it use the standard button style.
    
    if (ImGui::Button("Quit", ImVec2(120, 30))) { SendMessage(g_hwnd, WM_SYSCOMMAND, SC_CLOSE, 0); } // Send SC_CLOSE to trigger WndProc logic
    // ImGui::PopStyleColor(3); // Already removed in previous attempt, ensuring it's gone

    ImGui::PopStyleVar();
    ImGui::End(); // End Main Content
}

// Main code
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Create application window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, 
                      NULL, NULL, NULL, _T("AutoClicker"), NULL };
    
    // Load the application icon with LoadImage for better control
    HICON hIconBig = (HICON)LoadImage(
        wc.hInstance,
        MAKEINTRESOURCE(IDI_ICON1),
        IMAGE_ICON,
        32, 32,  // Standard size for hIcon
        LR_DEFAULTCOLOR
    );
    
    HICON hIconSmall = (HICON)LoadImage(
        wc.hInstance,
        MAKEINTRESOURCE(IDI_ICON1),
        IMAGE_ICON,
        16, 16,  // Standard size for hIconSm
        LR_DEFAULTCOLOR
    );
    
    if (hIconBig && hIconSmall) {
        wc.hIcon = hIconBig;
        wc.hIconSm = hIconSmall;
    } else {
        DWORD error = GetLastError();
        TCHAR errorMsg[256];
        _stprintf_s(errorMsg, 256, _T("Failed to load icon. Error code: %d"), error);
        MessageBox(NULL, errorMsg, _T("Icon Error"), MB_OK | MB_ICONERROR);
        
        // Try loading from file directly as a fallback
        // Try multiple possible locations
        const TCHAR* iconPaths[] = {
            _T("cursor_208x256_1jZ_icon.ico"),               // Current directory
            _T("src\\cursor_208x256_1jZ_icon.ico"),          // src subdirectory
            _T("..\\src\\cursor_208x256_1jZ_icon.ico"),      // Parent src directory
            _T("Release\\cursor_208x256_1jZ_icon.ico"),      // Release subdirectory
            _T("Debug\\cursor_208x256_1jZ_icon.ico")         // Debug subdirectory
        };
        
        // Try each path
        for (int i = 0; i < 5 && (!hIconBig || !hIconSmall); i++) {
            // If we already have one icon but not the other, we'll still try to load both
            hIconBig = hIconBig ? hIconBig : (HICON)LoadImage(
                NULL,
                iconPaths[i],
                IMAGE_ICON,
                32, 32,
                LR_LOADFROMFILE | LR_DEFAULTCOLOR
            );
            
            hIconSmall = hIconSmall ? hIconSmall : (HICON)LoadImage(
                NULL,
                iconPaths[i],
                IMAGE_ICON,
                16, 16,
                LR_LOADFROMFILE | LR_DEFAULTCOLOR
            );
            
            // If both loaded successfully, break out of the loop
            if (hIconBig && hIconSmall) break;
        }
        
        // Now assign the icons if they were loaded successfully
        if (hIconBig && hIconSmall) {
            wc.hIcon = hIconBig;
            wc.hIconSm = hIconSmall;
        } else {
            // Show additional error for fallback method
            error = GetLastError();
            _stprintf_s(errorMsg, 256, _T("Failed to load icon from file. Error code: %d"), error);
            MessageBox(NULL, errorMsg, _T("Icon Error"), MB_OK | MB_ICONERROR);
        }
    }

    ::RegisterClassEx(&wc);

    // Create window with WS_EX_TOPMOST style and WS_POPUP for borderless look
    g_hwnd = ::CreateWindowEx(
        WS_EX_TOPMOST,
        wc.lpszClassName,
        _T("AutoClicker"),
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        100, 100, 400, 300,
        NULL, NULL, wc.hInstance, NULL);

    if (!g_hwnd) {
         MessageBox(NULL, _T("Failed to create application window."), _T("Error"), MB_OK | MB_ICONERROR);
         ::UnregisterClass(wc.lpszClassName, wc.hInstance);
         return 1;
     }
 
     // Load settings *before* showing the window
     LoadSettings();
 
     // Determine initial show state based on settings
     int initialShowCmd = nCmdShow; // Default
     bool shouldHideWindow = false;
 
     if (g_startMinimized.load()) {
         if (g_startToTray.load()) {
             initialShowCmd = SW_HIDE; // Hide window if starting in tray
             shouldHideWindow = true;
         } else {
             initialShowCmd = SW_MINIMIZE; // Minimize if starting minimized
         }
     }
 
     // Initialize Direct3D first
     if (!CreateDeviceD3D(g_hwnd)) {
         CleanupDeviceD3D();
         ::DestroyWindow(g_hwnd);
         ::UnregisterClass(wc.lpszClassName, wc.hInstance);
         MessageBox(NULL, _T("Failed to initialize Direct3D."), _T("Error"), MB_OK | MB_ICONERROR);
         return 1;
     }
 
     // Show the window using the determined state
     ::ShowWindow(g_hwnd, initialShowCmd);
     ::UpdateWindow(g_hwnd);
 
     // If starting hidden to tray, add the tray icon now
     if (shouldHideWindow) {
         AddTrayIcon(g_hwnd);
         g_isMinimizedToTray = true; // Ensure state is correct
     }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Disable ImGui ini file creation
    io.IniFilename = NULL;

    // Set up default font with better character coverage
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 1;
    fontConfig.PixelSnapH = true;
    
    // Add default font with better coverage
    io.Fonts->AddFontDefault(&fontConfig);
    
    // Ensure font atlas is built
    io.Fonts->Build();

    // Setup Dear ImGui style
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.Alpha = 1.0f;
    style.WindowBorderSize = 0.0f; // Remove window border
    style.FrameBorderSize = 0.0f;  // Remove frame border
    style.PopupBorderSize = 0.0f;
    style.PopupRounding = style.WindowRounding;

    // Apply new Dark Theme Colors
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.88f, 0.88f, 0.88f, 1.00f); // #E0E0E0
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.46f, 0.46f, 0.46f, 1.00f); // #757575
    colors[ImGuiCol_WindowBg]               = ImVec4(0.118f, 0.118f, 0.118f, 1.00f); // #1E1E1E
    colors[ImGuiCol_ChildBg]                = ImVec4(0.118f, 0.118f, 0.118f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.145f, 0.145f, 0.145f, 1.00f); // #252525 (Slightly lighter for popups)
    colors[ImGuiCol_Border]                 = ImVec4(0.259f, 0.259f, 0.259f, 1.00f); // #424242
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f); // No border shadow
    colors[ImGuiCol_FrameBg]                = ImVec4(0.173f, 0.173f, 0.173f, 1.00f); // #2C2C2C
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.220f, 0.220f, 0.220f, 1.00f); // #383838
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.259f, 0.259f, 0.259f, 1.00f); // #424242
    colors[ImGuiCol_TitleBg]                = ImVec4(0.145f, 0.145f, 0.145f, 1.00f); // #252525
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.145f, 0.145f, 0.145f, 1.00f); // Same as inactive
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.118f, 0.118f, 0.118f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.145f, 0.145f, 0.145f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.118f, 0.118f, 0.118f, 1.00f); // Match window bg
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.259f, 0.259f, 0.259f, 1.00f); // #424242
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.302f, 0.714f, 0.675f, 0.80f); // Accent color (semi-transparent) #4DB6AC
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.302f, 0.714f, 0.675f, 1.00f); // Accent color #4DB6AC
    colors[ImGuiCol_CheckMark]              = ImVec4(0.302f, 0.714f, 0.675f, 1.00f); // Accent color #4DB6AC
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.302f, 0.714f, 0.675f, 1.00f); // Accent color #4DB6AC
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.502f, 0.796f, 0.769f, 1.00f); // Accent Hover #80CBC4
    colors[ImGuiCol_Button]                 = ImVec4(0.302f, 0.714f, 0.675f, 0.40f); // Accent color (transparent) #4DB6AC
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.302f, 0.714f, 0.675f, 1.00f); // Accent color #4DB6AC
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.149f, 0.651f, 0.604f, 1.00f); // Accent Active #26A69A
    colors[ImGuiCol_Header]                 = ImVec4(0.302f, 0.714f, 0.675f, 0.31f); // Accent color (transparent) #4DB6AC
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.302f, 0.714f, 0.675f, 0.80f); // Accent color #4DB6AC
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.149f, 0.651f, 0.604f, 1.00f); // Accent Active #26A69A
    colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.302f, 0.714f, 0.675f, 0.78f); // Accent color #4DB6AC
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.302f, 0.714f, 0.675f, 1.00f); // Accent color #4DB6AC
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.302f, 0.714f, 0.675f, 0.20f); // Accent color (very transparent) #4DB6AC
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.302f, 0.714f, 0.675f, 0.67f); // Accent color #4DB6AC
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.302f, 0.714f, 0.675f, 0.95f); // Accent color #4DB6AC
    colors[ImGuiCol_Tab]                    = LerpImVec4(colors[ImGuiCol_Header],       colors[ImGuiCol_TitleBgActive], 0.80f);
    colors[ImGuiCol_TabHovered]             = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_TabActive]              = LerpImVec4(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
    colors[ImGuiCol_TabUnfocused]           = LerpImVec4(colors[ImGuiCol_Tab],          colors[ImGuiCol_TitleBg], 0.80f);
    colors[ImGuiCol_TabUnfocusedActive]     = LerpImVec4(colors[ImGuiCol_TabActive],    colors[ImGuiCol_TitleBg], 0.40f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.88f, 0.88f, 0.88f, 1.00f); // Text color #E0E0E0
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f); // Example: Orange for hover
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.302f, 0.714f, 0.675f, 1.00f); // Accent color #4DB6AC
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.502f, 0.796f, 0.769f, 1.00f); // Accent Hover #80CBC4
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.302f, 0.714f, 0.675f, 0.35f); // Accent color (transparent) #4DB6AC
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f); // Yellow for drag/drop
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.302f, 0.714f, 0.675f, 1.00f); // Accent color #4DB6AC
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.10f, 0.10f, 0.10f, 0.75f); // Darker dimming

    // Adjust specific elements like the title bar background if needed
    // The custom title bar drawing code might need color adjustments too.

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load saved settings from file (MOVED EARLIER)
    // LoadSettings();

    // Create the clicker thread
    g_pClickerThread = new std::thread(AutoClickerThread);

    bool done = false;
    while (!done) {
        // Process Windows messages
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                done = true;
                g_immediateQuit = true;
            }
        }
        if (done) break;
        
        // Check for immediate quit
        if (g_immediateQuit.load()) {
            done = true;
            break;
        }
        
        // Start the ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Call the UI rendering function
        RenderUIWindow();

        // Render ImGui
        ImGui::Render();
        // Use the theme's background color for clearing
        const ImVec4& bg_color = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        const float clear_color_with_alpha[4] = { bg_color.x * bg_color.w, bg_color.y * bg_color.w, bg_color.z * bg_color.w, bg_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    // Save settings before exit
    SaveSettings();

    // Cleanup
    g_appShouldClose = true;
    g_immediateQuit = true;
    
    // Thread cleanup
    if (g_pClickerThread) {
        // Try a quick join or detach
        auto joinFuture = std::async(std::launch::async, [&]() {
            if (g_pClickerThread->joinable()) {
                g_pClickerThread->join();
            }
        });
        
        if (joinFuture.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
            if (g_pClickerThread->joinable()) {
                g_pClickerThread->detach();
            }
        }
        
        delete g_pClickerThread;
        g_pClickerThread = nullptr;
    }

    // ImGui cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    // DirectX cleanup
    CleanupDeviceD3D();
    ::DestroyWindow(g_hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    
    return 0;
}

// Helper functions for Direct3D initialization and cleanup
bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
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
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() 
{ 
    CleanupRenderTarget(); 
    if (g_pSwapChain) g_pSwapChain->Release(); 
    if (g_pd3dDeviceContext) g_pd3dDeviceContext->Release(); 
    if (g_pd3dDevice) g_pd3dDevice->Release(); 
}

void CreateRenderTarget() 
{ 
    ID3D11Texture2D* pBackBuffer; 
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)); 
    if (pBackBuffer) { 
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView); 
        pBackBuffer->Release(); 
    } 
}

void CleanupRenderTarget() 
{ 
    if (g_mainRenderTargetView) g_mainRenderTargetView->Release(); 
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Window message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Let ImGui process the message first
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) 
        return true;

    // Get ImGui IO to check if mouse is over controls
    ImGuiIO& io = ImGui::GetIO();
    
    switch (msg) {
        case WM_TRAYICON: {
            switch (lParam) {
                case WM_LBUTTONDBLCLK: // Double-click
                case WM_LBUTTONUP:     // Single left-click
                    // Restore window
                    ShowWindow(hWnd, SW_RESTORE);
                    SetForegroundWindow(hWnd);
                    RemoveTrayIcon(hWnd);
                    g_isMinimizedToTray = false;
                    break;
                case WM_RBUTTONUP: { // Right-click - Show context menu
                    POINT curPoint;
                    GetCursorPos(&curPoint);
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenu(hMenu, MF_STRING, 1, _T("Restore")); // Menu item ID 1
                    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenu(hMenu, MF_STRING, 2, _T("Exit"));    // Menu item ID 2

                    // Set foreground window is necessary for the menu to disappear when clicking elsewhere
                    SetForegroundWindow(hWnd);
                    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, curPoint.x, curPoint.y, 0, hWnd, NULL);
                    PostMessage(hWnd, WM_NULL, 0, 0); // Necessary to properly handle menu commands
                    DestroyMenu(hMenu);
                    } break;
            }
            return 0; // Handled
        }
        case WM_NCHITTEST: {
            // Custom hit test for the whole window client area to allow dragging
            // First, let default handling work
            LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
            if (hit == HTCLIENT) {
                POINT cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(hWnd, &cursor);
                
                // If in the top 30px (our title bar area), report as caption 
                // unless it's in the button area or ImGui wants to capture mouse
                if (cursor.y < 30) {
                    RECT clientRect;
                    GetClientRect(hWnd, &clientRect);
                    const int windowButtonsWidth = 45 * 3 + 15;
                    
                    if (cursor.x < clientRect.right - windowButtonsWidth) {
                        return HTCAPTION;
                    }
                }
            }
            return hit;
        }
        
        case WM_NCCALCSIZE: {
            if (wParam == TRUE) {
                return 0;
            }
            break;
        }
        
        case WM_LBUTTONDOWN: {
            // For controls that aren't in title bar but need dragging
            if (!io.WantCaptureMouse) {
                // Get cursor position (client coordinates)
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                
                // Get client area
                RECT clientRect;
                GetClientRect(hWnd, &clientRect);
                
                // Check if in window buttons area (top-right corner)
                const int titleBarHeight = 30;
                const int windowButtonsWidth = 45 * 3 + 15; // 3 buttons + padding
                
                bool inTitleBar = (y < titleBarHeight);
                bool inButtons = inTitleBar && (x > clientRect.right - windowButtonsWidth);
                
                // If in title bar but not in buttons area, start dragging
                if (inTitleBar && !inButtons) {
                    g_isDragging = true;
                    g_dragStartX = x;
                    g_dragStartY = y;
                    
                    // Get current window position
                    RECT winRect;
                    GetWindowRect(hWnd, &winRect);
                    g_dragWindowStartX = winRect.left;
                    g_dragWindowStartY = winRect.top;
                    
                    SetCapture(hWnd);
                    return 0;
                }
            }
            break;
        }
        
        case WM_MOUSEMOVE: {
            if (g_isDragging) {
                // Get current cursor position
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                
                // Calculate how far the mouse has moved
                int deltaX = x - g_dragStartX;
                int deltaY = y - g_dragStartY;
                
                // Calculate new window position
                int newX = g_dragWindowStartX + deltaX;
                int newY = g_dragWindowStartY + deltaY;
                
                // Move the window
                SetWindowPos(hWnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                return 0;
            }
            break;
        }
        
        case WM_LBUTTONUP: {
            if (g_isDragging) {
                g_isDragging = false;
                ReleaseCapture();
                return 0;
            }
            break;
        }
        
        case WM_CAPTURECHANGED: {
            if ((HWND)lParam != hWnd) {
                g_isDragging = false;
            }
            break;
        }
        
        case WM_SIZE:
            if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
            
        case WM_SYSCOMMAND: {
            UINT command = wParam & 0xfff0;
            if (command == SC_MINIMIZE) {
                // Check if we should minimize to tray instead
                if (g_startToTray.load()) {
                    // Only add icon if not already minimized to tray
                    if (!g_isMinimizedToTray) {
                         AddTrayIcon(hWnd);
                    }
                    ShowWindow(hWnd, SW_HIDE); // Hide the main window
                    g_isMinimizedToTray = true;
                    return 0; // Prevent default minimize
                }
            } else if (command == SC_CLOSE) {
                // Check if we should minimize to tray instead of closing
                if (g_minimizeOnClose.load()) {
                    // Only add icon if not already minimized to tray
                    if (!g_isMinimizedToTray) {
                         AddTrayIcon(hWnd);
                    }
                    ShowWindow(hWnd, SW_HIDE); // Hide the main window
                    g_isMinimizedToTray = true;
                    return 0; // Prevent default close
                } else {
                    // Original close behavior: Quit the application
                    g_immediateQuit = true; // Signal threads to stop
                    RemoveTrayIcon(hWnd);   // Clean up tray icon immediately
                    PostQuitMessage(0);     // Post quit message
                    return 0;               // Handled
                }
            } else if (command == SC_KEYMENU) {
                return 0; // Disable Alt system menu
            }
            break; // Let default handling occur for other commands
        }

        case WM_COMMAND: { // Handle menu commands here
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId) {
                case 1: // Restore
                    ShowWindow(hWnd, SW_RESTORE);
                    SetForegroundWindow(hWnd);
                    RemoveTrayIcon(hWnd);
                    g_isMinimizedToTray = false;
                    return 0;
                case 2: // Exit
                    g_immediateQuit = true; // Signal threads to stop
                    RemoveTrayIcon(hWnd);   // Clean up tray icon immediately
                    DestroyWindow(hWnd);    // Destroy window to trigger WM_DESTROY
                    PostQuitMessage(0);     // Ensure main loop terminates
                    return 0;
                default:
                    return DefWindowProc(hWnd, msg, wParam, lParam);
            }
            break;
        }
            
        case WM_DESTROY: {
            RemoveTrayIcon(hWnd); // Ensure tray icon is removed on exit
            ::PostQuitMessage(0);
            return 0;
        }
            
        case WM_KEYDOWN:
            if (waitingForKey) {
                if (wParam != VK_ESCAPE) { // Allow escape to cancel
                    toggleKey = wParam;
                    GetKeyName(toggleKey, keyName, sizeof(keyName));
                }
                waitingForKey = false;
                return 0;
            }
            
            // Add keyboard key selection 
            if (waitingForKeyboardKey) {
                // Only process keypresses if we're ready to listen
                if (!g_keyboardKeyReadyToListen) {
                    return 0;
                }
                
                // For keyboard input, avoid detecting mouse button as 'P'
                if (wParam != VK_LBUTTON && wParam != VK_ESCAPE) {
                    selectedKeyboardKey = wParam;
                    GetKeyName(selectedKeyboardKey, keyboardKeyName, sizeof(keyboardKeyName));
                    // Record when the key was set
                    g_lastKeyboardKeySetTime = GetTickCount();
                    SaveSettings(); // Save settings when keyboard key changes
                }
                
                waitingForKeyboardKey = false;
                return 0;
            }
            
            if (wParam == toggleKey) {
                clickerActive = !clickerActive;
                
                // If just activated and position lock is on, update position
                if (clickerActive && isPositionLocked) {
                    mtx.lock();
                    GetCursorPos(&currentCursorPos);
                    mtx.unlock();
                }
                
                return 0;
            }
            break;
    }
    
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

// Helper function to get the name of a key
void GetKeyName(int keyCode, char* outName, size_t nameSize) {
    // For letters and numbers
    if ((keyCode >= 'A' && keyCode <= 'Z') || (keyCode >= '0' && keyCode <= '9')) {
        outName[0] = (char)keyCode;
        outName[1] = '\0';
        return;
    }
    
    // For function keys and special keys
    switch (keyCode) {
        case VK_F1: strcpy_s(outName, nameSize, "F1"); return;
        case VK_F2: strcpy_s(outName, nameSize, "F2"); return;
        case VK_F3: strcpy_s(outName, nameSize, "F3"); return;
        case VK_F4: strcpy_s(outName, nameSize, "F4"); return;
        case VK_F5: strcpy_s(outName, nameSize, "F5"); return;
        case VK_F6: strcpy_s(outName, nameSize, "F6"); return;
        case VK_F7: strcpy_s(outName, nameSize, "F7"); return;
        case VK_F8: strcpy_s(outName, nameSize, "F8"); return;
        case VK_F9: strcpy_s(outName, nameSize, "F9"); return;
        case VK_F10: strcpy_s(outName, nameSize, "F10"); return;
        case VK_F11: strcpy_s(outName, nameSize, "F11"); return;
        case VK_F12: strcpy_s(outName, nameSize, "F12"); return;
        case VK_SHIFT: strcpy_s(outName, nameSize, "Shift"); return;
        case VK_CONTROL: strcpy_s(outName, nameSize, "Ctrl"); return;
        case VK_MENU: strcpy_s(outName, nameSize, "Alt"); return;
        case VK_TAB: strcpy_s(outName, nameSize, "Tab"); return;
        case VK_RETURN: strcpy_s(outName, nameSize, "Enter"); return;
        case VK_BACK: strcpy_s(outName, nameSize, "Backspace"); return;
        case VK_ESCAPE: strcpy_s(outName, nameSize, "Esc"); return;
        case VK_SPACE: strcpy_s(outName, nameSize, "Space"); return;
        
        // Add more key mappings as needed
    }
    
    // For other keys, try to get the key name from the system
    UINT scanCode = MapVirtualKey(keyCode, MAPVK_VK_TO_VSC);
    if (scanCode == 0) {
        sprintf_s(outName, nameSize, "Key(%d)", keyCode);
        return;
    }
    
    LONG lParam = scanCode << 16;
    
    // Set extended key bit for certain keys
    switch (keyCode) {
        case VK_RIGHT: case VK_LEFT: case VK_UP: case VK_DOWN:
        case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT: // Page Up/Down
        case VK_INSERT: case VK_DELETE:
        case VK_DIVIDE: case VK_NUMLOCK:
            lParam |= (1 << 24); // Set extended key bit
            break;
    }
    
    if (GetKeyNameTextA(lParam, outName, nameSize) == 0) {
        sprintf_s(outName, nameSize, "Key(%d)", keyCode);
    }
}