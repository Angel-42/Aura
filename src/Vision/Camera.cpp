#include "Aura/Vision/Camera.hpp"
#include <iostream>

namespace Aura::Vision {

Camera::Camera(int device) : cap_(device) {
    if (!cap_.isOpened()) {
        std::cerr << "[Camera] Failed to open device " << device << "\n";
    }
}

Camera::~Camera() {
    release();
}

bool Camera::isOpened() const noexcept {
    return cap_.isOpened();
}

bool Camera::captureFrame(cv::Mat& frame) {
    cap_ >> frame;
    return !frame.empty();
}

void Camera::release() {
    if (cap_.isOpened()) {
        cap_.release();
    }
}

} // namespace Aura::Vision
