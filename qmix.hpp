#ifndef _QMIX_HPP_
#define _QMIX_HPP_

#include <array>
#include <memory>
#include <vector>

#include <opencv2/core.hpp>

struct Frame {
    std::array<std::shared_ptr<cv::Mat>, 3> cFrames;
    std::shared_ptr<cv::Mat> gFrame;
    int64_t msec;

    Frame() :gFrame(new cv::Mat()) {
        for (int i=0; i < cFrames.size(); ++i) {
            cFrames[i].reset(new cv::Mat());
        }
    };
};


struct QRSong {
    int id;
    double delay;
    double volume;
};

#endif