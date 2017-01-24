#include "mixer.hpp"

#include <cassert>
#include <cmath>
#include <chrono>
#include <string>

#include "utils.hpp"

const int Mixer::FBP = 512;
const int Mixer::SAMPLE_RATE = 44100;
const int Mixer::CHANNELS = 2;
const int Mixer::QUEUE_SIZE = 20;
const int Mixer::NUM_SONGS = 7;

const double Seeker::MAX_SPEED = 2;
const double Seeker::SPEED_THRESHOLD = 20;
const double Seeker::SPEED_PROBA = 0.01;

const double MAX_SIZE = 0.0005;
const double MIN_SIZE = 0.000138;
const double MAX_SPEED = 3;
const double KERNEL_WINDOW = 3;

double volume_for_size(double size) {
    size = std::max(std::min(size, MAX_SIZE), MIN_SIZE);
    return (size - MIN_SIZE) / (MAX_SIZE - MIN_SIZE);
}

double get_speed(double speed_pos) {
    double high = 0.75;
    double low = 0.25;
    double middle = 0.40;
    if (speed_pos > high) {
        return 1 + (MAX_SPEED - 1) * (speed_pos - high) / (1 - high);
    } else if (speed_pos > middle) {
        return 1;
    } else if (speed_pos > low) {
        return (speed_pos - low) / (middle - low);
    } else {
        return - MAX_SPEED * (low - speed_pos) / low;
    }
}

inline double sinc(double x) {
    if (x == 0) {
        return 1;
    } else {
        return std::sin(M_PI * x) / (M_PI * x);
    }
}

inline double lanczos(double x, double w) {
    return sinc(x) * sinc(x / w);
}

float inline interpolate(std::vector<float>& samples, double location) {
    size_t a, size;
    size = samples.size();
    a = std::floor(location);
    double result = 0;
    for(int i=-KERNEL_WINDOW + 1; i <= KERNEL_WINDOW; ++i) {
        double delta = location - a + i;
        size_t idx = positive_modulo(a + i, size);
        result += samples[idx] * lanczos(delta, KERNEL_WINDOW);
    }
    //return samples[a];// * (1 - w) + samples[b] * w;
    return result;
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
}

void Seeker::get_samples(
        double speed,
        std::vector<float>& buffer,
        size_t n_samples) {

    assert(buffer.size() == n_samples * channels_);

    std::uniform_real_distribution<double> distro;

    auto it = buffer.begin();

    for (size_t index=0; index < n_samples; ++index) {
        for (int channel=0; channel < channels_; ++channel) {
            //*it = song_[channel][position_];
            *it = interpolate(song_[channel], position_);
            //*it = song_[channel][std::floor(position_)];
            //*it = std::sin(440 * position_ * 2 * M_PI);
            it++;
        }
        position_ += speed;
        position_ = positive_modulof(position_, num_samples_);
    }
}

void Seeker::noplay(size_t n_samples) {
    //position_ += n_samples;
}

const SndfileHandle& Seeker::file() const {
    return file_;
}

Mixer::Mixer(SoundIoDevice* device) 
: to_cb_(QUEUE_SIZE)
, from_cb_(QUEUE_SIZE)
, from_camera_(QUEUE_SIZE) {
    dbg(lanczos(0, KERNEL_WINDOW), lanczos(-1, KERNEL_WINDOW), lanczos(-2, KERNEL_WINDOW));
    for (int i=0; i < NUM_SONGS; ++i) {
        std::string filename = "songs/" + std::to_string(i) + ".wav";
        files_.emplace_back(filename);
        assert(files_[i].file().channels() == CHANNELS);
        assert(files_[i].file().samplerate() == SAMPLE_RATE);

    }
    stream_ = soundio_outstream_create(device);
    assert(stream_);
    stream_->format = SoundIoFormatFloat32NE;
    stream_->userdata = static_cast<void*>(this);
    stream_->write_callback = &Mixer::static_soundio_callback;
    stream_->sample_rate = SAMPLE_RATE;

    call_sio(soundio_outstream_open, stream_);
    assert(!stream_->layout_error);
    assert(stream_->layout.channel_count == CHANNELS);

    mixer_thread_ = std::thread([this](){
        do_mixer_thread();
    });

    call_sio(soundio_outstream_start, stream_);
}

Mixer::~Mixer() {
    soundio_outstream_destroy(stream_);
}

void Mixer::do_mixer_thread() {
    std::vector<QRSong> state(7);

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
                //files_[i].reset();
                files_[i].noplay(FBP);
                continue;
            }

            double speed_pos = state[i].center.x;
            double speed = get_speed(speed_pos);
            
            files_[i].get_samples(speed, tmp_buffer, FBP);
            double volume = volume_for_size(state[i].size);
            for (int s=0; s < FBP; ++s) {
                for (int c=0; c < CHANNELS; ++c) {
                    int idx = s * CHANNELS + c;
                    buffer[idx] += tmp_buffer[idx] * volume;
                }
            }
        }
        while(to_cb_.isFull());
        to_cb_.write(std::move(buffer));
    }
}

void Mixer::push_camera_state(std::vector<QRSong>&& songs) {
    if (!from_camera_.write(std::move(songs))) {
        dbg("Failed to push state from camera");
    }
}

void Mixer::static_soundio_callback(
        SoundIoOutStream* stream,
        int frame_count_min,
        int frame_count_max) {
    auto me = static_cast<Mixer*>(stream->userdata);
    return me->soundio_callback(stream, frame_count_min, frame_count_max);
}

void Mixer::soundio_callback(
        SoundIoOutStream* stream,
        int frame_count_min,
        int frame_count_max) {
    if (FBP < frame_count_min || FBP > frame_count_max) {
        print(
            "Replace the value of FBP by something between", 
            frame_count_min, frame_count_max);
        assert(false);
    }
    SoundIoChannelArea *areas;
    int frame_count = FBP;
    call_sio(soundio_outstream_begin_write, stream, &areas, &frame_count);
    assert(frame_count == FBP);
    std::vector<float> from_mixer;
    if (to_cb_.read(from_mixer)) {
        assert(from_mixer.size() == FBP * CHANNELS);
        for (int frame=0; frame < frame_count; ++frame) {
            for (int channel=0; channel < CHANNELS; ++channel) {
                *reinterpret_cast<float*>(areas[channel].ptr) = from_mixer[frame * CHANNELS + channel];
                areas[channel].ptr += areas[channel].step;
            }
        }
        if (!from_cb_.write(std::move(from_mixer))) {
            dbg("Failed to send buffer back");
        }
    } else {
        dbg("Filling with void");
        for (int frame=0; frame < frame_count; ++frame) {
            for (int channel=0; channel < CHANNELS; ++channel) {
                *reinterpret_cast<float*>(areas[channel].ptr) = 0;
                areas[channel].ptr += areas[channel].step;
            }
        }
    }
}