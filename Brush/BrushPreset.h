//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <QImage>
#include <QUuid>

struct BrushSnapshot {
    QUuid brushId;
    QImage tipImage;
    float size = 0.0F;
    float opacity = 0.0F;
    float hardness = 0.0F;
    float flow = 0.0F;
    float density = 0.0F;
};
