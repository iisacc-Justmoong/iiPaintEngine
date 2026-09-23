//
// Created by Justmoong on 2026 May 24.
//

#include "BrushLibrary.h"

std::vector<BrushPreset> builtInBrushPresets()
{
    BrushPreset ink;
    ink.name = "Pressure Ink";
    ink.size = 16;
    BrushDynamicsBinding size;
    size.curve.points = {{0, 0.05}, {0.35, 0.3}, {1, 1}};
    ink.dynamics.bindings.push_back(size);
    ink.stroke.spacingFollowsSize = true;
    BrushPreset pencil = ink;
    pencil.name = "Grain Pencil"; pencil.size = 5; pencil.flow = 0.5F; pencil.hardness = 0.65F;
    pencil.material.paperGrain.enabled = true;
    pencil.material.paperGrain.space = BrushTextureSpace::Document;
    pencil.material.paperGrain.width = 4; pencil.material.paperGrain.height = 4;
    pencil.material.paperGrain.alpha = {255, 100, 220, 150, 80, 210, 140, 240, 230, 130, 250, 90, 160, 240, 110, 200};
    pencil.material.paperGrain.strength = 0.6;
    BrushPreset airbrush;
    airbrush.name = "Soft Airbrush"; airbrush.size = 64; airbrush.hardness = 0; airbrush.flow = 0.08F;
    airbrush.stroke.airbrushEnabled = true; airbrush.stroke.airbrushRate = 60;
    BrushPreset flat = ink;
    flat.name = "Flat Marker"; flat.shape.kind = BrushTipShape::Square;
    flat.shape.angleMode = BrushAngleMode::Fixed; flat.shape.angleRadians = -0.5;
    flat.shape.roundness = 0.25; flat.opacity = 0.65F;
    BrushPreset spray;
    spray.name = "Color Spray"; spray.size = 6; spray.stroke.spacingRatio = 0.75;
    spray.material.scatter.enabled = true; spray.material.scatter.count = 8;
    spray.material.scatter.radius = 3; spray.material.scatter.relativeToSize = true;
    spray.material.scatter.distribution = BrushScatterDistribution::Disk;
    spray.color.enabled = true; spray.color.hueJitter = 0.1; spray.color.valueJitter = 0.2;
    BrushPreset mixer = flat;
    mixer.name = "Wet Flat Mixer"; mixer.size = 32; mixer.flow = 0.3F;
    mixer.shape.angleMode = BrushAngleMode::StrokeDirection;
    mixer.material.simulation = {true, BrushSimulationModel::Mixer, 0.8, 0.4, 0.7, 0.7, 0.2};
    mixer.material.bristle = {true, BristleShape::Flat, 24, 6, 0.6};
    std::vector<BrushPreset> presets{ink, pencil, airbrush, flat, spray, mixer};
    for (std::size_t i = 0; i < presets.size(); ++i) {
        presets[i].brushId.bytes[0] = 0xB2;
        presets[i].brushId.bytes[15] = static_cast<std::uint8_t>(i + 1);
    }
    return presets;
}
