#ifndef _QMIX_HPP_
#define _QMIX_HPP_

#include <array>
#include <memory>
#include <vector>

#include <opencv2/core.hpp>

struct Frame {
    std::shared_ptr<cv::Mat> gFrame;
    int64_t msec;

    Frame() :gFrame(new cv::Mat()) {}
};


struct QRSong {
    double delay {0};
    double volume {0};
    cv::Point2d center {0,0};
    double size {0};
    bool active {false};
};

#endif