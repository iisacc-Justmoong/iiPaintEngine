#include <cmath>
#include <string>

#include "Brush/BrushPresetSerializer.h"
#include "tests/RasterDabTestUtils.h"

namespace {

std::vector<BrushDab> lineDabs(const BrushState &brush)
{
    return streamTestDabs(brush,
                          StrokePoint{{0.0, 0.0}, 1.0, 0.0, 0.0, 0.0, 0.0, 0, 0.0},
                          StrokePoint{{12.0, 0.0}, 1.0, 1.0, 1.0, 0.0, 0.0, 0, 12.0});
}

bool samePosition(DocumentPoint lhs, DocumentPoint rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

} // namespace

int main()
{
    static_assert(BrushSimulationModel::Dry != BrushSimulationModel::WetPaint);
    static_assert(BrushSimulationModel::Smudge != BrushSimulationModel::Mixer);
    static_assert(BristleShape::Round != BristleShape::Flat);

    BrushState brush;
    brush.randomSeed = 77;
    brush.rasterizer.brushSize = 4.0;
    brush.rasterizer.spacingRatio = 0.5;
    brush.material.texture.enabled = true;
    brush.material.texture.space = BrushTextureSpace::StrokeFollow;
    brush.material.texture.width = 2;
    brush.material.texture.height = 1;
    brush.material.texture.alpha = {255, 64};
    brush.material.texture.grainStrength = 0.75;
    brush.material.texture.strength = 0.8;
    brush.material.texture.scaleJitter = 0.1;
    brush.material.texture.rotationJitter = 0.2;
    brush.material.paperGrain.enabled = true;
    brush.material.paperGrain.space = BrushTextureSpace::Paper;
    brush.material.paperGrain.width = 2;
    brush.material.paperGrain.height = 1;
    brush.material.paperGrain.alpha = {255, 128};
    brush.material.paperGrain.strength = 0.4;
    brush.material.dualBrush.enabled = true;
    brush.material.dualBrush.compositeMode = DualBrushCompositeMode::Add;
    brush.material.dualBrush.scale = 0.5;
    brush.material.dualBrush.spacingRatio = 0.25;
    brush.material.scatter.enabled = true;
    brush.material.scatter.radius = 3.0;
    brush.material.scatter.count = 2;
    brush.material.simulation.enabled = true;
    brush.material.simulation.model = BrushSimulationModel::Mixer;
    brush.material.simulation.wetness = 0.6;
    brush.material.simulation.smudgeStrength = 0.4;
    brush.material.simulation.mixStrength = 0.7;
    brush.material.simulation.pickup = 0.5;
    brush.material.simulation.deposit = 0.8;
    brush.material.bristle.enabled = true;
    brush.material.bristle.shape = BristleShape::Flat;
    brush.material.bristle.count = 12;
    brush.material.bristle.length = 6.0;
    brush.material.bristle.stiffness = 0.8;

    const std::vector<BrushDab> command = lineDabs(brush);
    if (command.size() < 4) {
        return 1;
    }

    BrushState unscatteredBrush = brush;
    unscatteredBrush.material.scatter.enabled = false;
    const std::vector<BrushDab> unscattered = lineDabs(unscatteredBrush);
    if (command.size() != unscattered.size()) {
        return 1;
    }

    const std::vector<BrushDab> sameSeed = lineDabs(brush);
    brush.randomSeed += 1;
    const std::vector<BrushDab> differentSeed = lineDabs(brush);
    if (!samePosition(command[1].position, sameSeed[1].position)
            || samePosition(command[1].position, differentSeed[1].position)) {
        return 1;
    }

    if (command[0].textureAlpha <= 0.0
            || command[0].textureAlpha > 1.0
            || command[1].grain <= 0.0
            || !command[0].dualBrush
            || command[0].alpha >= brush.rasterizer.flow) {
        return 1;
    }

    BrushState simulationOnly;
    simulationOnly.rasterizer.brushSize = 4.0;
    simulationOnly.rasterizer.spacingRatio = 0.5;
    simulationOnly.material.simulation.enabled = true;
    simulationOnly.material.simulation.model = BrushSimulationModel::WetPaint;
    simulationOnly.material.simulation.wetness = 0.8;
    simulationOnly.material.simulation.smudgeStrength = 0.4;
    simulationOnly.material.simulation.mixStrength = 0.2;
    const std::vector<BrushDab> simulationEnabled = lineDabs(simulationOnly);
    simulationOnly.material.simulation.enabled = false;
    const std::vector<BrushDab> simulationDisabled = lineDabs(simulationOnly);
    if (simulationEnabled.empty()
            || simulationDisabled.empty()
            || !(simulationEnabled.front().alpha < simulationDisabled.front().alpha)
            || !nearlyEqual(simulationDisabled.front().alpha, simulationOnly.rasterizer.flow)) {
        return 1;
    }

    BrushPreset preset;
    preset.brushId.bytes[0] = 9;
    preset.name = "Expressive Flat Mixer";
    preset.dynamics.sizeResponse.enabled = true;
    preset.dynamics.sizeResponse.pressure.enabled = true;
    preset.dynamics.sizeResponse.pressure.min = 0.2;
    preset.dynamics.sizeResponse.pressure.center = 0.6;
    preset.dynamics.sizeResponse.pressure.max = 1.0;
    preset.material = brush.material;

    const std::string payload = serializeBrushPreset(preset);
    const BrushPreset reopened = deserializeBrushPreset(payload);
    if (reopened.brushId.bytes[0] != 9
            || reopened.name != "Expressive Flat Mixer"
            || !reopened.dynamics.sizeResponse.pressure.enabled
            || !nearlyEqual(reopened.dynamics.sizeResponse.pressure.min, 0.2)
            || !reopened.material.texture.enabled
            || reopened.material.texture.space != BrushTextureSpace::StrokeFollow
            || reopened.material.texture.alpha.size() != 2
            || !nearlyEqual(reopened.material.texture.strength, 0.8)
            || !reopened.material.paperGrain.enabled
            || reopened.material.paperGrain.space != BrushTextureSpace::Paper
            || !reopened.material.dualBrush.enabled
            || reopened.material.dualBrush.compositeMode != DualBrushCompositeMode::Add
            || !reopened.material.scatter.enabled
            || reopened.material.scatter.count != 2
            || !reopened.material.simulation.enabled
            || reopened.material.simulation.model != BrushSimulationModel::Mixer
            || !nearlyEqual(reopened.material.simulation.pickup, 0.5)
            || !nearlyEqual(reopened.material.simulation.deposit, 0.8)
            || !reopened.material.bristle.enabled
            || reopened.material.bristle.shape != BristleShape::Flat
            || reopened.material.bristle.count != 12) {
        return 1;
    }

    return 0;
}
