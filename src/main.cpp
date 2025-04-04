#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

// Common Controls version 6 manifest
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Include resource header
#include "resource.h"

// Remove Font Awesome include as it's not used
// #include "fonts/IconsFontAwesome6.h"

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

// Color themes
ImVec4 g_themeAccent = ImVec4(0.28f, 0.56f, 1.0f, 1.0f);
ImVec4 g_themeAccentHover = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
ImVec4 g_themeAccentActive = ImVec4(0.2f, 0.4f, 0.9f, 1.0f);
ImVec4 g_themeBackground = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
ImVec4 g_themeHeaderBg = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);

// Global variables
bool isRunning = false;
int clicksPerSecond = 10;
std::atomic<bool> clickerActive = false;
std::atomic<bool> shouldExit = false;
std::mutex mtx;
POINT currentCursorPos = { 0, 0 };
bool isPositionLocked = false;
int toggleKey = VK_F6;
char keyName[32] = "F6";
bool waitingForKey = false;
float clickFeedbackTimer = 0.0f;
float clickFeedbackDuration = 0.5f;
float buttonHoverAlpha = 0.0f;
float targetHoverAlpha = 0.0f;
std::string statusText = "";

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

// Lerp between colors
ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t) {
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
    ImGui::TextDisabled("(?)");
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
        if (hotkeyCurrentlyPressed && !hotkeyPressedLast && !g_isSettingHotkey.load()) {
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
            }
            
            // Scan for keypresses - skip modifier keys
            for (int vk = 8; vk < 255; ++vk) {
                // Skip modifier keys
                if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || 
                    vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL || 
                    vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU || 
                    vk == VK_LWIN || vk == VK_RWIN || vk == VK_APPS) {
                    continue;
                }
                
                // Check if key is pressed
                if (GetAsyncKeyState(vk) & 0x8000) {
                    // Don't set if ESC (used to cancel)
                    if (vk != VK_ESCAPE) {
                        g_toggleHotkey = vk;
                    }
                    g_isSettingHotkey = false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(250)); 
                    break;
                }
            }
            
            // Allow ESC to cancel without setting
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                g_isSettingHotkey = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
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
                    // Send keyboard events
                    INPUT input[2] = {};
                    input[0].type = INPUT_KEYBOARD;
                    input[1].type = INPUT_KEYBOARD;
                    
                    // Key down
                    input[0].ki.wVk = selectedKeyboardKey;
                    input[0].ki.dwFlags = 0;
                    
                    // Key up
                    input[1].ki.wVk = selectedKeyboardKey;
                    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
                    
                    SendInput(2, input, sizeof(INPUT));
                }
                
                // Update click feedback timer
                clickFeedbackTimer = clickFeedbackDuration;
            }
            
            // Get sleep time based on clicks per second
            int sleepTimeMs = 1000 / clicksPerSecond;
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
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

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
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));

    // Minimize Button
    if (ImGui::Button("_", ImVec2(45, buttonHeight))) { ShowWindow(g_hwnd, SW_MINIMIZE); }
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);

    // Maximize Button
    WINDOWPLACEMENT wp; wp.length = sizeof(WINDOWPLACEMENT); GetWindowPlacement(g_hwnd, &wp);
    bool isMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
    const char* maxChar = isMaximized ? "#" : "O";
    if (ImGui::Button(maxChar, ImVec2(45, buttonHeight))) { ShowWindow(g_hwnd, isMaximized ? SW_RESTORE : SW_MAXIMIZE); }
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);

    // Close Button
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("X", ImVec2(45, buttonHeight))) { g_appShouldClose = true; PostQuitMessage(0); }
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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Configuration Section
    ImGui::Text("Configuration");
    ImGui::Indent();

    // Click Interval
    int currentInterval = g_clickInterval.load();
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("Interval (ms)", &currentInterval, 10, 50)) {
        if (currentInterval < 10) currentInterval = 10;
        g_clickInterval.store(currentInterval);
    }
    ImGui::SameLine(); 
    HelpMarker("Delay between clicks (minimum 10ms)");

    // Lock Cursor Option
    bool lockCursor = g_lockCursor.load();
    if (ImGui::Checkbox("Lock Cursor Position", &lockCursor)) {
        g_lockCursor.store(lockCursor);
        if (lockCursor && g_running.load()) {
            GetCursorPos(&g_lockedCursorPos);
        }
    }
    ImGui::SameLine(); 
    HelpMarker("Locks cursor at its current position when clicking starts");

    // Add hotkey selection UI
    ImGui::Spacing();
    
    // Toggle Hotkey
    ImGui::Text("Toggle Key:");
    ImGui::SameLine();
    std::string hotkey = VKCodeToString(g_toggleHotkey.load());
    if (g_isSettingHotkey.load()) {
        ImGui::Button("Press new key... (ESC to cancel)", ImVec2(200, 0));
    } else {
        if (ImGui::Button(hotkey.c_str(), ImVec2(100, 0))) {
            g_isSettingHotkey = true;
        }
        ImGui::SameLine();
        HelpMarker("Click to change the start/stop hotkey");
    }
    
    ImGui::Unindent();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Add Input Type Selection (before mouse button)
    ImGui::Text("Input Type:");
    ImGui::SameLine();
    
    if (ImGui::BeginCombo("##InputType", inputTypeNames[selectedInputType])) {
        for (int i = 0; i < 2; i++) {
            bool isSelected = (selectedInputType == i);
            if (ImGui::Selectable(inputTypeNames[i], isSelected)) {
                selectedInputType = (InputType)i;
            }
            
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Select whether to simulate mouse or keyboard input");
    
    // Only show mouse button selection when in mouse input mode
    if (selectedInputType == MOUSE_INPUT) {
        ImGui::Spacing();
        ImGui::Text("Mouse Button:");
        ImGui::SameLine();
        
        if (ImGui::BeginCombo("##MouseButton", mouseButtonNames[selectedMouseButton])) {
            for (int i = 0; i < 3; i++) {
                bool isSelected = (selectedMouseButton == i);
                if (ImGui::Selectable(mouseButtonNames[i], isSelected)) {
                    selectedMouseButton = (MouseButton)i;
                }
                
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select which mouse button to auto-click");
    }
    
    // Show keyboard input selection when keyboard mode is selected
    if (selectedInputType == KEYBOARD_INPUT) {
        ImGui::Spacing();
        ImGui::Text("Keyboard Key:");
        ImGui::SameLine();
        
        if (ImGui::Button(waitingForKeyboardKey ? "Press any key..." : keyboardKeyName, ImVec2(100, 0))) {
            waitingForKeyboardKey = true;
        }
        
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to change the keyboard key to simulate");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();

    // Action buttons with static colors
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.6f, 0.15f, 1.0f));
    
    if (ImGui::Button(g_running.load() ? "Stop" : "Start", ImVec2(120, 30))) {
        g_running = !g_running;
        
        // Capture cursor position when starting with lock enabled
        if (g_running && g_lockCursor.load()) {
            GetCursorPos(&g_lockedCursorPos);
        }
    }
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();

    // Quit button with static color
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
    
    if (ImGui::Button("Quit", ImVec2(120, 30))) {
        g_immediateQuit = true;
        PostQuitMessage(0);
    }
    ImGui::PopStyleColor(3);

    ImGui::PopStyleVar();
    ImGui::End(); // End Main Content
}

// Main code
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Create application window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, 
                      NULL, NULL, NULL, _T("AutoClicker"), NULL };
    
    // Load the application icon
    wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));

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

    // Initialize Direct3D first
    if (!CreateDeviceD3D(g_hwnd)) {
        CleanupDeviceD3D();
        ::DestroyWindow(g_hwnd);
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        MessageBox(NULL, _T("Failed to initialize Direct3D."), _T("Error"), MB_OK | MB_ICONERROR);
        return 1;
    }

    ::ShowWindow(g_hwnd, nCmdShow);
    ::UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Disable ImGui ini file creation
    io.IniFilename = NULL;

    // Set up default font
    // Instead of trying to load fonts from resources, just use the default font
    io.Fonts->AddFontDefault();

    // Setup Dear ImGui style and colors
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.Alpha = 1.0f;

    // Colors
    style.Colors[ImGuiCol_WindowBg] = g_themeBackground;
    style.Colors[ImGuiCol_Header] = g_themeHeaderBg;
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = g_themeAccent;
    style.Colors[ImGuiCol_SliderGrabActive] = g_themeAccentActive;
    style.Colors[ImGuiCol_CheckMark] = g_themeAccent;
    style.Colors[ImGuiCol_Border] = ImVec4(0.3f, 0.3f, 0.3f, 0.5f);

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

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
        const float clear_color_with_alpha[4] = { 0.15f, 0.15f, 0.15f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

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
            
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
            break;
            
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
            
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
                if (wParam != VK_ESCAPE) { // Allow escape to cancel
                    selectedKeyboardKey = wParam;
                    GetKeyName(selectedKeyboardKey, keyboardKeyName, sizeof(keyboardKeyName));
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