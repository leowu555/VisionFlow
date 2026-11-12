#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "visionflow/frame.hpp"
#include "visionflow/video_source.hpp"

namespace {

constexpr const char* kWindowName = "VisionFlow";
constexpr int kQuitKey = 'q';

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [camera-index | video-file]\n"
              << "  camera-index  Non-negative integer webcam index (default: 0)\n"
              << "  video-file    Path to a video file\n"
              << "Press 'q' in the video window to quit.\n";
}

bool isNonNegativeInteger(const std::string& text) {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); });
}

// Measures observed FPS over a rolling window of roughly one second.
class FpsCounter {
public:
    explicit FpsCounter(visionflow::Clock::duration window = std::chrono::seconds(1))
        : window_(window), windowStart_(visionflow::Clock::now()) {}

    void tick(visionflow::Clock::time_point now) {
        ++framesInWindow_;
        const auto elapsed = now - windowStart_;
        if (elapsed >= window_) {
            const double seconds = std::chrono::duration<double>(elapsed).count();
            fps_ = static_cast<double>(framesInWindow_) / seconds;
            framesInWindow_ = 0;
            windowStart_ = now;
        }
    }

    [[nodiscard]] double fps() const noexcept { return fps_; }

private:
    visionflow::Clock::duration window_;
    visionflow::Clock::time_point windowStart_;
    int framesInWindow_{0};
    double fps_{0.0};
};

void drawOverlay(cv::Mat& image, visionflow::FrameId id, double observedFps) {
    std::ostringstream text;
    text << "Frame " << id << " | FPS " << std::fixed << std::setprecision(1) << observedFps;

    const cv::Point origin{10, 30};
    cv::putText(image, text.str(), origin, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 4,
                cv::LINE_AA);
    cv::putText(image, text.str(), origin, cv::FONT_HERSHEY_SIMPLEX, 0.8,
                cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
}

// Destroys the OpenCV window on scope exit, including on exceptions.
struct WindowGuard {
    explicit WindowGuard(const std::string& name) : name_(name) {
        cv::namedWindow(name_, cv::WINDOW_AUTOSIZE);
    }
    ~WindowGuard() { cv::destroyWindow(name_); }
    WindowGuard(const WindowGuard&) = delete;
    WindowGuard& operator=(const WindowGuard&) = delete;

private:
    std::string name_;
};

std::unique_ptr<visionflow::VideoSource> openSource(int argc, char** argv) {
    if (argc < 2) {
        return std::make_unique<visionflow::VideoSource>(0);
    }
    const std::string arg = argv[1];
    if (isNonNegativeInteger(arg)) {
        return std::make_unique<visionflow::VideoSource>(std::stoi(arg));
    }
    return std::make_unique<visionflow::VideoSource>(arg);
}

int run(int argc, char** argv) {
    auto source = openSource(argc, argv);

    std::cout << "Opened " << source->description() << ": " << source->width() << "x"
              << source->height() << " @ " << source->fps() << " FPS (reported)\n";

    // Video files decode far faster than real time, so pace them to their
    // native frame rate. Cameras are naturally paced by the device.
    const bool paceToSourceFps = source->isFile() && source->fps() > 0.0;
    const auto targetFramePeriod =
        paceToSourceFps ? std::chrono::duration_cast<visionflow::Clock::duration>(
                              std::chrono::duration<double>(1.0 / source->fps()))
                        : visionflow::Clock::duration::zero();

    WindowGuard window(kWindowName);
    FpsCounter fpsCounter;
    visionflow::FrameId nextFrameId = 0;

    while (true) {
        visionflow::Frame frame;
        if (!source->read(frame.image)) {
            std::cout << "End of stream after " << nextFrameId << " frames.\n";
            break;
        }
        frame.id = nextFrameId++;
        frame.timestamp = visionflow::Clock::now();

        fpsCounter.tick(frame.timestamp);

        cv::Mat display = frame.image.clone();
        drawOverlay(display, frame.id, fpsCounter.fps());
        cv::imshow(kWindowName, display);

        int delayMs = 1;
        if (paceToSourceFps) {
            const auto spent = visionflow::Clock::now() - frame.timestamp;
            const auto remaining = targetFramePeriod - spent;
            delayMs = std::max(
                1, static_cast<int>(
                       std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count()));
        }

        const int key = cv::waitKey(delayMs);
        if ((key & 0xFF) == kQuitKey) {
            std::cout << "Quit requested after " << nextFrameId << " frames.\n";
            break;
        }
        if (cv::getWindowProperty(kWindowName, cv::WND_PROP_VISIBLE) < 1.0) {
            std::cout << "Window closed after " << nextFrameId << " frames.\n";
            break;
        }
    }

    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 2) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        const std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return EXIT_SUCCESS;
        }
    }

    try {
        return run(argc, argv);
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error: " << e.what() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }
    return EXIT_FAILURE;
}
