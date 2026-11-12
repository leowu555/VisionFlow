#pragma once

#include <string>

#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>

namespace visionflow {

// Owns an OpenCV capture device (webcam or video file).
//
// The underlying source is opened in the constructor and released in the
// destructor. Construction throws std::runtime_error if the source cannot be
// opened, so a constructed VideoSource is always usable until release().
class VideoSource {
public:
    explicit VideoSource(int deviceIndex);
    explicit VideoSource(const std::string& filePath);

    ~VideoSource();

    VideoSource(const VideoSource&) = delete;
    VideoSource& operator=(const VideoSource&) = delete;
    VideoSource(VideoSource&&) = delete;
    VideoSource& operator=(VideoSource&&) = delete;

    // Reads the next frame into `out`. Returns false when no frame is
    // available (end of a file, device disconnected, or source released).
    [[nodiscard]] bool read(cv::Mat& out);

    // FPS reported by the backend. May be 0 if the backend does not know it
    // (common for some webcams).
    [[nodiscard]] double fps() const;
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;

    [[nodiscard]] bool isOpened() const;
    [[nodiscard]] bool isFile() const noexcept { return isFile_; }
    [[nodiscard]] const std::string& description() const noexcept { return description_; }

    // Releases the underlying device/file. Safe to call more than once.
    void release();

private:
    cv::VideoCapture capture_;
    std::string description_;
    bool isFile_{false};
};

}  // namespace visionflow
