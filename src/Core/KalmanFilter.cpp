#include "Aura/Core/KalmanFilter.hpp"

namespace Aura {
namespace Core {

KalmanFilter2D::KalmanFilter2D(float processNoise, float measurementNoise)
    : filter_(4, 2, 0),
      state_(4, 1, CV_32F),
      measurement_(2, 1, CV_32F),
      initialized_(false) {
    filter_.transitionMatrix = (cv::Mat_<float>(4, 4) <<
        1, 0, 1, 0,
        0, 1, 0, 1,
        0, 0, 1, 0,
        0, 0, 0, 1);

    filter_.measurementMatrix = cv::Mat::zeros(2, 4, CV_32F);
    filter_.measurementMatrix.at<float>(0, 0) = 1;
    filter_.measurementMatrix.at<float>(1, 1) = 1;

    cv::setIdentity(filter_.processNoiseCov, cv::Scalar(processNoise));
    cv::setIdentity(filter_.measurementNoiseCov, cv::Scalar(measurementNoise));
    cv::setIdentity(filter_.errorCovPost, cv::Scalar::all(1));
}

cv::Point2f KalmanFilter2D::update(const cv::Point2f& measurement) {
    if (!initialized_) {
        state_.at<float>(0) = measurement.x;
        state_.at<float>(1) = measurement.y;
        state_.at<float>(2) = 0;
        state_.at<float>(3) = 0;
        filter_.statePost = state_.clone();
        initialized_ = true;
    }

    filter_.predict();
    measurement_.at<float>(0) = measurement.x;
    measurement_.at<float>(1) = measurement.y;
    cv::Mat estimated = filter_.correct(measurement_);
    return cv::Point2f(estimated.at<float>(0), estimated.at<float>(1));
}

cv::Point2f KalmanFilter2D::predict() {
    if (!initialized_) {
        return cv::Point2f(0.f, 0.f);
    }

    cv::Mat prediction = filter_.predict();
    return cv::Point2f(prediction.at<float>(0), prediction.at<float>(1));
}

} // namespace Core
} // namespace Aura
