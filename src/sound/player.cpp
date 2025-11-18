#include "player.h"



bool Player::initializeAudio() {
    if (Pa_Initialize() != paNoError) return false;

    PaError err = Pa_OpenDefaultStream(
        &m_audio_stream,
        0,               // No input
        2,               // Stereo output
        paFloat32,       // 32-bit floating point
        44100,           // Sample rate
        512,             // Buffer size
        &Player::paStaticCallback,
        this
    );

    if (err != paNoError) return false;

    Pa_StartStream(m_audio_stream);
    return true;
}

int Player::paStaticCallback(const void *input, void *output,
                             unsigned long frame_count,
                             const PaStreamCallbackTimeInfo *time_info,
                             PaStreamCallbackFlags status_flags,
                             void *user_data) {
    (void)input;
    (void)time_info;
    (void)status_flags;

    Player *player = static_cast<Player *>(user_data);
    player->audioCallback(static_cast<float *>(output), frame_count);
    return paContinue;
}

void Player::audioCallback(float *out_buffer, unsigned long frame_count) {
    m_sequencer.update(frame_count);
    m_mixer.processAudio(out_buffer, frame_count);
}

void Player::loadSong(Song *song) {
    m_sequencer.setSong(song);
    m_sequencer.setMixer(&m_mixer);
    m_sequencer.reset();
}

void Player::play() {
    m_sequencer.play();
}

void Player::stop() {
    m_sequencer.stop();
    m_mixer.end();
}

void Player::seekToTick(int tick) {
    m_mixer.end();
    m_sequencer.seekToTick(tick);
}
