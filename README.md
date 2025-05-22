# AutoClicker

A lightweight, feature-rich auto-clicking utility built with Dear ImGui and DirectX 11. This application allows you to simulate mouse clicks and keyboard inputs at customizable intervals.

## Features

- **Customizable Click Interval**: Set the delay between clicks from 10ms to unlimited
- **Multiple Input Types**: Supports both mouse clicks and keyboard key presses
- **Mouse Button Selection**: Choose between left, right, and middle mouse buttons
- **Keyboard Input**: Simulate any keyboard key press
- **Position Locking**: Option to lock the cursor position when clicking
- **Global Hotkey**: Toggle the auto-clicker with a customizable hotkey
- **Persistent Settings**: All your preferences are automatically saved and restored between sessions
- **Modern UI**: Clean, borderless window interface with a custom title bar and tabbed layout
- **Enhanced Key Binding**: Visual feedback and protection against accidental activation
- **Cross-System Compatibility**: Robust hotkey detection across different Windows versions
- **System Tray Support**: Minimize to system tray with full context menu support
- **Startup Options**: Configure the application to start with Windows and start minimized
- **Dark Theme**: Modern dark theme with teal accent colors for better visual clarity

## What's New in This Version

### 🐛 **Critical Bug Fixes**
- **Fixed System Tray Issues**: Resolved duplicate tray icon creation that caused system instability
- **Fixed Window Message Handling**: Eliminated duplicate message handlers that could cause undefined behavior
- **Improved Memory Management**: Removed memory leaks and cleaned up unused variables
- **Enhanced Thread Safety**: Better synchronization and cleanup during application shutdown
- **Fixed Keyboard Input Compatibility**: Enhanced keyboard simulation to work with all applications, not just text boxes

### ✨ **Interface & Usability**
- **Tabbed Interface**: Settings organized into intuitive tabs: "Clicking", "Hotkeys", and "Startup & Behavior"
- **System Tray Integration**: 
  - Minimize to tray instead of closing
  - Right-click tray icon for quick access to restore or exit options
  - Double-click tray icon to restore the application
- **Startup Options**:
  - Start with Windows
  - Start minimized
  - Start minimized to system tray
  - Minimize to tray on close

### 🔧 **Technical Improvements**
- **Code Cleanup**: Removed duplicate functions and consolidated global variables
- **Better Error Handling**: Improved icon loading with multiple fallback paths
- **Enhanced Build System**: Automatic dependency management with FetchContent
- **Reduced Resource Usage**: Optimized performance and memory footprint
- **Enhanced Keyboard Input Engine**: 
  - Added scan code support for better hardware-level simulation
  - Implemented extended key flag support for arrow keys and function keys
  - Added timing delays between key down/up events for improved compatibility
  - Real-time administrator privilege detection with user warnings

## Building the Project

### Prerequisites

- **Windows operating system** (Windows 10/11 recommended)
- **Git** (required for downloading dependencies)
- **CMake** (3.16 or higher)
- **Visual Studio 2019 or newer** with C++ development tools
  - Or **Build Tools for Visual Studio** with MSVC compiler

### Installing Prerequisites

If you don't have Git installed:
```powershell
# Using Windows Package Manager (recommended)
winget install Git.Git

# Or download from: https://git-scm.com/download/win
```

### Build Steps

1. **Clone the repository**
   ```bash
   git clone https://github.com/siines/AutoClicker.git
   cd AutoClicker
   ```

2. **Create build directory and configure**
   ```bash
   mkdir build
   cd build
   cmake ..
   ```
   
   📝 **Note**: CMake will automatically download ImGui dependencies via FetchContent

3. **Build the project**
   ```bash
   # Release build (recommended)
   cmake --build . --config Release
   
   # Debug build (for development)
   cmake --build . --config Debug
   ```

4. **Run the application**
   ```bash
   # From the build directory
   .\Release\AutoClicker.exe
   
   # Or from the project root
   .\build\Release\AutoClicker.exe
   
   # For best keyboard input compatibility, run as Administrator:
   # Right-click AutoClicker.exe → "Run as administrator"
   ```

### Build Output

After successful build, you'll find:
```
build/Release/
├── AutoClicker.exe              # Main executable (~950 KB)
├── AutoClicker.pdb              # Debug symbols
└── cursor_208x256_1jZ_icon.ico  # Application icon
```

## Usage Guide

1. **Start the application**: 
   - Run `AutoClicker.exe` from the Release folder
   - The application will load your previous settings automatically

2. **Configure settings**:
   - **Clicking Tab**:
     - Set the click interval in milliseconds (minimum 10ms)
     - Choose whether to lock the cursor position
     - Select the input type (mouse or keyboard)
     - For mouse input, select which button to simulate
     - For keyboard input, select which key to simulate
   - **Hotkeys Tab**:
     - Click the button to change the start/stop toggle hotkey (ESC to cancel)
     - Visual feedback shows when the system is ready to accept key input
   - **Startup & Behavior Tab**:
     - Configure startup behavior with Windows
     - Set tray minimization preferences

3. **Start/Stop clicking**:
   - Click the "Start" button or use the configured hotkey (default: F6)
   - Status indicator shows current state and active button/key
   - The clicking will continue until stopped by pressing the button or hotkey again

4. **System Tray Features**:
   - Enable "Minimize to Tray on Close" to hide instead of closing
   - Double-click the tray icon to restore the application
   - Right-click the tray icon for context menu (Restore/Exit)
   - Tray icon persists until explicitly closed via context menu

## Troubleshooting

### Build Issues
- **"git not found"**: Install Git using `winget install Git.Git` or download from git-scm.com
- **CMake errors**: Ensure CMake 3.16+ is installed and Visual Studio C++ tools are available
- **Permission errors**: Run PowerShell as Administrator if needed

### Runtime Issues
- **Application doesn't start**: 
  - Update graphics drivers and ensure DirectX 11 is available
  - Check if Windows Defender or antivirus is blocking the executable
- **Mouse clicking doesn't work**: 
  - Some applications with enhanced security may block simulated inputs
  - Try running as Administrator for protected applications
- **Keyboard input not working**: 
  - **For games and secure applications**: Run AutoClicker as Administrator (most important!)
  - **For regular applications**: The enhanced keyboard engine should work without admin privileges
  - **Still not working?**: Some anti-cheat systems block all simulated input
  - **Alternative**: Try using different keys (some applications block specific keys)
- **Hotkey doesn't respond**: 
  - Ensure the hotkey isn't already in use by another application
  - Wait for the cooldown period after setting a new hotkey
- **High DPI issues**: 
  - Adjust Windows scaling settings
  - Right-click executable → Properties → Compatibility → High DPI settings

### System Tray Issues
- **Tray icon doesn't appear**: Ensure system tray icons are enabled in Windows settings
- **Can't restore from tray**: Double-click the icon or use right-click → Restore

## Development

### Code Structure
- `src/main.cpp` - Main application logic (consolidated single-file architecture)
- `src/AutoClicker.rc` - Windows resources and version information
- `src/resource.h` - Resource definitions
- `CMakeLists.txt` - Build configuration with automatic dependency management

### Contributing
- Ensure code compiles without errors
- Test all features before submitting changes
- Follow existing code style and conventions

## License

This project is provided as-is for educational and personal use.