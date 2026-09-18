#pragma once

// Disables Qt and native-platform pointer event coalescing so a delayed paint
// event loop still receives the measured points between the cursor endpoints.
// Call this before constructing QGuiApplication when possible. BitmapFileItem
// also applies the policy as a safe default for direct C++ and QML consumers.
void configureHighFidelityPointerInput();

bool highFidelityPointerInputConfigured();
