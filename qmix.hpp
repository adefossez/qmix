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
    int id;
    double delay;
    double volume;
    cv::Point2d center;
    double size;
};

#endif