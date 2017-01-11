#include "qmix.hpp"

#include <chrono>
#include <memory>
#include <iostream>
#include <string>

#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <zbar.h>

void find_songs(Frame frame, int channel, std::vector<QRSong>* songs) {
    cv::Mat blurred, thresh;
    //cv::GaussianBlur(*frame.gFrame, blurred, cv::Size(15, 15), 0);
    cv::GaussianBlur(*frame.gFrame, blurred, cv::Size(15, 15), 0);
    cv::threshold(blurred, thresh, 90, 255, cv::THRESH_BINARY);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    cv::Mat drawing = cv::Mat::zeros(thresh.size(), CV_8UC1);
    for(size_t i = 0; i< contours.size(); i++) {
        double peri = cv::arcLength(contours[i], true);
        std::vector<cv::Point> approx;
        cv::approxPolyDP(contours[i], approx, 0.04 * peri, true);
        if (approx.size() == 3 || approx.size() == 4) {
            QRSong song;
            song.id = approx.size() - 3 + 2 * channel;
            songs->push_back(song);
            cv::Scalar color(255);
            std::vector<std::vector<cv::Point>> tmp_contours;
            tmp_contours.push_back(approx);
            cv::drawContours(drawing, tmp_contours, 0, color, 2);
        }
    }

    cv::imshow("tmp", thresh);
    cv::imshow("tmp2", drawing);
    cv::waitKey(1000);
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
    cv::namedWindow("tmp2");

    auto begin = std::chrono::steady_clock::now();
    for(;;) {
        Frame frame;
        cv::Mat gTmp, cTmp;
        capture >> cTmp;
        cv::cvtColor(cTmp, gTmp, CV_BGR2GRAY);
        for (int i=0; i < frame.cFrames.size(); ++i) {
            cv::Mat tmp;
            cv::extractChannel(cTmp, tmp, i);
            //*frame.cFrames[i] = std::move(tmp);
            cv::equalizeHist(tmp, *frame.cFrames[i]);
        }
        cv::equalizeHist(gTmp, *frame.gFrame);
        //*frame.gFrame = std::move(gTmp);
        // cv::imshow("tmp", *frame.gFrame);
        // cv::waitKey(1);
        auto duration = std::chrono::steady_clock::now() - begin;
        frame.msec = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        std::vector<QRSong> songs;
        find_songs(frame, 0, &songs);
        // std::cout << songs.size() << std::endl;
    }
    return 0;
}