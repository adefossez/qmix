#ifndef _MIXER_HPP_
#define _MIXER_HPP_

#include <atomic>
#include <random>
#include <thread>
#include <vector>

#include <folly/ProducerConsumerQueue.h>

#include <sndfile.hh>
#include <soundio/soundio.h>

#include "qmix.hpp"


class Seeker {
public:
    Seeker(const std::string& path);
    void get_samples(
        double speed,
        std::vector<float>& buffer,
        size_t n_samples);
    void noplay(size_t n_samples);
    const SndfileHandle& file() const;
    void reset();
private:
    SndfileHandle file_;
    std::vector<std::vector<float>> song_;
    int channels_;
    int sample_rate_;
    size_t num_samples_;
    double duration_;
    double read_speed_;
    double frame_duration_;

    double position_{0};

    std::default_random_engine generator_;

    static const double MAX_SPEED;
    static const double SPEED_PROBA;
    static const double SPEED_THRESHOLD;
};

class Mixer {
public:
    Mixer(SoundIoDevice* device);
    virtual ~Mixer();
    void push_camera_state(std::vector<QRSong>&& songs);

private:
    void do_mixer_thread();
    static void static_soundio_callback(
        SoundIoOutStream* stream,
        int frame_count_min,
        int frame_count_max);
    void soundio_callback(
        SoundIoOutStream* stream,
        int frame_count_min,
        int frame_count_max);

    SoundIoOutStream* stream_;
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