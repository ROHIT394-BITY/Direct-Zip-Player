# DirectZip ⚡ In-Memory ZIP MP4 Video Player

**DirectZip** is a native C desktop video player that allows users to play MP4 videos directly from ZIP archives **without extracting them to local disk storage**. 

By combining pure in-memory ZIP decompression with a custom FFmpeg `AVIOContext` memory-streaming pipeline and SDL2 hardware-accelerated rendering, DirectZip completely eliminates the need for temporary extraction folders, intermediate files, or manual archive unpacking.

---

## 1. Key Features

- **⚡ Zero Disk Extraction:** Reads and decompresses compressed MP4 video streams directly into RAM buffers. Never creates temporary files or writes video data to local storage.
- **📂 Multi-Format Import:** Open ZIP archives via native Windows file picker dialog, drag-and-drop into the player window, or command-line arguments.
- **📑 Automatic MP4 Discovery & Playlist:** Scans the ZIP central directory, filters valid MP4/M4V video entries, ignores non-media files, and generates a visual playlist with durations and file sizes.
- **🎬 Hardware-Accelerated Video Playback:** Decodes standard H.264 video streams using FFmpeg (`libavcodec`, `libswscale`) and renders at 60 FPS using SDL2 with full **1080p Full HD (1920x1080)** or **720p HD (1280x720)** output resolution and bicubic scaling.
- **✨ HD Resolution Badge:** Automatically detects stream resolution and displays live `[ 1080p FHD ]` or `[ 720p HD ]` indicators on the top bar and playlist.
- **🔊 Low-Latency Resampled Audio:** Resamples audio streams using `libswresample` to 16-bit 44.1 kHz stereo and outputs with SDL2 audio queuing and software volume attenuation.
- **⏩ Adaptive Video Seeking:** Click or drag along the playback timeline for instant in-memory random access with decoder flushing and audio clock resynchronization.
- **🧵 Responsive Multithreaded Engine:** Heavy ZIP decompression is offloaded to background worker threads, ensuring the UI remains smooth and responsive with progress indicators.
- **🛡️ Memory Safety Arena:** Configurable memory limits (default 2 GB), integer overflow protection, safe allocation checks, and zero memory leaks.

---

## 2. Technology Stack

- **Language:** Pure C (C11 standard).
- **Build System:** CMake (compatible with Ninja, MinGW Makefiles, and MSVC).
- **Decompression Engine:** Pure C PKZIP central directory parser & RFC 1951 Deflate inflator with CRC32 verification.
- **Multimedia Decoding:** FFmpeg 7.x (`libavformat`, `libavcodec`, `libavutil`, `libswscale`, `libswresample`) using custom in-memory `AVIOContext`.
- **UI & Audio Rendering:** SDL2 (Windowing, hardware-accelerated rendering, audio output, drag-and-drop events).
- **Threading:** Portable abstraction layer (`threading.h` / `threading.c`) supporting Win32 and POSIX threads.

---

## 3. Project Structure

```
DirectZip/
├── CMakeLists.txt              # CMake build configuration
├── README.md                   # Project documentation
├── build.bat                   # Automated Windows build script
├── run.bat                     # Application launch script
├── include/
│   ├── zip_engine.h            # Central directory parser & in-memory decompression
│   ├── memory_manager.h        # RAM buffer allocation & boundary management
│   ├── video_decoder.h         # FFmpeg custom AVIOContext & frame decoders
│   ├── audio_player.h          # SDL2 audio queuing & volume/mute subsystem
│   ├── player_controller.h     # High-level state machine & playback coordinator
│   ├── playlist.h              # Track selection & playlist metadata manager
│   ├── ui.h                    # Modern dark-theme UI & rendering subsystem
│   └── threading.h             # Portable thread/mutex/condition variable API
├── src/
│   ├── main.c                  # Application entry point & 60 FPS event loop
│   ├── zip_engine.c            # ZIP decompression & RFC 1951 inflator
│   ├── memory_manager.c        # Thread-safe buffer allocator & safety checks
│   ├── video_decoder.c         # AVIO in-memory stream reader & seek callbacks
│   ├── audio_player.c          # Audio playback & volume scaling
│   ├── player_controller.c     # Playback state, A/V clock sync, and workers
│   ├── playlist.c              # Multi-video playlist implementation
│   ├── ui.c                    # Custom dark theme UI & font glyph renderer
│   └── threading.c             # Win32 / POSIX thread implementation
├── tests/
│   ├── test_memory.c           # Memory boundary & allocation tests
│   ├── test_zip.c              # Archive detection & corruption safety tests
│   ├── test_playlist.c         # Playlist indexing & navigation tests
│   ├── test_playback_pipeline.c# Full end-to-end in-memory playback test suite
│   └── data/                   # Synthetic test archives (single, multi, corrupt)
├── deps/                       # Pre-configured SDL2 & FFmpeg development libraries
└── docs/
    └── ARCHITECTURE.md         # In-depth architectural & data-flow documentation
```

---

## 4. How to Build & Run

### Prerequisites
* Windows 10/11 (or Linux)
* MinGW-w64 GCC (or Clang / MSVC)
* CMake (3.20+) and Ninja (or MinGW Makefiles)

*(Note: Pre-packaged Windows x64 SDL2 and FFmpeg development libraries and headers are already included in `deps/`)*

### Quick Build (Windows)
Run the automated build script:
```cmd
build.bat
```
This will:
1. Create the `build/` directory.
2. Configure the project with CMake and Ninja.
3. Compile `DirectZip.exe` and test executables.
4. Automatically copy all runtime DLLs next to the executable.
5. Run the complete automated test suite (`ctest`).

### Quick Launch
To launch the player with the bundled multi-video sample archive:
```cmd
run.bat
```
Or open a specific ZIP file directly:
```cmd
run.bat path\to\archive.zip
```

---

## 5. User Interface & Controls

| Action | Control |
|---|---|
| **Open ZIP** | Click `[+] Open ZIP` on top bar, drag & drop `.zip`, or pass via CLI |
| **Play / Pause** | Spacebar or click `[ > ]` / `[ \|\| ]` button |
| **Seek -5s / +5s** | Left / Right Arrow keys |
| **Seek -10s / +10s** | Click `[-10]` / `[+10]` buttons |
| **Timeline Scrub** | Click or drag anywhere along the timeline progress bar |
| **Previous Video** | `P` key or click `[ \|< ]` button |
| **Next Video** | `N` key or click `[ >\| ]` button |
| **Volume Up / Down** | Up / Down Arrow keys or drag the volume slider |
| **Mute / Unmute** | `M` key or click `VOL`/`MUT` |
| **Fullscreen** | `F` / `F11` key or click `[ ]` button (`ESC` to exit) |

---

## 6. Verification and Test Results

DirectZip includes an automated unit and integration test suite:

```cmd
ctest --output-on-failure
```

### Verified Test Cases:
1. **Corrupted ZIP Safety:** Gracefully detects and rejects malformed ZIP headers without crashing.
2. **Non-MP4 Filtering:** Successfully ignores text files, PDFs, and images inside archives, reporting 0 videos when no MP4 files are found.
3. **In-Memory Decompression:** Validates direct RFC 1951 Deflate and Stored decompression into RAM buffers.
4. **Custom AVIO Streaming:** Decodes H.264 video frames and AAC audio frames directly from RAM without disk access.
5. **Adaptive Seeking:** Verifies random-access seeking within memory buffers and decoder buffer flushing.
6. **Multi-Video Navigation:** Tests dynamic switching between multiple MP4 entries and videos without audio.
7. **Zero-Disk-Extraction Assertion:** Confirms that no intermediate `.mp4` files are written to local storage at any point during playback.
