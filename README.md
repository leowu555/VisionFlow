# VisionFlow

VisionFlow is a real-time, distributed computer vision platform written primarily in modern C++.
The long-term goal is a system that ingests video from many sources, runs vision workloads on the
frames, and distributes that work across processes and machines with low latency.

The project is built incrementally, one phase at a time. Each phase leaves the repository in a
working state.

## Current status: Phase 1 — video ingestion foundation

Phase 1 implements a single-process, single-threaded video viewer:

- `visionflow::Frame` (`include/visionflow/frame.hpp`): a frame ID, a `std::chrono::steady_clock`
  capture timestamp, and the `cv::Mat` image.
- `visionflow::VideoSource` (`include/visionflow/video_source.hpp`, `src/video_source.cpp`): an
  RAII wrapper around `cv::VideoCapture` that opens a webcam or video file, reads frames, exposes
  the reported FPS, width, and height, and releases the source on destruction.
- `src/main.cpp`: opens a source, reads frames in a loop, stamps each with a monotonically
  increasing ID and a timestamp, displays it with an overlay of the frame ID and observed FPS, and
  exits cleanly on `q`, on window close, or at end of stream.

There is no machine learning, networking, threading, or persistence yet.

## Project layout

```
CMakeLists.txt
include/visionflow/   Public headers
src/                  Library sources and the application entry point
tests/                Reserved for tests (empty in Phase 1)
assets/               Local sample videos (video files here are git-ignored)
```

## Dependencies

- A C++20 compiler (Apple Clang 14+, GCC 10+, or MSVC 2019 16.10+)
- CMake 3.16+
- OpenCV 4.x or 5.x (verified with 5.1) with the `core`, `imgproc`, `videoio`, and `highgui` modules

macOS (Homebrew):

```sh
brew install cmake opencv
```

Ubuntu/Debian:

```sh
sudo apt install build-essential cmake libopencv-dev
```

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

If CMake cannot find OpenCV, point it at the install, e.g.
`-DOpenCV_DIR=$(brew --prefix opencv)/lib/cmake/opencv4`.

## Run

```sh
./build/visionflow                 # default webcam (index 0)
./build/visionflow 1               # webcam at index 1
./build/visionflow assets/demo.mp4 # video file
./build/visionflow --help
```

Press `q` in the video window to quit. Video files are played back at their native frame rate.

On macOS, the terminal app you launch from needs camera permission
(System Settings → Privacy & Security → Camera) to open a webcam.
