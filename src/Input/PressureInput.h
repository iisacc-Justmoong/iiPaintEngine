//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include "Core/Types.h"

struct PressureInput {
    bool enabled = true;
    Types::Scalar minimum = 0.0;
    Types::Scalar maximum = 1.0;
    Types::Scalar rawPressure = 1.0;
    bool contact = true;
    bool hovering = false;
    Types::Scalar curveMinimum = 0.0;
    Types::Scalar curveCenter = 0.5;
    Types::Scalar curveMaximum = 1.0;
};

Types::Scalar resolvePressureInput(const PressureInput &input);
bool pressureInputHasContact(Types::Scalar rawPressure);
bool pressureInputHasVariablePressure(Types::Scalar rawPressure);
