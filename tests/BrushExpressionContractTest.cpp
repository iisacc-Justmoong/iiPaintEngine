#include <string>

#include "Brush/BrushPresetSerializer.h"
#include "Stroke/StrokeCommand.h"

namespace {

StrokeInput makeLineInput()
{
    StrokeInput input;
    input.points.push_back(StrokePoint{{0.0, 0.0}, 1.0, 0.0, 0.0, 0.0, 0.0, 0, 0.0});
    input.points.push_back(StrokePoint{{12.0, 0.0}, 1.0, 1.0, 1.0, 0.0, 0.0, 0, 12.0});
    return input;
}

bool samePosition(DocumentPoint lhs, DocumentPoint rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
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
    brush.material.texture.width = 2;
    brush.material.texture.height = 1;
    brush.material.texture.alpha = {255, 64};
    brush.material.texture.grainStrength = 0.75;
    brush.material.dualBrush.enabled = true;
    brush.material.dualBrush.scale = 0.5;
    brush.material.dualBrush.spacingRatio = 0.25;
    brush.material.scatter.enabled = true;
    brush.material.scatter.radius = 3.0;
    brush.material.scatter.count = 2;
    brush.material.simulation.model = BrushSimulationModel::Mixer;
    brush.material.simulation.wetness = 0.6;
    brush.material.simulation.smudgeStrength = 0.4;
    brush.material.simulation.mixStrength = 0.7;
    brush.material.bristle.shape = BristleShape::Flat;
    brush.material.bristle.count = 12;
    brush.material.bristle.length = 6.0;
    brush.material.bristle.stiffness = 0.8;

    const StrokeCommand command = makeStrokeCommand(makeLineInput(), brush, Stabilizer{0.0});
    if (command.dabs.size() < 4) {
        return 1;
    }

    BrushState unscatteredBrush = brush;
    unscatteredBrush.material.scatter.enabled = false;
    const StrokeCommand unscattered = makeStrokeCommand(makeLineInput(), unscatteredBrush, Stabilizer{0.0});
    if (command.dabs.size() != unscattered.dabs.size()) {
        return 1;
    }

    const StrokeCommand sameSeed = makeStrokeCommand(makeLineInput(), brush, Stabilizer{0.0});
    brush.randomSeed += 1;
    const StrokeCommand differentSeed = makeStrokeCommand(makeLineInput(), brush, Stabilizer{0.0});
    if (!samePosition(command.dabs[1].position, sameSeed.dabs[1].position)
            || samePosition(command.dabs[1].position, differentSeed.dabs[1].position)) {
        return 1;
    }

    if (command.dabs[0].textureAlpha <= 0.0
            || command.dabs[0].textureAlpha > 1.0
            || command.dabs[1].grain <= 0.0
            || !command.dabs[0].dualBrush
            || command.dabs[0].alpha >= brush.rasterizer.flow) {
        return 1;
    }

    BrushPreset preset;
    preset.brushId.bytes[0] = 9;
    preset.name = "Expressive Flat Mixer";
    preset.material = brush.material;

    const std::string payload = serializeBrushPreset(preset);
    const BrushPreset reopened = deserializeBrushPreset(payload);
    if (reopened.brushId.bytes[0] != 9
            || reopened.name != "Expressive Flat Mixer"
            || !reopened.material.texture.enabled
            || reopened.material.texture.alpha.size() != 2
            || !reopened.material.dualBrush.enabled
            || !reopened.material.scatter.enabled
            || reopened.material.scatter.count != 2
            || reopened.material.simulation.model != BrushSimulationModel::Mixer
            || reopened.material.bristle.shape != BristleShape::Flat
            || reopened.material.bristle.count != 12) {
        return 1;
    }

    return 0;
}
