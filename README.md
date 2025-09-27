# err_  - A System Dashboard


System Dashboard for error.os


> **A lightweight system utility application only made for error.os you can customize to fit your own operating system** 

## 🎯 Overview

**err_** is a comprehensive Qt6-based system dashboard designed specifically for **error.os** and compatible Debian-based distributions. Built with a terminal-aesthetic monospace interface, it provides system administrators and users with essential system management tools in a single, elegant application.

## ✨ Features

### 📊 System Information
- **Real-time OS detection** from `/etc/os-release`
- **CPU architecture** and hardware information
- **Memory usage** statistics from `/proc/meminfo`
- **Storage capacity** monitoring
- **Interactive logo** of error.os

### ⚙️ Driver Management
- **Automatic NVIDIA GPU detection** via `lspci`
- **One-click NVIDIA driver installation**
- **Comprehensive printer driver support**
- **CUPS printing system setup**

### 📦 Application Management
- **Pre-configured application installer** with popular packages
- **Batch installation** with checkable list interface
- **Package removal tool** with auto-completion
- **dpkg integration** for installed package detection

### 🔧 System Integration
- **Multi-terminal support** (konsole, xterm, qterminal)
- **Privilege escalation** via sudo in terminal
- **System settings launcher** (systemsettings/lxqt-config)
- **Professional progress dialogs**

## 🖥️ Interface
  ( kind of looks like this) 
```
┌─────────────────────────────────────────────────┐
│ err_ - System Dashboard                         │
├─────────┬─────────┬─────────┬─────────┬─────────┤
│System   │ Drivers │ Install │ Remove  │Settings │
│Info     │         │ Apps    │ Apps    │         │
├─────────┴─────────┴─────────┴─────────┴─────────┤
│                                                 │
│  ┌─ System Information ──┐  ┌─ Logo ─┐          │
│  │ OS: Debian GNU/Linux  │  │        │          │
│  │ CPU: x86_64          │  │  [◉◉]  │          │
│  │ RAM: 8192 MB         │  │        │          │
│  │ Storage: 256 GB      │  │        │          │
│  └──────────────────────┘  └────────┘          │
└─────────────────────────────────────────────────┘
```

## 📋 Prerequisites

### System Requirements
- **Qt6** (Core, Widgets, Network)
- **Debian-based distribution** (Debian, Ubuntu, etc.)
- **apt package manager**
- **Terminal emulator** (konsole, xterm, or qterminal)

### Dependencies
```bash
sudo apt install qt6-base-dev qt6-tools-dev cmake build-essential
```

## 🚀 Installation

### Building from Source
```bash
# Clone the repository
git clone https://github.com/your-org/err_.git
cd err_

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Install (optional)
sudo make install
```

### CMakeLists.txt Configuration
```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
target_link_libraries(err_ Qt6::Core Qt6::Widgets)
```

## 🎮 Usage

### Launch Application
```bash
./err_
```

### Navigation
- **System Info Tab**: View hardware and OS information
- **Drivers Tab**: Install graphics and printer drivers
- **Install Apps Tab**: Select and install applications
- **Remove Apps Tab**: Uninstall packages with auto-completion
- **Settings Tab**: Access system configuration tools

### Easter Egg
Triple-click the error.os logo to find out. 

## 📁 Application List

The installer includes these pre-configured applications:

| Application | Package | Description |
|-------------|---------|-------------|
| Firefox | `firefox-esr` | Web Browser |
| VLC | `vlc` | Media Player |
| LibreOffice | `libreoffice` | Office Suite |
| GIMP | `gimp` | Image Editor |
| Thunderbird | `thunderbird` | Email Client |
| Kdenlive | `kdenlive` | Video Editor |
| Audacity | `audacity` | Audio Editor |
| Inkscape | `inkscape` | Vector Graphics |
| OBS Studio | `obs-studio` | Screen Recorder |
| FileZilla | `filezilla` | FTP Client |
| Transmission | `transmission-gtk` | Torrent Client |
| Chromium | `chromium` | Web Browser |
| Geany | `geany` | Code Editor |
| Qbittorrent | `qbittorrent` | Torrent Client |
# And more than you think. 

## 🎨 Design Philosophy

**err_** embraces a **monospace terminal aesthetic** with:
- **DejaVu Sans Mono** font family
- **Dark theme** with blue accent colors (`#00BFFF`, `#1a3cff`)
- **Consistent spacing** and typography

## 🔧 Technical Architecture

```
err_/
cmakelists.txt
main.cpp
resources.qrc
   / { 3 resources }
err_.desktop
```

## 🛡️ Security Considerations
- **Privilege escalation** handled via terminal sudo prompts instead of normal pkexec. 
- **No embedded credentials** or hardcoded passwords
- **User confirmation** required for all system modifications
- **Read-only system information** gathering

## 🐛 Troubleshooting

### Common Issues

**No terminal found error:**
```bash
sudo apt install konsole xterm
```

**Qt6 compilation errors:**
```bash
sudo apt install qt6-base-dev qt6-tools-dev
```

**Package installation fails:**
```bash
sudo apt update && sudo apt upgrade
```
 
If you encounter any more issues then consider pulling an issue tab  or joining our discord server. 

## 📄 License

This project is licensed under the **MIT License** - see the LICENSE file for details.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📞 Support

For support and bug reports, please create an issue in the repository.

---

```
Built for error.os
Professional • Cool • fast 
```