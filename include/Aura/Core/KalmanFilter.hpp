#ifndef AURA_CORE_KALMANFILTER_HPP
#define AURA_CORE_KALMANFILTER_HPP

#include <opencv2/opencv.hpp>

namespace Aura {
namespace Core {

class KalmanFilter2D {
public:
    KalmanFilter2D(float processNoise = 1e-2f, float measurementNoise = 1e-1f);
    cv::Point2f update(const cv::Point2f& measurement);
    cv::Point2f predict();

private:
    cv::KalmanFilter filter_;
    cv::Mat state_;
    cv::Mat measurement_;
    bool initialized_;
};

} // namespace Core
} // namespace Aura

#endif // AURA_CORE_KALMANFILTER_HPP
