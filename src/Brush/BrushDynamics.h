//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/Types.h"
#include <array>
#include <cstdint>
#include <vector>

enum class BrushDynamicsEasing {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
};

enum class BrushDynamicsCombineMode {
    Multiply,
    Add,
    Replace,
};

struct BrushCurvePoint {
    Types::Scalar input = 0.0;
    Types::Scalar output = 0.0;
};

struct BrushDynamicsResponseCurve {
    bool enabled = false;
    Types::Scalar min = 1.0;
    Types::Scalar center = 1.0;
    Types::Scalar max = 1.0;
    Types::Scalar jitter = 0.0;
    BrushDynamicsEasing easing = BrushDynamicsEasing::Linear;
    std::vector<BrushCurvePoint> points; // Strictly increasing x in [0,1]; overrides min/center/max.
};

enum class BrushDynamicsSource {
    Pressure, Velocity, Tilt, TiltX, TiltY, Direction, Rotation,
    TangentialPressure, Distance, Time, Random, StrokeRandom, Custom,
};

enum class BrushDynamicsTarget {
    Size, Flow, Opacity, Hardness, Spacing, Scatter, Rotation, Roundness,
    TextureDepth, Wetness, DryOut, BristleSpread, ScatterCount,
    Hue, Saturation, Value, ColorMix, Mix,
};

// These are iiPaintEngine controls, not Photoshop descriptor numeric values.
enum class BrushDynamicsControl {
    Off, Fade, PenPressure, PenTilt, StylusWheel, StylusRotation,
    InitialDirection, Direction,
};

struct BrushDynamicsVariation {
    bool enabled = false;
    BrushDynamicsControl control = BrushDynamicsControl::Off;
    Types::Scalar jitter = 0.0; // Fraction of the available range, [0,1].
    Types::Scalar minimum = 0.0; // Floor of a scale control, [0,1].
    std::uint32_t fadeSteps = 25; // Emitted tip marks, not input events or pixels.
};

struct BrushPoseSettings {
    bool enabled = false;
    bool overridePressure = false;
    bool overrideTiltX = false;
    bool overrideTiltY = false;
    bool overrideRotation = false;
    Types::Scalar pressure = 1.0;
    Types::Scalar tiltX = 0.0;
    Types::Scalar tiltY = 0.0;
    Types::Scalar rotationRadians = 0.0;
};

struct BrushDynamicsBinding {
    bool enabled = true;
    BrushDynamicsSource source = BrushDynamicsSource::Pressure;
    BrushDynamicsTarget target = BrushDynamicsTarget::Size;
    BrushDynamicsCombineMode combineMode = BrushDynamicsCombineMode::Multiply;
    Types::Scalar inputMinimum = 0.0;
    Types::Scalar inputMaximum = 1.0;
    BrushDynamicsResponseCurve curve{true, 0.0, 0.5, 1.0};
    unsigned int customInput = 0; // One of eight application-defined scalar channels.
};

struct BrushDynamicsPropertyResponse {
    bool enabled = false;
    Types::Scalar neutral = 1.0;
    BrushDynamicsCombineMode combineMode = BrushDynamicsCombineMode::Multiply;
    BrushDynamicsResponseCurve pressure;
    BrushDynamicsResponseCurve velocity;
    BrushDynamicsResponseCurve tilt;
    BrushDynamicsResponseCurve random;
};

struct BrushDynamics {
    bool pressureInputEnabled = true;
    bool velocityInputEnabled = true;
    bool tiltInputEnabled = true;
    bool randomInputEnabled = true;
    bool pressureToSizeEnabled = true;
    bool pressureToOpacityEnabled = true;
    bool pressureToFlowEnabled = true;
    bool velocityToSpacingEnabled = true;
    bool velocityToOpacityEnabled = true;
    bool velocityToDryOutEnabled = true;
    bool tiltToEllipseEnabled = true;
    bool rotationJitterEnabled = true;
    bool grainJitterEnabled = true;
    Types::Scalar pressureToSize = 0.0;
    Types::Scalar pressureToOpacity = 0.0;
    Types::Scalar pressureToFlow = 0.0;
    Types::Scalar velocityToSpacing = 0.0;
    Types::Scalar velocityToOpacity = 0.0;
    Types::Scalar velocityToDryOut = 0.0;
    bool tiltToRotation = false;
    Types::Scalar tiltToEllipse = 0.0;
    bool tiltToTextureDirection = false;
    Types::Scalar rotationJitter = 0.0;
    Types::Scalar grainJitter = 0.0;
    BrushDynamicsPropertyResponse sizeResponse;
    BrushDynamicsPropertyResponse flowResponse;
    BrushDynamicsPropertyResponse opacityResponse;
    BrushDynamicsPropertyResponse spacingResponse;
    BrushDynamicsPropertyResponse scatterResponse;
    BrushDynamicsPropertyResponse rotationResponse{false, 0.0, BrushDynamicsCombineMode::Add};
    BrushDynamicsPropertyResponse textureDepthResponse;
    BrushDynamicsPropertyResponse wetnessResponse;
    BrushDynamicsPropertyResponse dryOutResponse;
    BrushDynamicsPropertyResponse bristleSpreadResponse;
    std::vector<BrushDynamicsBinding> bindings; // Applied in order after legacy responses.
    BrushPoseSettings pose;
    BrushDynamicsVariation sizeVariation;
    BrushDynamicsVariation angleVariation;
    BrushDynamicsVariation roundnessVariation;
    BrushDynamicsVariation scatterVariation;
    BrushDynamicsVariation scatterCountVariation;
    BrushDynamicsVariation textureDepthVariation;
    BrushDynamicsVariation colorMixVariation;
    BrushDynamicsVariation opacityVariation;
    BrushDynamicsVariation flowVariation;
    BrushDynamicsVariation wetnessVariation;
    BrushDynamicsVariation mixVariation;
    bool flipXJitter = false;
    bool flipYJitter = false;
    bool projectTip = false;
};

struct BrushDynamicsInput {
    Types::Scalar pressure = 1.0;
    Types::Scalar velocity = 0.0;
    Types::Scalar tiltX = 0.0;
    Types::Scalar tiltY = 0.0;
    Types::Scalar randomRotation = 0.0;
    Types::Scalar randomGrain = 0.0;
    Types::Scalar directionRadians = 0.0;
    Types::Scalar rotationRadians = 0.0;
    Types::Scalar tangentialPressure = 0.0;
    Types::Scalar distance = 0.0;
    Types::Scalar elapsedTime = 0.0;
    Types::Scalar strokeRandom = 0.0;
    std::array<Types::Scalar, 8> custom{};
    std::uint32_t dabIndex = 0;
    Types::Scalar initialDirectionRadians = 0.0;
};

struct BrushDynamicsResult {
    Types::Scalar sizeScale = 1.0;
    Types::Scalar opacityScale = 1.0;
    Types::Scalar flowScale = 1.0;
    Types::Scalar hardnessScale = 1.0;
    Types::Scalar spacingScale = 1.0;
    Types::Scalar scatterScale = 1.0;
    bool rotationFromTilt = false;
    Types::Scalar rotationRadians = 0.0;
    Types::Scalar rotationOffsetRadians = 0.0;
    Types::Scalar rotationJitterRadians = 0.0;
    Types::Scalar ellipseScaleX = 1.0;
    Types::Scalar ellipseScaleY = 1.0;
    bool textureDirectionFromTilt = false;
    Types::Scalar textureDirectionRadians = 0.0;
    Types::Scalar textureDepthScale = 1.0;
    Types::Scalar wetnessScale = 1.0;
    Types::Scalar dryOutScale = 1.0;
    Types::Scalar bristleSpreadScale = 1.0;
    Types::Scalar grain = 0.0;
    Types::Scalar roundnessScale = 1.0;
    Types::Scalar scatterCountScale = 1.0;
    Types::Scalar hueOffset = 0.0;
    Types::Scalar saturationScale = 1.0;
    Types::Scalar valueScale = 1.0;
    Types::Scalar colorMix = 0.0;
    Types::Scalar mixScale = 1.0;
    bool flipX = false;
    bool flipY = false;
};

// 직접 편집한 곡선을 검증한다. 순서·범위 또는 유한성에 문제가 있으면 std::invalid_argument를 던진다.
Types::Scalar evaluateBrushResponseCurve(const BrushDynamicsResponseCurve &curve, Types::Scalar input);

// 속성별 곡선과 바인딩을 한 번 검증해 여러 센서 입력을 같은 설정으로 평가한다.
// 호출자가 소유한 BrushDynamics를 빌리므로 객체의 수명 동안 설정을 변경할 수 없다.
class BrushDynamicsEvaluator final {
public:
    // 활성 곡선의 매듭을 검증한다. 잘못된 매듭은 std::invalid_argument로 보고한다.
    explicit BrushDynamicsEvaluator(const BrushDynamics &dynamics);
    BrushDynamicsEvaluator(const BrushDynamics &&) = delete;

    // 검증된 설정으로 현재 입력을 평가한다. P개 매듭의 구간 선택은 O(log P)이다.
    [[nodiscard]] BrushDynamicsResult evaluate(const BrushDynamicsInput &input) const;

    // 검증에 사용한 설정을 반환한다.
    [[nodiscard]] const BrushDynamics &settings() const noexcept { return dynamics_; }

private:
    const BrushDynamics &dynamics_;
};

BrushDynamicsInput applyBrushPose(const BrushPoseSettings &pose, const BrushDynamicsInput &input);

// 편집 가능한 설정을 받아 단일 입력을 검증하고 평가한다.
BrushDynamicsResult resolveBrushDynamics(const BrushDynamics &dynamics,
                                         const BrushDynamicsInput &input);
