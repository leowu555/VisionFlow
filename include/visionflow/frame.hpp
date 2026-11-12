#pragma once

#include <chrono>
#include <cstdint>

#include <opencv2/core/mat.hpp>

namespace visionflow {

using Clock = std::chrono::steady_clock;
using FrameId = std::uint64_t;

// A single captured video frame plus the metadata needed to track it.
struct Frame {
    FrameId id{0};
    Clock::time_point timestamp{};
    cv::Mat image;
};

}  // namespace visionflow
