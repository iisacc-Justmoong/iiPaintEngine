#pragma once

#include <Brush/BrushState.h>
#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>

struct BrushPreset;

namespace ii::brush {
enum class Tool { Paint, Airbrush, Mixer, Bristle, Smudge, Eraser };
enum class Parameter : std::size_t {
    Size, Hardness, Opacity, Flow, Roundness, Angle, Spacing, Scatter, BristleCount,
    Stabilization, PostCorrection, Pressure, Tilt, Velocity, Wetness, Mix, Load,
    Dilution, Start, End, MinimumSize, TaperStrength, Count
};
enum class Unit { Pixels, Percent, Degrees, Count, Response };
struct ParameterInfo {
    Parameter parameter;
    std::string_view key;
    Unit unit;
    double minimum, maximum, initial;
};
[[nodiscard]] std::span<const ParameterInfo> parameterInfo() noexcept;
class Controls final {
public:
    Controls();
    [[nodiscard]] double get(Parameter parameter) const;
    // Accepts unchanged valid values; rejects invalid values without mutation.
    bool set(Parameter parameter, double value);
    [[nodiscard]] std::string serialize() const;
    bool restore(std::string_view text);
private:
    std::array<double, static_cast<std::size_t>(Parameter::Count)> m_values{};
};
enum class InactiveReason { None, ToolMismatch, TimePlacement, TaperDisabled, Invalid,
                            PresetOverride, PresetDisabled };
struct Availability { bool enabled; InactiveReason reason; };
[[nodiscard]] Availability availability(Tool tool, Parameter parameter, const Controls& controls) noexcept;
// Checks known preset overrides in addition to tool-level policy. A positive
// absolute spacing shadows percentage spacing. This does not validate all
// preset fields or guarantee an effect for a particular input or dynamics.
[[nodiscard]] Availability availabilityWithPreset(Tool tool, Parameter parameter,
    const Controls& controls, const BrushPreset& base) noexcept;
enum class ErrorCode { InvalidControls, InvalidInput, BudgetExceeded };
struct Error { ErrorCode code; std::string message; };
struct TaperProfile {
    double start = 0, end = 0, minimumSize = 1, strength = 0;
};
struct ResolvedBrush {
    BrushState state;
    TaperProfile taper;
    double stabilization = 0, postCorrection = 0;
    bool eraser = false, hairline = false;
};
struct DynamicsContext {
    double velocityReferenceSpeed = 1200; // Document pixels per second at full response.
};
[[nodiscard]] std::expected<ResolvedBrush, Error> resolve(Tool tool, const Controls& controls,
    std::uint32_t argb = 0xff000000U, std::uint32_t seed = 1, const DynamicsContext& context = {});
[[nodiscard]] std::expected<ResolvedBrush, Error> resolveWithPreset(Tool tool, const Controls& controls,
    const BrushPreset& base, std::uint32_t argb = 0xff000000U, std::uint32_t seed = 1,
    const DynamicsContext& context = {});
[[nodiscard]] double taperSize(const TaperProfile& profile, double distance, double totalLength) noexcept;
} // namespace ii::brush
