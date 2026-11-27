# STRSVR - GNSS Stream Server (Linux Port)

A Linux-native reimplementation of the **STRSVR** GNSS stream server originally written by **T.Takasu** (2007-2017). This version is written in C++17 with Qt5 for cross-platform portability and native Linux GUI support.

## About the Original

**STRSVR** (STRing Server) is a Windows application that processes and distributes GNSS data streams. Originally built with Delphi/VCL, it supports:
- NMEA 0183 parsing and generation (GGA, GSA, GSV, RMC, ZDA)
- Trimble CMR/CMR+ protocol support
- Serial port I/O with configurable baud rates
- TCP/IP server and client modes
- UDP broadcast/receive
- NTRIP client for RTK corrections
- Data conversion and relay capabilities

## This Port

### Features Implemented
- ✅ Full INI configuration parser (strsvr.ini compatible)
- ✅ NMEA 0183 sentence parser and generator (GGA, GSA, GSV, RMC, ZDA)
- ✅ Serial port I/O (POSIX via QSerialPort)
- ✅ TCP server (multi-client broadcast)
- ✅ TCP client (auto-reconnect)
- ✅ UDP server and client
- ✅ Stream routing between channels
- ✅ Real-time GNSS position monitoring
- ✅ Modern Qt5 GUI (Dashboard, Channels, Monitor, Log)
- ✅ System tray integration
- ✅ Cross-platform (Linux primary, Windows supported)
- ✅ Thread-safe logging system

### Planned / In Progress
- ⏳ CMR/CMR+ binary protocol parsing
- ⏳ NTRIP client with mountpoint authentication
- ⏳ File I/O (record/playback streams)
- ⏳ FTP file transfer support
- ⏳ More advanced data conversion rules
- ⏳ Antenna position handling

## Building

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install qt5-default qt5serialport5-dev cmake build-essential
cd linux_port
chmod +x build.sh
./build.sh --run
```

### Linux (Fedora/RHEL)
```bash
sudo dnf install qt5-qtbase-devel qt5-qtserialport-devel cmake gcc-c++
cd linux_port
./build.sh
./build/strsvr
```

### Arch Linux
```bash
sudo pacman -S qt5-base qt5-serialport cmake base-devel
cd linux_port
./build.sh
```

### Windows
```cmd
REM Open "Developer Command Prompt for VS 2019/2022" or Qt MinGW Shell
cd linux_port
build.bat
```

### Manual CMake
```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -- -j$(nproc)
./strsvr
```

## Configuration

Copy `strsvr.ini.example` to `strsvr.ini` and edit as needed. The configuration file follows the original format with sections for global settings, data conversion, serial commands, TCP/IP, paths, and more.

### Command-line Options
```
strsvr [-c config_file] [--nogui] [--help] [--version]

-c, --config    Specify configuration file path (default: strsvr.ini)
    --nogui     Run in headless mode (no GUI)
    --help      Show help
    --version   Show version
```

## Project Structure

```
linux_port/
├── CMakeLists.txt              # Build configuration
├── build.sh                    # Linux build script
├── build.bat                   # Windows build script
├── strsvr.ini.example          # Configuration template
├── README.md                   # This file
└── src/
    ├── main.cpp                # Application entry point
    ├── core/                   # Core engine
    │   ├── appconfig.h/cpp     # INI configuration parser
    │   ├── logger.h/cpp        # Thread-safe logger
    │   ├── nmeaparser.h/cpp    # NMEA 0183 parser/generator
    │   └── streammanager.h/cpp # Central data flow manager
    ├── io/                     # I/O modules
    │   ├── serialport.h/cpp    # Serial port worker
    │   └── network.h/cpp       # TCP/UDP server/client workers
    └── gui/                    # Qt GUI
        ├── mainwindow.h/cpp    # Main application window
        ├── settingsdialog.h/cpp # Preferences dialog
        ├── serialdialog.h/cpp  # Serial port configuration
        ├── networkdialog.h/cpp # Network (TCP/UDP/NTRIP) configuration
        └── aboutdialog.h/cpp   # About dialog
```

## GNSS Data Flow

```
                    ┌──────────────┐
   Serial Port ──►  │              │
                    │   Stream     │
   TCP Client  ──►  │   Manager   │ ──► TCP Server (broadcast to clients)
                    │              │ ──► UDP Client (multicast)
   UDP Server  ──►  │              │ ──► Data Conversion Engine
                    └──────────────┘ ──► File Output
                             │
                             ▼
                    ┌──────────────┐
                    │  NMEA Parser │ ──► Position Monitor (GUI)
                    └──────────────┘
```

## Legacy Notes

This is **not a drop-in binary replacement** for the original strsvr.exe. The original was a Delphi application with proprietary logic, and this port is a clean-room reimplementation that provides equivalent functionality using standard open-source components. Configuration files (`strsvr.ini`) should be largely compatible.

## License

This port is released under the GPL v3 or later. The original STRSVR by T.Takasu had its own license — if you use algorithms or data formats from the original, ensure compliance with that license as well.

## Technical Details

- **Language**: C++17
- **GUI Framework**: Qt 5.15+ (tested with Qt 5.15.2, 5.15.3; works with Qt 6.x with minor adjustments)
- **Build System**: CMake 3.14+
- **Threading**: Qt event loop + QThread where needed
- **Serial I/O**: QSerialPort (cross-platform, works on Linux /dev/ttyUSB*, /dev/ttyACM*)
- **Network I/O**: QTcpServer, QTcpSocket, QUdpSocket
- **Configuration**: QSettings with INI format
- **Logging**: Custom thread-safe Logger with Qt signals for GUI integration
