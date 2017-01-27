#include "qmix.hpp"

#include <chrono>
#include <memory>
#include <iostream>
#include <string>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <folly/Format.h>
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
    double approxPeri;

    ShapeInfo() : shape(Shape::OTHER), center(0, 0) {}
};

const double PERI_ALLOWANCE = 0.02;

ShapeInfo get_shape(const std::vector<cv::Point>& contour) {
    ShapeInfo result;
    result.peri = cv::arcLength(contour, true);
    cv::approxPolyDP(contour, result.approx, PERI_ALLOWANCE * result.peri, true);
    result.approxPeri = cv::arcLength(result.approx, true);
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
    if (info) {
        *info = shape;
    }
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
    return result;
}

void find_songs(const cv::Mat& image, std::vector<QRSong>* songs, cv::Mat* outImage) {
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hier;
    cv::findContours(image, contours, hier, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    assert(songs);
    assert(outImage);

    cv::cvtColor(image, *outImage, cv::COLOR_GRAY2RGBA);

    std::vector<std::vector<cv::Point>> tmpContours(1);

    // cv::Mat drawing = cv::Mat::zeros(image.size(), CV_8UC1);
    for(size_t i = 0; i< contours.size(); i++) {
        ShapeInfo info;
        auto result = try_decode(contours, hier, i, &info);
        if (info.shape == Shape::TRIANGLE || info.shape == Shape::SQUARE){
            //double delta = std::abs(info.peri - info.approxPeri) / info.peri / PERI_ALLOWANCE;
            double scale = 1;
            cv::Scalar validColor(0, 255 * scale, 0, 255);
            cv::Scalar shapeColor(0, 0, 255 * scale, 255);
            cv::Scalar approxColor(255, 0, 0, 160);
            auto color = result >= 0 ? validColor : shapeColor;
            tmpContours[0] = info.approx;
            drawContours(*outImage, contours, i, color, 2);
            drawContours(*outImage, tmpContours, 0, approxColor, 2);
            if (result >= 0) {
                
            } else {
                tmpContours[0] = info.approx;
            }
        }
        if (result >= 0) {
            if (result >= songs->size()) {
                dbg("Invalid id", result);
            }
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
    call_pa(Pa_Initialize);
    Mixer mixer;
    cv::VideoCapture capture(0);
    int width = capture.get(cv::CAP_PROP_FRAME_WIDTH);
    int height = capture.get(cv::CAP_PROP_FRAME_HEIGHT);
    std::cout << folly::format("Camera {}x{}\n", width, height);

    auto modes = sf::VideoMode::getFullscreenModes();
    for (const auto& m: modes) {
        std::cout << folly::format("Mode {}x{}\n", m.width, m.height);
    }
    sf::VideoMode mode = modes[0];
    double scale = std::min(static_cast<double>(mode.width) / width, 
                            static_cast<double>(mode.height) / height);
    sf::Vector2f location((mode.width - width * scale) / 2, 
                         (mode.height - height * scale) / 2);
    dbg("Scale", scale, "location", location.x, location.y);
    sf::RenderWindow window(mode, "QMIX", sf::Style::Fullscreen);
    window.setVerticalSyncEnabled(true);

    sf::Font font;
    assert(font.loadFromFile("font.ttf"));

    folly::ProducerConsumerQueue<cv::Mat> queue(2);
    std::atomic<bool> stop(false);
    std::thread show_thread([&]() {
        //auto begin = std::chrono::steady_clock::now();
        int i = 0;
        while(!stop) {
            cv::Mat color, gray, flipped, blurred, thresh;
            capture >> color;
            cv::cvtColor(color, gray, CV_BGR2GRAY);
            cv::flip(gray, flipped, 1);
            cv::GaussianBlur(flipped, blurred, cv::Size(15, 15), 0);
            cv::threshold(blurred, thresh, 100, 255, cv::THRESH_BINARY);
            //queue.write(thresh);
            //auto duration = std::chrono::steady_clock::now() - begin;
            //auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
            ++i;
            //std::cout << "FPS " << (i * 1000 / msec) << std::endl;

            std::vector<QRSong> songs(7);
            cv::Mat drawing;
            find_songs(thresh, &songs, &drawing);
            mixer.push_camera_state(std::move(songs));
            queue.write(std::move(drawing));
            // std::cout << songs.size() << std::endl;
        }
        dbg("Camera thread done");
    });
    
    cv::Mat image, sfmlMat;
    sf::Image sfmlImage;
    sf::Texture texture;
    sf::Sprite sprite;
    std::deque<int64_t> durations;
    auto begin = std::chrono::steady_clock::now();
    while(window.isOpen()) {
        sf::Event event;
        while(window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) {
                    window.close();
                }
            }
        }
        if(queue.read(image)) {
            auto duration = std::chrono::steady_clock::now() - begin;
            auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
            durations.push_back(msec);
            //cv::cvtColor(image, sfmlMat, cv::COLOR_GRAY2RGBA);
            sfmlMat = std::move(image);
            sfmlImage.create(sfmlMat.cols, sfmlMat.rows, sfmlMat.ptr());
            assert(sfmlMat.cols == width && sfmlMat.rows == height);
            assert(texture.loadFromImage(sfmlImage));
            sprite.setTexture(texture);
            sprite.setPosition(location);
            sprite.setScale(scale, scale);
            window.clear(sf::Color::Black);
            window.draw(sprite);
            if (durations.size() == 10) {
                double delta_time = durations.back() - durations.front();
                int fps = delta_time > 0 ? 1000 * durations.size() / delta_time : 0;
                durations.pop_front();

                sf::Text text;
                text.setFont(font);
                text.setString(folly::sformat("FPS: {}", fps));
                text.setCharacterSize(24);
                text.setFillColor(sf::Color::Red);
                window.draw(text);
            }
            window.display();
        }
    }
    stop = true;
    show_thread.join();
    return 0;
}