#include "qmix.hpp"

#include <chrono>
#include <memory>
#include <iostream>
#include <string>

#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <portaudio.h>

#include <SoundFileRead.h>

#include "utils.hpp"

enum class Shape {
    TRIANGLE, 
    SQUARE,
    OTHER
};

struct ShapeInfo {
    Shape shape;
    cv::Point center;
    double size {0};

    ShapeInfo() : shape(Shape::OTHER), center(0, 0) {}
};

ShapeInfo get_shape(const std::vector<cv::Point>& contour) {
    double peri = cv::arcLength(contour, true);
    std::vector<cv::Point> approx;
    cv::approxPolyDP(contour, approx, 0.1 * peri, true);
    ShapeInfo result;
    if (approx.size() == 3 || approx.size() == 4) {
        for (const auto& i : approx) {
            result.center.x += i.x;
            result.center.y += i.y;
        }
        result.center.x /= approx.size();
        result.center.y /= approx.size();
        result.size = cv::contourArea(contour);
        if (approx.size() == 3) {
            result.size *= std::sqrt(3);
            result.shape = Shape::TRIANGLE;
        } else {
            result.shape = Shape::SQUARE;
        }
    }
    return result;
}

int try_decode(const std::vector<std::vector<cv::Point>>& contours, const std::vector<cv::Vec4i>& hier, int i, ShapeInfo* info=nullptr, int level=0) {
    auto shape = get_shape(contours[i]);
    int result = 0;
    if (shape.shape == Shape::OTHER) {
        return -1;
    } else {
        int sub_result = 0;
        if (level < 2) {
            sub_result = -1;
            auto child = hier[i][2];
            while (sub_result < 0 && child >= 0) {
                sub_result = try_decode(contours, hier, child, nullptr, level + 1);
                child = hier[child][1];
            }
            if (sub_result < 0) {
                return -1;
            }
        }
        result = 2 * sub_result;
        if (shape.shape == Shape::TRIANGLE) {
            result += 1;
        }
    }
    if (result >= 0 && info) {
        *info = shape;
    }
    return result;
}

void find_songs(Frame frame, std::vector<QRSong>* songs) {
    cv::Mat blurred, thresh;
    cv::GaussianBlur(*frame.gFrame, blurred, cv::Size(15, 15), 0);
    cv::threshold(blurred, thresh, 70, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hier;
    cv::findContours(thresh, contours, hier, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    cv::Mat drawing = cv::Mat::zeros(thresh.size(), CV_8UC1);
    for(size_t i = 0; i< contours.size(); i++) {
        double peri = cv::arcLength(contours[i], true);
        std::vector<cv::Point> approx;
        cv::approxPolyDP(contours[i], approx, 0.1 * peri, true);
        if (approx.size() == 3 || approx.size() == 4) {
            cv::Scalar color(255);
            std::vector<std::vector<cv::Point>> tmp_contours;
            tmp_contours.push_back(approx);
            cv::drawContours(drawing, tmp_contours, 0, color, 2);
        }
        ShapeInfo info;
        auto result = try_decode(contours, hier, i, &info);
        if (result >= 0) {
            QRSong song;
            song.id = result;
            song.center.x = static_cast<double>(info.center.x) / frame.gFrame->cols;
            song.center.y = static_cast<double>(info.center.y) / frame.gFrame->rows;
            song.size = info.size / (frame.gFrame->cols * frame.gFrame->rows);
            std::cout << song.id << " "
                      << song.center.x << " "
                      << song.center.y << " "
                      << song.size
                      << frame.msec << std::endl;
        }
    }

    cv::imshow("tmp2", thresh);
    cv::imshow("tmp", drawing);
    cv::waitKey(1);
}

void play_cb(const void *input, void* output, unsigned long frameCount, 
             const PaStreamCallbackTimeInfo* timeInfo,
             PaStreamCallbackFlags statusFlags,
             void *userData) {
    auto file = reinterpret_cast<SoundFileRead*>(userData);
}

void play_song(int id) {
    std::string path = "songs/" + std::to_string(id) + ".wav";
    SoundFileRead* file = new SoundFileRead(path.c_str());
    dbg("Reading file", id, "num channels", file->getChannels());

    PaStream* stream;
    call_pa(
        Pa_OpenDefaultStream, &stream, 0, 2, 
        paInt16, 44100, 256, play_cb, static_cast<void*>(file));
}

int main(int argc, char** argv) {
    call_pa(Pa_Initialize);
    play_song(0);
    return 0;

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
        cv::equalizeHist(gTmp, *frame.gFrame);
        //*frame.gFrame = std::move(gTmp);
        //cv::imshow("tmp", *frame.gFrame);
        //cv::waitKey(1);
        auto duration = std::chrono::steady_clock::now() - begin;
        frame.msec = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        std::vector<QRSong> songs;
        find_songs(frame, &songs);
        // std::cout << songs.size() << std::endl;
    }
    return 0;
}