#pragma once
#ifndef MIXER_H
#define MIXER_H

extern "C" {
    #include "portaudio.h"
}

#include <QMap>
#include "soundtypes.h"
#include "constants.h"

class Mixer {
public:
    Mixer() {}

    void setInstrumentData(const VoiceGroup *vg, const QMap<QString, VoiceGroup> *all_vg,
                           const QMap<QString, Sample> *samples,
                           const QMap<QString, QByteArray> *pcm_data);
    void noteOn(uint8_t channel, uint8_t key, uint8_t velocity, uint8_t program);
    void noteOff(uint8_t channel, uint8_t key);
    void setChannelVolume(uint8_t channel, uint8_t value);
    void setChannelPan(uint8_t channel, uint8_t value);
    void setChannelExpression(uint8_t channel, uint8_t value);
    void setChannelPitchBend(uint8_t channel, int value);
    void processAudio(float *out_buffer, unsigned long frame_count);
    void end();

private:
    Voice m_voices[g_max_voices];
    ChannelContext m_channels[g_num_midi_channels];

    const VoiceGroup *m_voicegroup = nullptr;
    const QMap<QString, VoiceGroup> *m_all_voicegroups = nullptr;
    const QMap<QString, Sample> *m_samples = nullptr;
    const QMap<QString, QByteArray> *m_pcm_data = nullptr;

    // Resolve keysplit instruments to their actual underlying instrument
    const Instrument *resolveInstrument(const Instrument *inst, uint8_t key,
                                        const VoiceGroup **out_vg) const;

    Voice *allocateVoice(uint8_t key);
};

#endif // MIXER_H
