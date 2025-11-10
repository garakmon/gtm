#include "player.h"



bool Player::initializeAudio() {
    if (Pa_Initialize() != paNoError) return false;

    // open a default stream to play a noise
    PaError err = Pa_OpenDefaultStream(
        &m_audio_stream,
        0,               // No input
        2,               // Stereo output
        paFloat32,       // 32-bit floating point
        44100,           // Sample rate
        512,             // Buffer size
        &Player::paStaticCallback,
        &m_mixer
    );

    if (err != paNoError) return false;

    Pa_StartStream(m_audio_stream);
    return true;
}

int Player::paStaticCallback(const void * input, void * output,
                             unsigned long frame_count,
                             const PaStreamCallbackTimeInfo * time_info,
                             PaStreamCallbackFlags status_flags,
                             void * user_data) {
    Mixer * mixer_ptr = static_cast<Mixer *>(user_data);
    return mixer_ptr->audioCallback(output, static_cast<uint32_t>(frame_count));
}


