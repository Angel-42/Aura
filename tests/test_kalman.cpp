#include <gtest/gtest.h>
#include "Aura/Core/KalmanFilter.hpp"

using namespace Aura::Core;

TEST(KalmanFilter2D, PredictUninitializedReturnsZero) {
    KalmanFilter2D kf;
    cv::Point2f p = kf.predict();
    EXPECT_FLOAT_EQ(p.x, 0.f);
    EXPECT_FLOAT_EQ(p.y, 0.f);
}

TEST(KalmanFilter2D, FirstUpdateInitializesAtMeasurement) {
    KalmanFilter2D kf;
    cv::Point2f result = kf.update({100.f, 200.f});
    // First update = initialization, output should be at or near measurement
    EXPECT_NEAR(result.x, 100.f, 5.f);
    EXPECT_NEAR(result.y, 200.f, 5.f);
}

TEST(KalmanFilter2D, ConvergesOnRepeatedMeasurement) {
    KalmanFilter2D kf;
    cv::Point2f target{150.f, 300.f};
    cv::Point2f result;
    for (int i = 0; i < 60; ++i)
        result = kf.update(target);
    EXPECT_NEAR(result.x, target.x, 2.f);
    EXPECT_NEAR(result.y, target.y, 2.f);
}

TEST(KalmanFilter2D, SmoothsSingleNoisySpike) {
    KalmanFilter2D kf;
    // Stabilise at (50, 50)
    for (int i = 0; i < 20; ++i) kf.update({50.f, 50.f});
    // Single noisy spike — the filter dampens it but doesn't block it entirely.
    // With default params (processNoise=1e-2, measureNoise=1e-1) the output
    // should be clearly below the midpoint 275 = (50+500)/2.
    cv::Point2f noisy = kf.update({500.f, 500.f});
    EXPECT_LT(noisy.x, 350.f);
    EXPECT_LT(noisy.y, 350.f);
    // And should not be at the spike position
    EXPECT_GT(noisy.x, 50.f);
}

TEST(KalmanFilter2D, PredictAfterUpdateIsCloseToLastKnown) {
    KalmanFilter2D kf;
    for (int i = 0; i < 20; ++i) kf.update({80.f, 160.f});
    cv::Point2f pred = kf.predict();
    EXPECT_NEAR(pred.x, 80.f, 10.f);
    EXPECT_NEAR(pred.y, 160.f, 10.f);
}

TEST(KalmanFilter2D, TracksSteadyMovement) {
    KalmanFilter2D kf;
    cv::Point2f result;
    // Déplacement linéaire de (0,0) à (100,0)
    for (int i = 0; i <= 50; ++i)
        result = kf.update({static_cast<float>(i * 2), 0.f});
    // À la fin le filtre doit être proche de 100
    EXPECT_NEAR(result.x, 100.f, 15.f);
}
