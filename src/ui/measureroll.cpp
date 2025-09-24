#include "measureroll.h"

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
}
