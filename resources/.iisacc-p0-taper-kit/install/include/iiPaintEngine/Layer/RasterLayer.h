//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <vector>
#include <span>

#include "Core/RasterSample.h"
#include "Core/Types.h"

struct RasterLayer {
    Types::Pixel width = 0;
    Types::Pixel height = 0;
    std::vector<std::uint32_t> pixels;
};

struct PremultipliedPixel {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double alpha = 0.0;
};

struct StrokeCompositeBuffer {
    Types::Pixel width = 0;
    Types::Pixel height = 0;
    std::vector<PremultipliedPixel> pixels;
};

RasterLayer makeRasterLayer(Types::Pixel width,
                            Types::Pixel height,
                            std::uint32_t clearArgb = 0x00000000U);

StrokeCompositeBuffer makeStrokeCompositeBuffer(Types::Pixel width, Types::Pixel height);

void accumulateStrokeSamples(StrokeCompositeBuffer &buffer, const std::vector<RasterSample> &samples);

std::uint32_t strokeCompositePixelAt(const StrokeCompositeBuffer &buffer, DevicePixelPoint position);

void copyStrokeCompositeSamplePixels(RasterLayer &layer,
                                     const StrokeCompositeBuffer &buffer,
                                     const std::vector<RasterSample> &samples);

// Recompute only touched preview pixels from committed pixels plus pending coverage.
void copyStrokePreviewSamplePixels(RasterLayer &preview, const RasterLayer &committed,
                                   const StrokeCompositeBuffer &pending, std::span<const RasterSample> samples,
                                   bool destinationOut);

void compositeStrokeBufferOntoLayer(RasterLayer &layer, const StrokeCompositeBuffer &buffer);

void eraseStrokeBufferFromLayer(RasterLayer &layer, const StrokeCompositeBuffer &buffer);

void paintRasterSamples(RasterLayer &layer, const std::vector<RasterSample> &samples);

std::uint32_t rasterLayerPixelAt(const RasterLayer &layer, DevicePixelPoint position);
