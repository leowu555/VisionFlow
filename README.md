# VisionFlow

VisionFlow is a real-time, distributed computer vision platform written primarily in modern C++.
The long-term goal is a system that ingests video from many sources, runs vision workloads on the
frames, and distributes that work across processes and machines with low latency.

The project is built incrementally, one phase at a time. Each phase leaves the repository in a
working state.

## Phase 1 — video ingestion foundation

Phase 1 implements a single-process, single-threaded video viewer:

- `visionflow::Frame` (`include/visionflow/frame.hpp`): a frame ID, a `std::chrono::steady_clock`
  capture timestamp, and the `cv::Mat` image.
- `visionflow::VideoSource` (`include/visionflow/video_source.hpp`, `src/video_source.cpp`): an
  RAII wrapper around `cv::VideoCapture` that opens a webcam or video file, reads frames, exposes
  the reported FPS, width, and height, and releases the source on destruction.
- `src/main.cpp`: opens a source, reads frames in a loop, stamps each with a monotonically
  increasing ID and a timestamp, displays it with an overlay of the frame ID and observed FPS, and
  exits cleanly on `q`, on window close, or at end of stream.

## Phase 2 — thread-safe bounded queue

`visionflow::BoundedQueue<T>` (`include/visionflow/bounded_queue.hpp`) is a header-only,
fixed-capacity FIFO queue built on `std::mutex`, `std::condition_variable`, and `std::deque`. It is
the building block later phases will use to hand `Frame` objects from a capture thread to
processing threads. The video pipeline itself is still single-threaded; nothing uses the queue yet.

**Why bounded?** Capture and processing run at different speeds. An unbounded queue between them
grows without limit whenever processing falls behind, consuming memory and adding latency.

**Backpressure** is what a bounded queue gives you instead: when the queue is full, `push()` blocks
the producer until a consumer makes room, so a slow consumer automatically slows the producer down.
`try_push()` is the non-blocking alternative: it returns `false` when full, letting the caller
decide what to do (for example, drop the frame).

**`close()`** marks the end of the stream. After it is called:

- no new items are accepted (`push()` and `try_push()` return `false`);
- every thread blocked in `push()` or `pop()` wakes up;
- items already queued can still be drained, and `pop()` returns `false` only once the queue is
  both closed and empty.

Calling `close()` more than once is harmless.

Items are moved in and out, so move-only types and `Frame` (whose `cv::Mat` pixels are shared, not
copied) work without deep copies. If a push fails, the item passed to it is discarded.

### Running the tests

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Or run the test binary directly for per-test output: `./build/tests/test_bounded_queue`.
Tests are built by default; pass `-DVISIONFLOW_BUILD_TESTS=OFF` to skip them.

There is no machine learning, networking, or persistence yet.

## Project layout

```
CMakeLists.txt
include/visionflow/   Public headers
src/                  Library sources and the application entry point
tests/                Test executables (registered with CTest)
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
`-DOpenCV_DIR=$(brew --prefix opencv)/lib/cmake/opencv5` (or `opencv4` for OpenCV 4.x).

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
