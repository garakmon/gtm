#include "measureroll.h"

#include "colors.h"
#include "song.h"



MeasureRoll::MeasureRoll(QObject *parent) : QObject(parent) {
    //
}

bool MeasureRoll::advance() {
    //
    this->m_current_tick += 1;
    // check for end of song
}

void MeasureRoll::drawMeasures() {
    // draw measure lines
    this->m_scene_measures.clear();

    int track_num = 0;

    int final_tick = this->m_active_song->durationInTicks();

    // TODO: bounds checking
    // for (auto event : this->m_active_song->tracks().front()) {
    //     // TODO m_tracks
    //     //this->m_scene_measures.addItem(new QGraphicsRectItem(track_num));
    //     track_num++;
    // }

    this->m_scene_measures.setBackgroundBrush(ui_color_piano_roll_bg);
    this->m_scene_measures.setSceneRect(QRect(0, 0, final_tick * ui_tick_x_scale, 30));
}
