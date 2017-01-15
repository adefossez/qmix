#include "mixer.hpp"

#include <cassert>
#include <cmath>
#include <chrono>
#include <string>

#include "utils.hpp"

const int Mixer::FBP = 128;
const int Mixer::SAMPLE_RATE = 44100;
const int Mixer::CHANNELS = 2;
const int Mixer::QUEUE_SIZE = 20;
const int Mixer::NUM_SONGS = 7;

const double Seeker::MAX_SPEED = 2;
const double Seeker::SPEED_THRESHOLD = 20;

const double MAX_SIZE = 0.0005;
const double MIN_SIZE = 0.000138;

double volume_for_size(double size) {
    size = std::max(std::min(size, MAX_SIZE), MIN_SIZE);
    return (size - MIN_SIZE) / (MAX_SIZE - MIN_SIZE);
}

float inline interpolate(std::vector<float>& samples, double location) {
    size_t a, b;
    assert(location >= 0 && location < 1);
    size_t size = samples.size();
    a = std::floor(location * size);
    b = a == size - 1 ? 0 : a + 1;
    double w = location - static_cast<double>(a) / size;
    return samples[a];// * (1 - w) + samples[b] * w;
}

Seeker::Seeker(const std::string& path) : file_(path.c_str()) {
    std::vector<float> tmp;
    channels_ = file_.channels();
    sample_rate_ = file_.samplerate();
    num_samples_ = file_.frames();
    song_.resize(channels_);
    for (auto& c : song_) {
        c.reserve(num_samples_);
    }
    int read_size = 2048;
    tmp.resize(channels_ * read_size);
    size_t read;
    while(true) {
        read = file_.readf(tmp.data(), read_size);
        if (read == 0) break;
        for (size_t i=0; i < read; ++i) {
            for (int channel=0; channel < channels_; ++channel) {
                song_[channel].push_back(tmp[i * channels_ + channel]);
            }
        }
    }

    duration_ = static_cast<double>(num_samples_) / sample_rate_;
    read_speed_ = 1 / duration_;
    frame_duration_ = 1.0 / sample_rate_;
    dbg("Song", path, channels_, sample_rate_, duration_, read_speed_, frame_duration_);
}

void Seeker::reset() {
    position_ = 0;
    first_ = true;
}

void Seeker::get_samples(
        uint64_t time,
        double target,
        std::vector<float>& buffer,
        size_t n_samples) {

    assert(buffer.size() == n_samples * channels_);

    target = positive_modulof(target, 1);
    if (first_) {
        first_ = false;
        current_ = target;
    }
    double delta = target - current_;
    if (std::abs(delta) > 0.5) {
        delta -= 1;
    }
    double delay = frame_duration_ * n_samples;
    double speed = delta / delay;

    if (true || std::abs(speed) < SPEED_THRESHOLD) {
        speed = 0;
    } else {
        auto sign = speed / std::abs(speed);
        speed = std::min(std::abs(speed), MAX_SPEED) * sign;
    }



    auto it = buffer.begin();
    double position = current_ + static_cast<double>(time) / sample_rate_ / duration_;
    for (size_t index=0; index < n_samples; ++index) {
        position = positive_modulof(position, 1);
        for (int channel=0; channel < channels_; ++channel) {
            //*it = song_[position_ * channels_ + channel];
            *it = interpolate(song_[channel], position);
            //*it = std::sin(440 * position * 2 * M_PI * duration_);
            it++;
        }
        position += frame_duration_ * (speed + read_speed_);
        current_ += speed * frame_duration_;
        current_ = positive_modulof(current_, 1);
        ++position_;
        position_ = positive_modulo(position_, num_samples_);
    }
    if (speed != 0)
        dbg("speed", delta / delay, speed, read_speed_, current_, target, position);
}

Mixer::Mixer() 
: to_cb_(QUEUE_SIZE)
, from_cb_(QUEUE_SIZE)
, from_camera_(QUEUE_SIZE) {
    for (int i=0; i < NUM_SONGS; ++i) {
        std::string filename = "songs/" + std::to_string(i) + ".wav";
        files_.emplace_back(filename);
        assert(files_[i].file().channels() == CHANNELS);
        assert(files_[i].file().samplerate() == SAMPLE_RATE);

    }
    call_pa(
        Pa_OpenDefaultStream, &stream_, 0, CHANNELS, 
        paFloat32, SAMPLE_RATE, FBP, pa_static_callback, static_cast<void*>(this));
    mixer_thread_ = std::thread([this](){
        do_mixer_thread();
    });
}

Mixer::~Mixer() {
    call_pa(Pa_StopStream, stream_);
    call_pa(Pa_CloseStream, stream_);
    call_pa(Pa_Terminate);
}

void Mixer::do_mixer_thread() {
    call_pa(Pa_StartStream, stream_);
    std::vector<QRSong> state(7);
    uint64_t index = 0;
    std::vector<float> tmp_buffer;
    tmp_buffer.resize(FBP * CHANNELS);
    dbg("Mixer thread starting");
    while(true) {
        from_camera_.read(state);
        std::vector<float> buffer;
        from_cb_.read(buffer);
        buffer.resize(FBP * CHANNELS);
        std::fill(buffer.begin(), buffer.end(), 0);
        for (int i=0; i < NUM_SONGS; ++i) {
            if (!state[i].active) {
                files_[i].reset();
                continue;
            }
            double base_position = 2 * (state[i].center.x - 0.5);
            files_[i].get_samples(index, base_position, tmp_buffer, FBP);
            double volume = volume_for_size(state[i].size);
            for (int s=0; s < FBP; ++s) {
                for (int c=0; c < CHANNELS; ++c) {
                    int idx = s * CHANNELS + c;
                    buffer[idx] += tmp_buffer[idx] * volume;
                }
            }
        }
        index += FBP;
        while(to_cb_.isFull());
        to_cb_.write(std::move(buffer));
    }
}

void Mixer::push_camera_state(std::vector<QRSong>&& songs) {
    if (!from_camera_.write(std::move(songs))) {
        dbg("Failed to push state from camera");
    }
}

int Mixer::pa_static_callback(
        const void* input_buffer,
        void* output_buffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void* user_data) {
    auto me = static_cast<Mixer*>(user_data);
    return me->pa_callback(
        input_buffer,
        output_buffer,
        framesPerBuffer,
        timeInfo,
        statusFlags);
}

int Mixer::pa_callback(
        const void* input_buffer,
        void* output_buffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags) {
    float* buffer = static_cast<float*>(output_buffer);
    assert(framesPerBuffer == FBP);
    if (statusFlags != 0) {
        dbg("Status flag", statusFlags);
    }

    std::vector<float> from_mixer;
    if (to_cb_.read(from_mixer)) {
        assert(from_mixer.size() == framesPerBuffer * CHANNELS);
        std::copy(from_mixer.begin(), from_mixer.end(), buffer);
        //std::fill(buffer, buffer + framesPerBuffer * CHANNELS, 0);
        if (!from_cb_.write(std::move(from_mixer))) {
            dbg("Failed to send buffer back");
        }
    } else {
        dbg("Filling with void");
        std::fill(buffer, buffer + framesPerBuffer * CHANNELS, 0);
    }
    return paContinue;
}