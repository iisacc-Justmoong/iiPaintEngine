#include <cmath>
#include <cstdint>
#include <vector>

#include "Brush/BrushPresetSerializer.h"
#include "tests/RasterDabTestUtils.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

std::uint8_t alphaAt(const std::vector<RasterSample> &samples, DevicePixelPoint position)
{
    for (const RasterSample &sample : samples) {
        if (sample.position.x == position.x && sample.position.y == position.y) {
            return alphaOf(sample.argb);
        }
    }
    return 0;
}

BrushDab dabAt(Types::Scalar x, Types::Scalar y)
{
    BrushDab dab;
    dab.position = {x, y};
    dab.scale = 1.0;
    dab.alpha = 1.0;
    dab.opacityCapScale = 1.0;
    dab.colorArgb = 0xFFFFFFFFU;
    return dab;
}

std::vector<BrushDab> twoPointDabs(const BrushState &brush)
{
    return streamTestDabs(brush,
                          StrokePoint{{0.0, 0.0}, 1.0, 0.0, 0.0, 0.0, 0.0, 1, 0.0},
                          StrokePoint{{10.0, 0.0}, 1.0, 1.0, 0.0, 0.0, 0.0, 1, 10.0});
}

} // namespace

int main()
{
    Rasterizer rasterizer;
    rasterizer.radius = 0;
    rasterizer.argb = 0xFFFFFFFFU;
    rasterizer.flow = 1.0;
    rasterizer.opacity = 1.0;

    BrushMaterial documentTexture;
    documentTexture.texture.enabled = true;
    documentTexture.texture.space = BrushTextureSpace::Document;
    documentTexture.texture.width = 2;
    documentTexture.texture.height = 1;
    documentTexture.texture.alpha = {255, 0};
    documentTexture.texture.scale = 1.0;
    const std::vector<RasterSample> documentSamples = projectBrushDabs({dabAt(1.0, 0.0)},
                                                                       rasterizer,
                                                                       RasterProjection{},
                                                                       documentTexture);
    if (alphaAt(documentSamples, {1, 0}) != 0) {
        return 1;
    }

    BrushMaterial tipTexture = documentTexture;
    tipTexture.texture.space = BrushTextureSpace::Tip;
    const std::vector<RasterSample> tipSamples = projectBrushDabs({dabAt(1.0, 0.0)},
                                                                  rasterizer,
                                                                  RasterProjection{},
                                                                  tipTexture);
    if (alphaAt(tipSamples, {1, 0}) == 0) {
        return 2;
    }

    BrushMaterial paperGrain;
    paperGrain.paperGrain.enabled = true;
    paperGrain.paperGrain.space = BrushTextureSpace::Paper;
    paperGrain.paperGrain.width = 2;
    paperGrain.paperGrain.height = 1;
    paperGrain.paperGrain.alpha = {0, 255};
    paperGrain.paperGrain.strength = 1.0;
    const std::vector<RasterSample> paperBlocked = projectBrushDabs({dabAt(0.0, 0.0)},
                                                                    rasterizer,
                                                                    RasterProjection{},
                                                                    paperGrain);
    const std::vector<RasterSample> paperOpen = projectBrushDabs({dabAt(1.0, 0.0)},
                                                                 rasterizer,
                                                                 RasterProjection{},
                                                                 paperGrain);
    if (alphaAt(paperBlocked, {0, 0}) != 0 || alphaAt(paperOpen, {1, 0}) == 0) {
        return 3;
    }

    BrushMaterial strokeFollow;
    strokeFollow.texture.enabled = true;
    strokeFollow.texture.space = BrushTextureSpace::StrokeFollow;
    strokeFollow.texture.width = 2;
    strokeFollow.texture.height = 1;
    strokeFollow.texture.alpha = {255, 0};
    BrushDab firstStrokeDab = dabAt(0.0, 0.0);
    firstStrokeDab.strokeDistance = 0.0;
    BrushDab secondStrokeDab = dabAt(0.0, 1.0);
    secondStrokeDab.strokeDistance = 1.0;
    const std::vector<RasterSample> strokeSamples = projectBrushDabs({firstStrokeDab, secondStrokeDab},
                                                                     rasterizer,
                                                                     RasterProjection{},
                                                                     strokeFollow);
    if (alphaAt(strokeSamples, {0, 0}) == 0 || alphaAt(strokeSamples, {0, 1}) != 0) {
        return 4;
    }

    BrushMaterial subtractDual;
    subtractDual.dualBrush.enabled = true;
    subtractDual.dualBrush.width = 1;
    subtractDual.dualBrush.height = 1;
    subtractDual.dualBrush.alpha = {255};
    subtractDual.dualBrush.compositeMode = DualBrushCompositeMode::Subtract;
    const std::vector<RasterSample> subtractSamples = projectBrushDabs({dabAt(0.0, 0.0)},
                                                                       rasterizer,
                                                                       RasterProjection{},
                                                                       subtractDual);
    BrushMaterial addDual = subtractDual;
    addDual.dualBrush.compositeMode = DualBrushCompositeMode::Add;
    const std::vector<RasterSample> addSamples = projectBrushDabs({dabAt(0.0, 0.0)},
                                                                  rasterizer,
                                                                  RasterProjection{},
                                                                  addDual);
    if (alphaAt(subtractSamples, {0, 0}) != 0 || alphaAt(addSamples, {0, 0}) == 0) {
        return 5;
    }

    BrushState jitterBrush;
    jitterBrush.randomSeed = 18;
    jitterBrush.rasterizer.brushSize = 4.0;
    jitterBrush.rasterizer.spacingRatio = 1.0;
    jitterBrush.material.texture.enabled = true;
    jitterBrush.material.texture.scaleJitter = 0.25;
    jitterBrush.material.texture.rotationJitter = 0.5;
    const std::vector<BrushDab> firstJitter = twoPointDabs(jitterBrush);
    const std::vector<BrushDab> sameJitter = twoPointDabs(jitterBrush);
    jitterBrush.randomSeed += 1;
    const std::vector<BrushDab> differentJitter = twoPointDabs(jitterBrush);
    if (firstJitter.size() < 2
            || sameJitter.size() < 2
            || differentJitter.size() < 2
            || !nearlyEqual(firstJitter[1].textureScale, sameJitter[1].textureScale)
            || !nearlyEqual(firstJitter[1].textureRotationRadians, sameJitter[1].textureRotationRadians)
            || nearlyEqual(firstJitter[1].textureScale, differentJitter[1].textureScale)
            || nearlyEqual(firstJitter[1].textureRotationRadians, differentJitter[1].textureRotationRadians)) {
        return 6;
    }

    BrushPreset preset;
    preset.material.texture.enabled = true;
    preset.material.texture.space = BrushTextureSpace::Document;
    preset.material.texture.assetCache.enabled = true;
    preset.material.texture.assetCache.assetId.bytes[0] = 7;
    preset.material.texture.assetCache.cacheKey = "paper-v1";
    preset.material.texture.assetCache.revision = 3;
    preset.material.texture.assetCache.width = 2;
    preset.material.texture.assetCache.height = 1;
    preset.material.texture.assetCache.alpha = {64, 192};
    preset.material.dualBrush.enabled = true;
    preset.material.dualBrush.compositeMode = DualBrushCompositeMode::Difference;
    const BrushPreset reopened = deserializeBrushPreset(serializeBrushPreset(preset));
    if (reopened.material.texture.space != BrushTextureSpace::Document
            || !reopened.material.texture.assetCache.enabled
            || reopened.material.texture.assetCache.assetId.bytes[0] != 7
            || reopened.material.texture.assetCache.cacheKey != "paper-v1"
            || reopened.material.texture.assetCache.revision != 3
            || reopened.material.texture.assetCache.alpha.size() != 2
            || reopened.material.dualBrush.compositeMode != DualBrushCompositeMode::Difference) {
        return 7;
    }

    return 0;
}
