//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <QUuid>

#include <vector>

#include "Brush/BrushSnapshot.h"
#include "Stroke/StrokePoint.h"

struct Stroke {
    QUuid id;
    std::vector<StrokePoint> points;
    BrushSnapshot brush;
};
