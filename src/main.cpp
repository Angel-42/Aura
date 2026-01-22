/**
 * @ Author: @Angel-42
 * @ Create Time: 2026-01-22 15:47:53
 * @ Modified by: Your name
 * @ Modified time: 2026-01-22 16:20:06
 * @ Description:
 */

#include <iostream>
#include <opencv2/opencv.hpp>

int main(void) {
    cv::VideoCapture cap(0);

    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera." << std::endl;
        return -1;
    }

    cv::Mat frame;
    std::cout << "Press 'q' to exit." << std::endl;
    while (true) {
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "Error: Could not read frame." << std::endl;
            break;
        }

        cv::imshow("Camera Feed", frame);

        if (cv::waitKey(30) == 'q') {
            break;
        }
    }
    cap.release();
    cv::destroyAllWindows();
    return 0;
}
