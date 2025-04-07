# AutoClicker

A lightweight, feature-rich auto-clicking utility built with Dear ImGui and DirectX 11. This application allows you to simulate mouse clicks and keyboard inputs at customizable intervals.

## Features

- **Customizable Click Interval**: Set the delay between clicks from 10ms to 1000ms
- **Multiple Input Types**: Supports both mouse clicks and keyboard key presses
- **Mouse Button Selection**: Choose between left, right, and middle mouse buttons
- **Keyboard Input**: Simulate any keyboard key press
- **Position Locking**: Option to lock the cursor position when clicking
- **Global Hotkey**: Toggle the auto-clicker with a customizable hotkey
- **Persistent Settings**: All your preferences are automatically saved and restored between sessions
- **Modern UI**: Clean, borderless window interface with a custom title bar and tabbed layout
- **Improved Key Binding**: Enhanced key binding UI with visual feedback and protection against accidental activation
- **Cross-System Compatibility**: Fixed issues with hotkey detection on different systems
- **System Tray Support**: Option to minimize to system tray instead of taskbar
- **Startup Options**: Configure the application to start with Windows and start minimized
- **Dark Theme**: Modern dark theme with accent colors for better visual clarity

## What's New in This Version

- **Tabbed Interface**: Settings are now organized into intuitive tabs: "Clicking", "Hotkeys", and "Startup & Behavior"
- **System Tray Integration**: 
  - Minimize to tray instead of closing
  - Right-click tray icon for quick access to restore or exit options
  - Double-click tray icon to restore the application
- **Startup Options**:
  - Start with Windows
  - Start minimized
  - Start minimized to system tray
- **Improved Visual Feedback**: Added subtle animations and visual cues for better user experience
- **Fixed Key Binding Issues**: Resolved problems with hotkey detection and added cooldown periods
- **Optimized Performance**: Reduced resource usage and improved responsiveness

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
   ```
   cmake --build . --config Release
   ```

4. **Run the application**
   The executable will be located in `build/Release/` folder.

## Usage Guide

1. **Start the application**: Run the executable

2. **Configure settings**:
   - **Clicking Tab**:
     - Set the click interval in milliseconds
     - Choose whether to lock the cursor position
     - Select the input type (mouse or keyboard)
     - For mouse input, select which button to simulate
     - For keyboard input, select which key to simulate
   - **Hotkeys Tab**:
     - Click the button to change the start/stop toggle hotkey (ESC to cancel)
   - **Startup & Behavior Tab**:
     - Configure options like "Start with Windows", "Start Minimized", "Start Minimized to System Tray", and "Minimize to Tray on Close"

3. **Start/Stop clicking**:
   - Click the "Start" button or use the configured hotkey (default: F6)
   - The clicking will continue until stopped by pressing the button or hotkey again

4. **System Tray Features**:
   - If "Minimize to Tray on Close" is enabled, clicking the 'X' button will hide the window and show an icon in the system tray
   - Double-click the tray icon to restore the application
   - Right-click the tray icon for options (Restore/Exit)
   - The Exit option always quits the application completely, regardless of settings

## Troubleshooting

- **Application doesn't start**: Make sure your graphics drivers are up to date and DirectX 11 is properly installed
- **Clicking doesn't work**: Some applications with enhanced security may block simulated inputs
- **Hotkey doesn't respond**: Ensure the hotkey isn't already in use by another application
- **High DPI Settings**: If UI appears too small or distorted, adjust Windows scaling settings or run in compatibility mode