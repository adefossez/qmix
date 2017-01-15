#ifndef _MIXER_HPP_
#define _MIXER_HPP_

#include <atomic>
#include <thread>
#include <vector>

#include <folly/ProducerConsumerQueue.h>

#include <sndfile.hh>
#include <portaudio.h>

#include "qmix.hpp"


class Seeker {
public:
    Seeker(const std::string& path);
    void get_samples(
        uint64_t time, 
        double target,
        std::vector<float>& buffer,
        size_t n_samples);
    const SndfileHandle& file();
    void reset();
private:
    SndfileHandle file_;
    std::vector<std::vector<float>> song_;
    double current_ {0};
    int channels_;
    int sample_rate_;
    size_t num_samples_;
    double duration_;
    double read_speed_;
    double frame_duration_;

    bool first_ {true};
    size_t position_{0};

    static const double MAX_SPEED;
    static const double SPEED_THRESHOLD;
};

class Mixer {
public:
    Mixer();
    virtual ~Mixer();
    void push_camera_state(std::vector<QRSong>&& songs);

private:
    void do_mixer_thread();
    static int pa_static_callback(
        const void* input_buffer,
        void* output_buffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void* user_data);
    int pa_callback(
        const void* input_buffer,
        void* output_buffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags);

    PaStream* stream_;
    folly::ProducerConsumerQueue<std::vector<float>> to_cb_;
    folly::ProducerConsumerQueue<std::vector<float>> from_cb_;
    folly::ProducerConsumerQueue<std::vector<QRSong>> from_camera_;
    std::vector<Seeker> files_;
    std::thread mixer_thread_;

    static const int FBP;
    static const int SAMPLE_RATE;
    static const int CHANNELS;
    static const int QUEUE_SIZE;
    static const int NUM_SONGS;
};

#endif