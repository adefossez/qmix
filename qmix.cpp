#include "qmix.hpp"

#include <chrono>
#include <memory>
#include <iostream>
#include <string>

#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <zbar.h>

std::vector<QRSong> find_songs(Frame frame) {
    std::vector<QRSong> results;
    // zbar::ImageScanner scanner;
    // scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 1);

    // int width = frame.frame->cols;
    // int height = frame.frame->rows;
    // uint8_t *raw = (uchar *)(frame.frame->data);
    // zbar::Image image(width, height, "Y800", raw, width * height);
    // scanner.scan(image);

    // for(auto symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol) {
    //     QRSong song;
    //     song.id = std::stoi(symbol->get_data());
    //     std::cout << "found track " << song.id << std::endl;
    //     results.push_back(std::move(song));
    // }

    return results;
}

int main(int argc, char** argv) {
    cv::VideoCapture capture(0);
    double fps = capture.get(cv::CAP_PROP_FPS);
    double width = capture.get(cv::CAP_PROP_FRAME_WIDTH);
    double height = capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    std::cout << fps << std::endl;
    std::cout << width << std::endl;
    std::cout << height << std::endl;

    cv::namedWindow("tmp");

    auto begin = std::chrono::steady_clock::now();
    for(;;) {
        cv::Mat color_frame;
        Frame frame;
        capture >> color_frame;
        cv::cvtColor(color_frame, *frame.frame, CV_BGR2GRAY);
        cv::imshow("tmp", *frame.frame);
        cv::waitKey(0);
        auto duration = std::chrono::steady_clock::now() - begin;
        frame.msec = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        auto r = find_songs(std::move(frame));
        std::cout << r.size() << std::endl;
    }
    return 0;
}