#ifndef _QMIX_HPP_
#define _QMIX_HPP_

#include <memory>
#include <vector>

#include <opencv2/core.hpp>

struct Frame {
    std::shared_ptr<cv::Mat> frame;
    int64_t msec;

    Frame() : frame(new cv::Mat()) {};
};


struct QRSong {
    int id;
    double delay;
    double volume;
};

std::vector<QRSong> find_songs(Frame frame);

#endif