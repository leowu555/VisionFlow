#include "visionflow/video_source.hpp"

#include <cmath>
#include <stdexcept>

namespace visionflow {

VideoSource::VideoSource(int deviceIndex)
    : description_("camera " + std::to_string(deviceIndex)), isFile_(false) {
    if (deviceIndex < 0) {
        throw std::invalid_argument("Camera index must be non-negative, got " +
                                    std::to_string(deviceIndex));
    }
    if (!capture_.open(deviceIndex)) {
        throw std::runtime_error("Failed to open " + description_ +
                                 " (is it connected and is camera access permitted?)");
    }
}

VideoSource::VideoSource(const std::string& filePath)
    : description_("file '" + filePath + "'"), isFile_(true) {
    if (filePath.empty()) {
        throw std::invalid_argument("Video file path must not be empty");
    }
    if (!capture_.open(filePath)) {
        throw std::runtime_error("Failed to open " + description_ +
                                 " (does it exist and is the format supported?)");
    }
}

VideoSource::~VideoSource() {
    release();
}

bool VideoSource::read(cv::Mat& out) {
    if (!capture_.isOpened()) {
        return false;
    }
    return capture_.read(out) && !out.empty();
}

double VideoSource::fps() const {
    const double value = capture_.get(cv::CAP_PROP_FPS);
    return (std::isfinite(value) && value > 0.0) ? value : 0.0;
}

int VideoSource::width() const {
    return static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_WIDTH));
}

int VideoSource::height() const {
    return static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_HEIGHT));
}

bool VideoSource::isOpened() const {
    return capture_.isOpened();
}

void VideoSource::release() {
    if (capture_.isOpened()) {
        capture_.release();
    }
}

}  // namespace visionflow
