#pragma once
#include <opencv2/opencv.hpp>

namespace Aura::Vision {

// Capture vidéo seule. UI et calibration sont dans des classes dédiées.
class Camera {
public:
    explicit Camera(int device = 0);
    ~Camera();

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    [[nodiscard]] bool isOpened() const noexcept;
    bool captureFrame(cv::Mat& frame);
    void release();

private:
    cv::VideoCapture cap_;
};

} // namespace Aura::Vision
