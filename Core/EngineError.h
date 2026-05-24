//
// Created by Justmoong on 2026 May 24.
//

#pragma once

enum class EngineErrorCode {
    None,
    InvalidArgument,
    InvalidState,
    IoFailure,
};

struct EngineError {
    EngineErrorCode code = EngineErrorCode::None;
    const char *message = nullptr;
};
