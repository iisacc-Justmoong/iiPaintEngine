//
// Created by Justmoong on 2026 May 24.
//

#pragma once

enum class EngineErrorCode {
    None,
    InvalidArgument,
    InvalidState,
    IoFailure,
    UnsupportedColorTransform,
};

struct EngineError {
    EngineErrorCode code = EngineErrorCode::None;
    const char *message = nullptr;
};
