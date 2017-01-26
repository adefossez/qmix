#include "qmix.hpp"

#include <chrono>
#include <memory>
#include <iostream>
#include <string>

#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <folly/ScopeGuard.h>

#include <portaudio.h>

#include "mixer.hpp"
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
    std::vector<cv::Point> approx;
    double peri;

    ShapeInfo() : shape(Shape::OTHER), center(0, 0) {}
};

ShapeInfo get_shape(const std::vector<cv::Point>& contour) {
    ShapeInfo result;
    result.peri = cv::arcLength(contour, true);
    cv::approxPolyDP(contour, result.approx, 0.04 * result.peri, true);
    if (result.approx.size() == 3 || result.approx.size() == 4) {
        for (const auto& i : result.approx) {
            result.center.x += i.x;
            result.center.y += i.y;
        }
        result.center.x /= result.approx.size();
        result.center.y /= result.approx.size();
        if (result.approx.size() == 3) {
            result.size = result.peri / 3 * std::sqrt(3) / 2;
            result.shape = Shape::TRIANGLE;
        } else {
            result.shape = Shape::SQUARE;
            result.size = result.peri / 4;
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
            // if (hier[child][0] != -1 || hier[child][1] != -1) {
            //     return -1;
            // }
            while (sub_result < 0 && child >= 0) {
                sub_result = try_decode(contours, hier, child, nullptr, level + 1);
                int sub_result_toplevel = try_decode(contours, hier, child, nullptr, 0);
                if (sub_result_toplevel != -1) {
                    return -1;
                }
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

void find_songs(const cv::Mat& image, std::vector<QRSong>* songs) {
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hier;
    cv::findContours(image, contours, hier, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    // cv::Mat drawing = cv::Mat::zeros(image.size(), CV_8UC1);
    for(size_t i = 0; i< contours.size(); i++) {
        ShapeInfo info;
        auto result = try_decode(contours, hier, i, &info);
        if (result >= 0) {
            if (result >= songs->size()) {
                dbg("Invalid id", result);
            }
            // cv::Scalar color(255);
            // cv::Scalar color_approx(128);
            // std::vector<std::vector<cv::Point>> tmp_contours;
            // std::vector<std::vector<cv::Point>> tmp_contours_approx;
            // tmp_contours.push_back(info.approx);
            // int idx = i;
            // while(idx >= 0) {
            //     tmp_contours.push_back(contours[idx]);
            //     idx = hier[idx][2];
            // }
            // cv::drawContours(drawing, tmp_contours, 0, color, 2);
            // cv::drawContours(drawing, tmp_contours_approx, 0, color_approx, 2);
            QRSong song;
            song.center.x = static_cast<double>(info.center.x) / image.cols;
            song.center.y = static_cast<double>(info.center.y) / image.rows;
            song.size = info.size / (image.cols * image.rows);
            song.active = true;
            (*songs)[result] = song;
        }
    }
}

int main(int argc, char** argv) {
    SoundIo *soundio = soundio_create();
    assert(soundio);
    auto guard = folly::makeGuard([&](){
        soundio_destroy(soundio);
    });
    call_sio(soundio_connect, soundio);
    soundio_flush_events(soundio);
    auto n_devices = soundio_output_device_count(soundio);
    SoundIoDevice* device = nullptr;
    int device_idx_config = argc >= 2 ? std::stoi(argv[1]) : soundio_default_output_device_index(soundio);
    for (int device_idx=0; device_idx < n_devices; ++device_idx) {
        SoundIoDevice* tmp_device;
        tmp_device = soundio_get_output_device(soundio, device_idx);
        assert(tmp_device);
        std::cout << "Device " << device_idx << " : " << device;
        if (device_idx == device_idx_config) {
            device = tmp_device;
            std::cout << " [selected]";
        } else {
            soundio_device_unref(tmp_device);
        }
        std::cout << std::endl;
    }
    if (device == nullptr) {
        std::cerr << "Invalid device idx" << std::endl;
        return 1;
    }
    
    auto device_guard = folly::makeGuard([&](){
        soundio_device_unref(device);
    });
    Mixer mixer(device);
    cv::VideoCapture capture(0);

    cv::namedWindow("tmp");
    //cv::namedWindow("tmp2");
    folly::ProducerConsumerQueue<cv::Mat> queue(2);
    std::thread show_thread([&]() {
        auto begin = std::chrono::steady_clock::now();
        int i = 0;
        for(;;) {
            cv::Mat color, gray, flipped, blurred, thresh;
            capture >> color;
            cv::cvtColor(color, gray, CV_BGR2GRAY);
            cv::flip(gray, flipped, 1);
            cv::GaussianBlur(flipped, blurred, cv::Size(15, 15), 0);
            cv::threshold(blurred, thresh, 100, 255, cv::THRESH_BINARY);
            queue.write(thresh);
            auto duration = std::chrono::steady_clock::now() - begin;
            auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
            ++i;
            //std::cout << "FPS " << (i * 1000 / msec) << std::endl;

            std::vector<QRSong> songs(7);
            find_songs(thresh, &songs);
            mixer.push_camera_state(std::move(songs));
            // std::cout << songs.size() << std::endl;
        }
    });
    
    while(true) {
        cv::Mat image;
        if(queue.read(image)) {
            cv::imshow("tmp", image);
        }
        soundio_flush_events(soundio);
        cv::waitKey(1);
    }
    return 0;
}