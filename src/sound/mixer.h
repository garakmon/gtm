#pragma once
#ifndef MIXER_H
#define MIXER_H

extern "C" {
    #include "portaudio.h"
}

#include <QMap>
#include <array>
#include <atomic>
#include "sound/soundtypes.h"
#include "util/constants.h"

class Mixer {
public:
    Mixer();

    void setInstrumentData(const VoiceGroup *vg, const QMap<QString, VoiceGroup> *all_vg,
                           const QMap<QString, Sample> *samples,
                           const QMap<QString, QByteArray> *pcm_data,
                           const QMap<QString, KeysplitTable> *keysplit_tables);
    void noteOn(uint8_t channel, uint8_t key, uint8_t velocity, uint8_t program);
    void noteOff(uint8_t channel, uint8_t key);
    void setChannelVolume(uint8_t channel, uint8_t value);
    void setChannelPan(uint8_t channel, uint8_t value);
    void setChannelExpression(uint8_t channel, uint8_t value);
    void setChannelPitchBend(uint8_t channel, int value);
    void setChannelMod(uint8_t channel, uint8_t value);
    void setChannelLfos(uint8_t channel, uint8_t value);
    void setChannelModt(uint8_t channel, uint8_t value);
    void setChannelLfodl(uint8_t channel, uint8_t value);
    void resetLfo(uint8_t channel);
    void setLfoTickRate(int samples_per_tick);
    void setChannelMute(uint8_t channel, bool muted);
    void setAllMuted(bool muted);
    void setMasterVolume(float volume);
    float masterVolume() const;
    void setSongVolume(uint8_t vol);
    void setReverbLevel(uint8_t level);
    void processAudio(float *out_buffer, unsigned long frame_count);
    void end();

    struct MeterLevels {
        float master_l = 0.0f;
        float master_r = 0.0f;
        std::array<float, g_num_midi_channels> channel_l{};
        std::array<float, g_num_midi_channels> channel_r{};
    };
    void getMeterLevels(MeterLevels *out) const;

    // Get current instrument info for a channel (for UI display)
    struct ChannelPlayInfo {
        bool active = false;
        QString voice_type;
    };
    ChannelPlayInfo getChannelPlayInfo(uint8_t channel) const;

private:
    Voice m_voices[g_max_voices];
    ChannelContext m_channels[g_num_midi_channels];

    const VoiceGroup *m_voicegroup = nullptr;
    const QMap<QString, VoiceGroup> *m_all_voicegroups = nullptr;
    const QMap<QString, Sample> *m_samples = nullptr;
    const QMap<QString, QByteArray> *m_pcm_data = nullptr;
    const QMap<QString, KeysplitTable> *m_keysplit_tables = nullptr;

    // Track current instrument per channel for UI display
    ChannelPlayInfo m_channel_play_info[g_num_midi_channels];

    std::atomic<float> m_master_volume{0.25f};
    std::atomic<float> m_song_volume{1.0f};  // GBA song volume: (vol+1)/16
    std::atomic<float> m_master_peak_l{0.0f};
    std::atomic<float> m_master_peak_r{0.0f};
    std::array<std::atomic<float>, g_num_midi_channels> m_channel_peak_l;
    std::array<std::atomic<float>, g_num_midi_channels> m_channel_peak_r;

    // Resolve keysplit instruments to their actual underlying instrument
    const Instrument *resolveInstrument(const Instrument *inst, uint8_t key,
                                        const VoiceGroup **out_vg) const;

    Voice *allocateVoice(uint8_t key);
    void updatePsgVoicePeak(Voice *v, uint8_t ch);
    void tickLfo(uint8_t ch);

    // LFO timing
    int m_lfo_counter = 0;
    int m_samples_per_lfo_tick = 919;  // default ~120 BPM: 44100 / (120 * 0.4)

    // Reverb state (GBA NORMAL reverb: two-tap circular delay with feedback)
    struct ReverbSample { float left = 0.0f; float right = 0.0f; };
    static const int k_reverb_buf_size = g_samples_per_frame * 2;  // 1470
    QVector<ReverbSample> m_reverb_buf = QVector<ReverbSample>(k_reverb_buf_size);
    int m_reverb_pos = 0;
    int m_reverb_pos2 = k_reverb_buf_size / 2;  // offset by half (735)
    float m_reverb_intensity = 0.0f;
};

#endif // MIXER_H
