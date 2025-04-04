# AutoClicker

A lightweight, feature-rich auto-clicking utility built with Dear ImGui and DirectX 11. This application allows you to simulate mouse clicks and keyboard inputs at customizable intervals.

## Features

- **Customizable Click Interval**: Set the delay between clicks from 10ms to 1000ms
- **Multiple Input Types**: Supports both mouse clicks and keyboard key presses
- **Mouse Button Selection**: Choose between left, right, and middle mouse buttons
- **Keyboard Input**: Simulate any keyboard key press
- **Position Locking**: Option to lock the cursor position when clicking
- **Global Hotkey**: Toggle the auto-clicker with a customizable hotkey
- **Modern UI**: Clean, borderless window interface with custom title bar
- **Improved Key Binding**: Enhanced key binding UI with visual feedback and protection against accidental activation
- **Cross-System Compatibility**: Fixed issues with hotkey detection on different systems

## What's New in This Version

This version addresses several user experience issues with the previous release:

- **Fixed Key Binding Issues**: Resolved a problem where pressing the key binding button would immediately register as the 'P' key on some systems
- **Activation Prevention**: Added a cooldown period after setting a hotkey to prevent accidental activation
- **Visual Feedback**: Added subtle visual cues to indicate when the application is ready to accept a new key binding
- **Improved UX**: Streamlined the key binding process with clear feedback and status indicators
- **Bug Fixes**: Fixed issues with key detection and handling across different systems

## How It Works

The application uses a dedicated thread for the clicking functionality, separate from the UI thread. This allows for responsive operation even when the application window isn't in focus.

- The main interface is built with Dear ImGui, a lightweight immediate-mode GUI library
- DirectX 11 is used for rendering
- Input simulation is handled through the Windows API (SendInput)
- The window features a custom-drawn title bar with minimize, maximize, and close buttons

## Building the Project

### Prerequisites

- Windows operating system
- CMake (3.10 or higher)
- Visual Studio 2019 or newer with C++ development tools

### Build Steps

1. **Clone the repository**

```
git clone https://github.com/siines/AutoClicker.git
cd AutoClicker
```

2. **Generate build files with CMake**

```
mkdir build
cd build
cmake ..
```

3. **Build the project**

For Debug build:
```
cmake --build . --config Debug
```

For Release build:
```
cmake --build . --config Release
```

4. **Run the application**

The executable will be located in either `build/Debug/` or `build/Release/` depending on your build configuration.

## Usage Guide

1. **Start the application**: Run the executable
2. **Configure settings**:
   - Set the click interval in milliseconds
   - Choose whether to lock the cursor position
   - Select the input type (mouse or keyboard)
   - For mouse input, select which button to simulate
   - For keyboard input, select which key to simulate
3. **Start/Stop clicking**:
   - Click the "Start" button or use the configured hotkey (default: F6)
   - The clicking will continue until stopped by pressing the button or hotkey again
4. **Customize hotkey**:
   - Click on the hotkey button
   - Wait for the "Press new key" prompt
   - Press your desired key to change the toggle hotkey
   - A brief cooldown period will prevent accidental activation

## Known Issues and Limitations

- **Window Dragging**: If dragging the window doesn't work properly, try clicking on the title bar area
- **Resource Loading**: Custom icon loading is currently disabled to prevent resource-related crashes
- **Multiple Monitors**: Cursor position may behave unexpectedly with certain multi-monitor configurations
- **High DPI Settings**: May have UI scaling issues on high DPI displays
- **Application Focus**: The auto-clicker will continue to operate even when other applications are in focus
- **Administrator Rights**: Some applications may require the auto-clicker to run with administrator privileges

## Troubleshooting

- **Application doesn't start**: Make sure your graphics drivers are up to date and DirectX 11 is properly installed
- **Clicking doesn't work**: Some applications with enhanced security may block simulated inputs
- **Hotkey doesn't respond**: Ensure the hotkey isn't already in use by another application
- **Window appears blank**: Try restarting the application or updating your graphics drivers
- **Key binding immediately selects 'P'**: This issue should be fixed in this version, but if it persists, try clicking the button and waiting for the visual indicator before pressing your desired key