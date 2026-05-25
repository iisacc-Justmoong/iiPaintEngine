//
// Created by Justmoong on 2026 May 24.
//

#include "DocumentSerializer.h"

#include <array>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>

namespace {

constexpr char kHexDigits[] = "0123456789ABCDEF";

void writeLine(std::ostringstream &output, const std::string &key, const std::string &value)
{
    output << key << '\t' << value << '\n';
}

template <typename T>
std::string numberText(T value)
{
    std::ostringstream output;
    output << std::setprecision(std::numeric_limits<long double>::digits10 + 1) << value;
    return output.str();
}

std::string boolText(bool value)
{
    return value ? "1" : "0";
}

std::string stringText(const std::string &value)
{
    std::ostringstream output;
    output << std::quoted(value);
    return output.str();
}

template <typename Point>
std::string pointText(const Point &point)
{
    std::ostringstream output;
    output << std::setprecision(std::numeric_limits<long double>::digits10 + 1)
           << point.x << ' ' << point.y;
    return output.str();
}

template <typename Rect>
std::string rectText(const Rect &rect)
{
    std::ostringstream output;
    output << std::setprecision(std::numeric_limits<long double>::digits10 + 1)
           << rect.origin.x << ' ' << rect.origin.y << ' '
           << rect.width << ' ' << rect.height;
    return output.str();
}

void appendHexByte(std::string &text, unsigned int value)
{
    text.push_back(kHexDigits[(value >> 4U) & 0x0FU]);
    text.push_back(kHexDigits[value & 0x0FU]);
}

std::string uuidText(const PaintUuid &uuid)
{
    std::string text;
    text.reserve(uuid.bytes.size() * 2);
    for (const std::uint8_t byte : uuid.bytes) {
        appendHexByte(text, byte);
    }
    return text;
}

std::string byteVectorText(const std::vector<std::byte> &bytes)
{
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const std::byte byte : bytes) {
        appendHexByte(text, std::to_integer<unsigned int>(byte));
    }
    return text;
}

std::string byteVectorText(const std::vector<Types::Byte> &bytes)
{
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const Types::Byte byte : bytes) {
        appendHexByte(text, byte);
    }
    return text;
}

std::string u32VectorText(const std::vector<std::uint32_t> &values)
{
    std::string text;
    text.reserve(values.size() * 8);
    for (const std::uint32_t value : values) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            text.push_back(kHexDigits[(value >> shift) & 0x0FU]);
        }
    }
    return text;
}

int hexValue(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return 0;
}

std::uint8_t parseHexByte(const std::string &text, std::size_t offset)
{
    return static_cast<std::uint8_t>((hexValue(text[offset]) << 4) | hexValue(text[offset + 1]));
}

std::map<std::string, std::string> parsePayload(const std::string &payload)
{
    std::map<std::string, std::string> values;
    std::istringstream input(payload);
    std::string line;
    while (std::getline(input, line)) {
        const std::size_t separator = line.find('\t');
        if (separator == std::string::npos) {
            continue;
        }
        values[line.substr(0, separator)] = line.substr(separator + 1);
    }
    return values;
}

template <typename T>
T readNumber(const std::map<std::string, std::string> &values,
             const std::string &key,
             T fallback = {})
{
    const auto iterator = values.find(key);
    if (iterator == values.end()) {
        return fallback;
    }

    T result{};
    std::istringstream input(iterator->second);
    input >> result;
    if (!input) {
        return fallback;
    }
    return result;
}

bool readBool(const std::map<std::string, std::string> &values,
              const std::string &key,
              bool fallback = false)
{
    return readNumber<int>(values, key, fallback ? 1 : 0) != 0;
}

std::string readString(const std::map<std::string, std::string> &values,
                       const std::string &key,
                       const std::string &fallback = {})
{
    const auto iterator = values.find(key);
    if (iterator == values.end()) {
        return fallback;
    }

    std::string result;
    std::istringstream input(iterator->second);
    input >> std::quoted(result);
    if (!input) {
        return fallback;
    }
    return result;
}

PaintUuid readUuid(const std::map<std::string, std::string> &values, const std::string &key)
{
    PaintUuid uuid{};
    const auto iterator = values.find(key);
    if (iterator == values.end() || iterator->second.size() < uuid.bytes.size() * 2) {
        return uuid;
    }

    for (std::size_t index = 0; index < uuid.bytes.size(); ++index) {
        uuid.bytes[index] = parseHexByte(iterator->second, index * 2);
    }
    return uuid;
}

std::vector<std::byte> readByteVector(const std::map<std::string, std::string> &values,
                                      const std::string &key)
{
    const auto iterator = values.find(key);
    if (iterator == values.end()) {
        return {};
    }

    std::vector<std::byte> bytes;
    const std::string &text = iterator->second;
    bytes.reserve(text.size() / 2);
    for (std::size_t offset = 0; offset + 1 < text.size(); offset += 2) {
        bytes.push_back(static_cast<std::byte>(parseHexByte(text, offset)));
    }
    return bytes;
}

std::vector<Types::Byte> readTypesByteVector(const std::map<std::string, std::string> &values,
                                             const std::string &key)
{
    const auto iterator = values.find(key);
    if (iterator == values.end()) {
        return {};
    }

    std::vector<Types::Byte> bytes;
    const std::string &text = iterator->second;
    bytes.reserve(text.size() / 2);
    for (std::size_t offset = 0; offset + 1 < text.size(); offset += 2) {
        bytes.push_back(parseHexByte(text, offset));
    }
    return bytes;
}

std::vector<std::uint32_t> readU32Vector(const std::map<std::string, std::string> &values,
                                         const std::string &key)
{
    const auto iterator = values.find(key);
    if (iterator == values.end()) {
        return {};
    }

    std::vector<std::uint32_t> output;
    const std::string &text = iterator->second;
    output.reserve(text.size() / 8);
    for (std::size_t offset = 0; offset + 7 < text.size(); offset += 8) {
        std::uint32_t value = 0;
        for (std::size_t digit = 0; digit < 8; ++digit) {
            value = static_cast<std::uint32_t>((value << 4U) | hexValue(text[offset + digit]));
        }
        output.push_back(value);
    }
    return output;
}

template <typename Point>
Point readPoint(const std::map<std::string, std::string> &values, const std::string &key)
{
    Point point{};
    const auto iterator = values.find(key);
    if (iterator == values.end()) {
        return point;
    }

    std::istringstream input(iterator->second);
    input >> point.x >> point.y;
    return point;
}

template <typename Rect>
Rect readRect(const std::map<std::string, std::string> &values, const std::string &key)
{
    Rect rect{};
    const auto iterator = values.find(key);
    if (iterator == values.end()) {
        return rect;
    }

    std::istringstream input(iterator->second);
    input >> rect.origin.x >> rect.origin.y >> rect.width >> rect.height;
    return rect;
}

void writeDocumentMetadata(std::ostringstream &output,
                           const std::string &prefix,
                           const DocumentMetadata &metadata)
{
    writeLine(output, prefix + ".title", stringText(metadata.title));
    writeLine(output, prefix + ".author", stringText(metadata.author));
    writeLine(output, prefix + ".storagePath", stringText(metadata.storagePath));
    writeLine(output, prefix + ".createdAt", stringText(metadata.createdAt));
    writeLine(output, prefix + ".modifiedAt", stringText(metadata.modifiedAt));
    writeLine(output, prefix + ".documentId", uuidText(metadata.documentId));
    writeLine(output, prefix + ".version", numberText(metadata.version));
    writeLine(output, prefix + ".appVersion", stringText(metadata.appVersion));
}

DocumentMetadata readDocumentMetadata(const std::map<std::string, std::string> &values,
                                      const std::string &prefix)
{
    DocumentMetadata metadata;
    metadata.title = readString(values, prefix + ".title");
    metadata.author = readString(values, prefix + ".author");
    metadata.storagePath = readString(values, prefix + ".storagePath");
    metadata.createdAt = readString(values, prefix + ".createdAt");
    metadata.modifiedAt = readString(values, prefix + ".modifiedAt");
    metadata.documentId = readUuid(values, prefix + ".documentId");
    metadata.version = readNumber<std::uint32_t>(values, prefix + ".version", 1);
    metadata.appVersion = readString(values, prefix + ".appVersion");
    return metadata;
}

void writeCanvasMetadata(std::ostringstream &output,
                         const std::string &prefix,
                         const CanvasMetadata &metadata)
{
    writeLine(output, prefix + ".title", stringText(metadata.title));
    writeLine(output, prefix + ".author", stringText(metadata.author));
    writeLine(output, prefix + ".createdAt", stringText(metadata.createdAt));
    writeLine(output, prefix + ".modifiedAt", stringText(metadata.modifiedAt));
    writeLine(output, prefix + ".documentId", uuidText(metadata.documentId));
    writeLine(output, prefix + ".thumbnail", u32VectorText(metadata.thumbnail));
    writeLine(output, prefix + ".backgroundColor", numberText(metadata.backgroundColor));
    writeLine(output, prefix + ".unit", numberText(static_cast<int>(metadata.unit)));
    writeLine(output, prefix + ".intendedExportWidth", numberText(metadata.intendedExportWidth));
    writeLine(output, prefix + ".intendedExportHeight", numberText(metadata.intendedExportHeight));
    writeLine(output, prefix + ".dpiX", numberText(metadata.dpiX));
    writeLine(output, prefix + ".dpiY", numberText(metadata.dpiY));
    writeLine(output, prefix + ".colorSpace", stringText(metadata.colorSpace));
    writeLine(output, prefix + ".appVersion", stringText(metadata.appVersion));
}

CanvasMetadata readCanvasMetadata(const std::map<std::string, std::string> &values,
                                  const std::string &prefix)
{
    CanvasMetadata metadata;
    metadata.title = readString(values, prefix + ".title");
    metadata.author = readString(values, prefix + ".author");
    metadata.createdAt = readString(values, prefix + ".createdAt");
    metadata.modifiedAt = readString(values, prefix + ".modifiedAt");
    metadata.documentId = readUuid(values, prefix + ".documentId");
    metadata.thumbnail = readU32Vector(values, prefix + ".thumbnail");
    metadata.backgroundColor = readNumber<std::uint32_t>(values, prefix + ".backgroundColor");
    metadata.unit = static_cast<CanvasUnit>(readNumber<int>(values, prefix + ".unit"));
    metadata.intendedExportWidth = readNumber<Types::Pixel>(values, prefix + ".intendedExportWidth");
    metadata.intendedExportHeight = readNumber<Types::Pixel>(values, prefix + ".intendedExportHeight");
    metadata.dpiX = readNumber<Types::Scalar>(values, prefix + ".dpiX", 72.0);
    metadata.dpiY = readNumber<Types::Scalar>(values, prefix + ".dpiY", 72.0);
    metadata.colorSpace = readString(values, prefix + ".colorSpace", "sRGB");
    metadata.appVersion = readString(values, prefix + ".appVersion");
    return metadata;
}

void writeLayerMetadata(std::ostringstream &output,
                        const std::string &prefix,
                        const LayerMetadata &metadata)
{
    writeLine(output, prefix + ".id", uuidText(metadata.id));
    writeLine(output, prefix + ".name", stringText(metadata.name));
    writeLine(output, prefix + ".visible", boolText(metadata.visible));
    writeLine(output, prefix + ".opacity", numberText(metadata.opacity));
    writeLine(output, prefix + ".blendMode", numberText(static_cast<int>(metadata.blendMode)));
}

LayerMetadata readLayerMetadata(const std::map<std::string, std::string> &values,
                                const std::string &prefix)
{
    LayerMetadata metadata;
    metadata.id = readUuid(values, prefix + ".id");
    metadata.name = readString(values, prefix + ".name");
    metadata.visible = readBool(values, prefix + ".visible", true);
    metadata.opacity = readNumber<Types::Scalar>(values, prefix + ".opacity", 1.0);
    metadata.blendMode = static_cast<RasterBlendMode>(readNumber<int>(values, prefix + ".blendMode"));
    return metadata;
}

void writeSurface(std::ostringstream &output,
                  const std::string &prefix,
                  const DrawingSurface &surface)
{
    writeLine(output, prefix + ".width", numberText(surface.width));
    writeLine(output, prefix + ".height", numberText(surface.height));
    writeLine(output, prefix + ".pixelFormat", numberText(static_cast<int>(surface.pixelFormat)));
    writeLine(output, prefix + ".colorSpace", numberText(static_cast<int>(surface.colorSpace)));
    writeLine(output, prefix + ".dpiX", numberText(surface.dpiX));
    writeLine(output, prefix + ".dpiY", numberText(surface.dpiY));
    writeLine(output, prefix + ".backingStore", numberText(static_cast<int>(surface.backingStore)));
    writeLine(output, prefix + ".pixels", u32VectorText(surface.pixels));
    writeLine(output, prefix + ".dirtyRegion.count", numberText(surface.dirtyRegion.size()));
    for (std::size_t index = 0; index < surface.dirtyRegion.size(); ++index) {
        writeLine(output, prefix + ".dirtyRegion." + numberText(index), rectText(surface.dirtyRegion[index]));
    }
    writeLine(output, prefix + ".dirtyBounds", rectText(surface.dirtyBounds));
    writeLine(output, prefix + ".textureHandle", numberText(surface.textureHandle));
}

DrawingSurface readSurface(const std::map<std::string, std::string> &values,
                           const std::string &prefix)
{
    DrawingSurface surface;
    surface.width = readNumber<Types::Pixel>(values, prefix + ".width");
    surface.height = readNumber<Types::Pixel>(values, prefix + ".height");
    surface.pixelFormat = static_cast<SurfacePixelFormat>(readNumber<int>(values, prefix + ".pixelFormat"));
    surface.colorSpace = static_cast<SurfaceColorSpace>(readNumber<int>(values, prefix + ".colorSpace"));
    surface.dpiX = readNumber<Types::Scalar>(values, prefix + ".dpiX", 72.0);
    surface.dpiY = readNumber<Types::Scalar>(values, prefix + ".dpiY", 72.0);
    surface.backingStore = static_cast<SurfaceBackingStore>(readNumber<int>(values, prefix + ".backingStore"));
    surface.pixels = readU32Vector(values, prefix + ".pixels");
    const std::size_t dirtyCount = readNumber<std::size_t>(values, prefix + ".dirtyRegion.count");
    surface.dirtyRegion.reserve(dirtyCount);
    for (std::size_t index = 0; index < dirtyCount; ++index) {
        surface.dirtyRegion.push_back(readRect<DevicePixelRect>(values,
                                                                prefix + ".dirtyRegion." + numberText(index)));
    }
    surface.dirtyBounds = readRect<DevicePixelRect>(values, prefix + ".dirtyBounds");
    surface.textureHandle = readNumber<std::uint64_t>(values, prefix + ".textureHandle");
    return surface;
}

void writeStrokePoint(std::ostringstream &output,
                      const std::string &prefix,
                      const StrokePoint &point)
{
    writeLine(output, prefix + ".position", pointText(point.position));
    writeLine(output, prefix + ".pressure", numberText(point.pressure));
    writeLine(output, prefix + ".time", numberText(point.time));
    writeLine(output, prefix + ".velocity", numberText(point.velocity));
    writeLine(output, prefix + ".tiltX", numberText(point.tiltX));
    writeLine(output, prefix + ".tiltY", numberText(point.tiltY));
    writeLine(output, prefix + ".deviceState", numberText(point.deviceState));
    writeLine(output, prefix + ".arcLength", numberText(point.arcLength));
}

StrokePoint readStrokePoint(const std::map<std::string, std::string> &values,
                            const std::string &prefix)
{
    StrokePoint point;
    point.position = readPoint<DocumentPoint>(values, prefix + ".position");
    point.pressure = readNumber<Types::Scalar>(values, prefix + ".pressure", 1.0);
    point.time = readNumber<Types::Scalar>(values, prefix + ".time");
    point.velocity = readNumber<Types::Scalar>(values, prefix + ".velocity");
    point.tiltX = readNumber<Types::Scalar>(values, prefix + ".tiltX");
    point.tiltY = readNumber<Types::Scalar>(values, prefix + ".tiltY");
    point.deviceState = readNumber<std::uint32_t>(values, prefix + ".deviceState");
    point.arcLength = readNumber<Types::Scalar>(values, prefix + ".arcLength");
    return point;
}

void writeStrokeInput(std::ostringstream &output,
                      const std::string &prefix,
                      const StrokeInput &input)
{
    writeLine(output, prefix + ".count", numberText(input.points.size()));
    for (std::size_t index = 0; index < input.points.size(); ++index) {
        writeStrokePoint(output, prefix + "." + numberText(index), input.points[index]);
    }
}

StrokeInput readStrokeInput(const std::map<std::string, std::string> &values,
                            const std::string &prefix)
{
    StrokeInput input;
    const std::size_t count = readNumber<std::size_t>(values, prefix + ".count");
    input.points.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        input.points.push_back(readStrokePoint(values, prefix + "." + numberText(index)));
    }
    return input;
}

void writeStrokeCurve(std::ostringstream &output,
                      const std::string &prefix,
                      const StrokeCurve &curve)
{
    writeLine(output, prefix + ".count", numberText(curve.samples.size()));
    for (std::size_t index = 0; index < curve.samples.size(); ++index) {
        writeStrokePoint(output, prefix + "." + numberText(index), curve.samples[index]);
    }
}

StrokeCurve readStrokeCurve(const std::map<std::string, std::string> &values,
                            const std::string &prefix)
{
    StrokeCurve curve;
    const std::size_t count = readNumber<std::size_t>(values, prefix + ".count");
    curve.samples.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        curve.samples.push_back(readStrokePoint(values, prefix + "." + numberText(index)));
    }
    return curve;
}

void writeRasterizer(std::ostringstream &output,
                     const std::string &prefix,
                     const Rasterizer &rasterizer)
{
    writeLine(output, prefix + ".radius", numberText(rasterizer.radius));
    writeLine(output, prefix + ".argb", numberText(rasterizer.argb));
    writeLine(output, prefix + ".brushSize", numberText(rasterizer.brushSize));
    writeLine(output, prefix + ".brushWidth", numberText(rasterizer.brushWidth));
    writeLine(output, prefix + ".brushHeight", numberText(rasterizer.brushHeight));
    writeLine(output, prefix + ".brushAlpha", byteVectorText(rasterizer.brushAlpha));
    writeLine(output, prefix + ".spacing", numberText(rasterizer.spacing));
    writeLine(output, prefix + ".spacingRatio", numberText(rasterizer.spacingRatio));
    writeLine(output, prefix + ".opacity", numberText(rasterizer.opacity));
    writeLine(output, prefix + ".flow", numberText(rasterizer.flow));
    writeLine(output, prefix + ".hardness", numberText(rasterizer.hardness));
    writeLine(output, prefix + ".density", numberText(rasterizer.density));
    writeLine(output, prefix + ".pressureScale", numberText(rasterizer.pressureScale));
    writeLine(output, prefix + ".velocitySpacing", numberText(rasterizer.velocitySpacing));
    writeLine(output, prefix + ".warmupDistance", numberText(rasterizer.warmupDistance));
    writeLine(output, prefix + ".taperDistance", numberText(rasterizer.taperDistance));
    writeLine(output, prefix + ".rotationJitter", numberText(rasterizer.rotationJitter));
}

Rasterizer readRasterizer(const std::map<std::string, std::string> &values,
                          const std::string &prefix)
{
    Rasterizer rasterizer;
    rasterizer.radius = readNumber<Types::Pixel>(values, prefix + ".radius", 2);
    rasterizer.argb = readNumber<std::uint32_t>(values, prefix + ".argb", 0xFF000000U);
    rasterizer.brushSize = readNumber<Types::Scalar>(values, prefix + ".brushSize");
    rasterizer.brushWidth = readNumber<Types::Pixel>(values, prefix + ".brushWidth");
    rasterizer.brushHeight = readNumber<Types::Pixel>(values, prefix + ".brushHeight");
    rasterizer.brushAlpha = readTypesByteVector(values, prefix + ".brushAlpha");
    rasterizer.spacing = readNumber<Types::Scalar>(values, prefix + ".spacing", 1.0);
    rasterizer.spacingRatio = readNumber<Types::Scalar>(values, prefix + ".spacingRatio", 1.0);
    rasterizer.opacity = readNumber<Types::Scalar>(values, prefix + ".opacity", 1.0);
    rasterizer.flow = readNumber<Types::Scalar>(values, prefix + ".flow", 1.0);
    rasterizer.hardness = readNumber<Types::Scalar>(values, prefix + ".hardness", 1.0);
    rasterizer.density = readNumber<Types::Scalar>(values, prefix + ".density", 1.0);
    rasterizer.pressureScale = readNumber<Types::Scalar>(values, prefix + ".pressureScale");
    rasterizer.velocitySpacing = readNumber<Types::Scalar>(values, prefix + ".velocitySpacing");
    rasterizer.warmupDistance = readNumber<Types::Scalar>(values, prefix + ".warmupDistance");
    rasterizer.taperDistance = readNumber<Types::Scalar>(values, prefix + ".taperDistance");
    rasterizer.rotationJitter = readNumber<Types::Scalar>(values, prefix + ".rotationJitter");
    return rasterizer;
}

void writeBrushDynamics(std::ostringstream &output,
                        const std::string &prefix,
                        const BrushDynamics &dynamics)
{
    writeLine(output, prefix + ".pressureToSize", numberText(dynamics.pressureToSize));
    writeLine(output, prefix + ".pressureToOpacity", numberText(dynamics.pressureToOpacity));
    writeLine(output, prefix + ".pressureToFlow", numberText(dynamics.pressureToFlow));
    writeLine(output, prefix + ".velocityToSpacing", numberText(dynamics.velocityToSpacing));
    writeLine(output, prefix + ".velocityToOpacity", numberText(dynamics.velocityToOpacity));
    writeLine(output, prefix + ".velocityToDryOut", numberText(dynamics.velocityToDryOut));
    writeLine(output, prefix + ".tiltToRotation", boolText(dynamics.tiltToRotation));
    writeLine(output, prefix + ".tiltToEllipse", numberText(dynamics.tiltToEllipse));
    writeLine(output, prefix + ".tiltToTextureDirection", boolText(dynamics.tiltToTextureDirection));
    writeLine(output, prefix + ".rotationJitter", numberText(dynamics.rotationJitter));
    writeLine(output, prefix + ".grainJitter", numberText(dynamics.grainJitter));
}

BrushDynamics readBrushDynamics(const std::map<std::string, std::string> &values,
                                const std::string &prefix)
{
    BrushDynamics dynamics;
    dynamics.pressureToSize = readNumber<Types::Scalar>(values, prefix + ".pressureToSize");
    dynamics.pressureToOpacity = readNumber<Types::Scalar>(values, prefix + ".pressureToOpacity");
    dynamics.pressureToFlow = readNumber<Types::Scalar>(values, prefix + ".pressureToFlow");
    dynamics.velocityToSpacing = readNumber<Types::Scalar>(values, prefix + ".velocityToSpacing");
    dynamics.velocityToOpacity = readNumber<Types::Scalar>(values, prefix + ".velocityToOpacity");
    dynamics.velocityToDryOut = readNumber<Types::Scalar>(values, prefix + ".velocityToDryOut");
    dynamics.tiltToRotation = readBool(values, prefix + ".tiltToRotation");
    dynamics.tiltToEllipse = readNumber<Types::Scalar>(values, prefix + ".tiltToEllipse");
    dynamics.tiltToTextureDirection = readBool(values, prefix + ".tiltToTextureDirection");
    dynamics.rotationJitter = readNumber<Types::Scalar>(values, prefix + ".rotationJitter");
    dynamics.grainJitter = readNumber<Types::Scalar>(values, prefix + ".grainJitter");
    return dynamics;
}

void writeStrokeResampler(std::ostringstream &output,
                          const std::string &prefix,
                          const StrokeResampler &resampler)
{
    writeLine(output, prefix + ".mode", numberText(static_cast<int>(resampler.mode)));
    writeLine(output, prefix + ".sampleSpacing", numberText(resampler.sampleSpacing));
}

StrokeResampler readStrokeResampler(const std::map<std::string, std::string> &values,
                                    const std::string &prefix)
{
    StrokeResampler resampler;
    resampler.mode = static_cast<StrokeInterpolationMode>(readNumber<int>(values, prefix + ".mode", 1));
    resampler.sampleSpacing = readNumber<Types::Scalar>(values, prefix + ".sampleSpacing", 1.0);
    return resampler;
}

void writeBrushState(std::ostringstream &output,
                     const std::string &prefix,
                     const BrushState &brush)
{
    writeRasterizer(output, prefix + ".rasterizer", brush.rasterizer);
    writeBrushDynamics(output, prefix + ".dynamics", brush.dynamics);
    writeStrokeResampler(output, prefix + ".resampler", brush.resampler);
    writeLine(output, prefix + ".randomSeed", numberText(brush.randomSeed));
}

BrushState readBrushState(const std::map<std::string, std::string> &values,
                          const std::string &prefix)
{
    BrushState brush;
    brush.rasterizer = readRasterizer(values, prefix + ".rasterizer");
    brush.dynamics = readBrushDynamics(values, prefix + ".dynamics");
    brush.resampler = readStrokeResampler(values, prefix + ".resampler");
    brush.randomSeed = readNumber<std::uint32_t>(values, prefix + ".randomSeed");
    return brush;
}

void writeBrushDab(std::ostringstream &output,
                   const std::string &prefix,
                   const BrushDab &dab)
{
    writeLine(output, prefix + ".position", pointText(dab.position));
    writeLine(output, prefix + ".scale", numberText(dab.scale));
    writeLine(output, prefix + ".rotationRadians", numberText(dab.rotationRadians));
    writeLine(output, prefix + ".alpha", numberText(dab.alpha));
    writeLine(output, prefix + ".opacityCapScale", numberText(dab.opacityCapScale));
    writeLine(output, prefix + ".ellipseScaleX", numberText(dab.ellipseScaleX));
    writeLine(output, prefix + ".ellipseScaleY", numberText(dab.ellipseScaleY));
    writeLine(output, prefix + ".textureDirectionRadians", numberText(dab.textureDirectionRadians));
    writeLine(output, prefix + ".grain", numberText(dab.grain));
    writeLine(output, prefix + ".colorArgb", numberText(dab.colorArgb));
    writeLine(output, prefix + ".blendMode", numberText(static_cast<int>(dab.blendMode)));
    writeLine(output, prefix + ".sequenceIndex", numberText(dab.sequenceIndex));
}

BrushDab readBrushDab(const std::map<std::string, std::string> &values,
                      const std::string &prefix)
{
    BrushDab dab;
    dab.position = readPoint<DocumentPoint>(values, prefix + ".position");
    dab.scale = readNumber<Types::Scalar>(values, prefix + ".scale", 1.0);
    dab.rotationRadians = readNumber<Types::Scalar>(values, prefix + ".rotationRadians");
    dab.alpha = readNumber<Types::Scalar>(values, prefix + ".alpha", 1.0);
    dab.opacityCapScale = readNumber<Types::Scalar>(values, prefix + ".opacityCapScale", 1.0);
    dab.ellipseScaleX = readNumber<Types::Scalar>(values, prefix + ".ellipseScaleX", 1.0);
    dab.ellipseScaleY = readNumber<Types::Scalar>(values, prefix + ".ellipseScaleY", 1.0);
    dab.textureDirectionRadians = readNumber<Types::Scalar>(values, prefix + ".textureDirectionRadians");
    dab.grain = readNumber<Types::Scalar>(values, prefix + ".grain");
    dab.colorArgb = readNumber<std::uint32_t>(values, prefix + ".colorArgb", 0xFF000000U);
    dab.blendMode = static_cast<RasterBlendMode>(readNumber<int>(values, prefix + ".blendMode"));
    dab.sequenceIndex = readNumber<std::uint32_t>(values, prefix + ".sequenceIndex");
    return dab;
}

void writeStrokeCommand(std::ostringstream &output,
                        const std::string &prefix,
                        const StrokeCommand &command)
{
    writeStrokeInput(output, prefix + ".path.rawInput", command.path.rawInput);
    writeStrokeInput(output, prefix + ".path.renderedInput", command.path.renderedInput);
    writeStrokeCurve(output, prefix + ".path.renderedCurve", command.path.renderedCurve);
    writeBrushState(output, prefix + ".brush", command.brush);
    writeLine(output, prefix + ".dabs.count", numberText(command.dabs.size()));
    for (std::size_t index = 0; index < command.dabs.size(); ++index) {
        writeBrushDab(output, prefix + ".dabs." + numberText(index), command.dabs[index]);
    }
    writeLine(output, prefix + ".dabDirtyBounds.count", numberText(command.dabDirtyBounds.size()));
    for (std::size_t index = 0; index < command.dabDirtyBounds.size(); ++index) {
        writeLine(output,
                  prefix + ".dabDirtyBounds." + numberText(index),
                  rectText(command.dabDirtyBounds[index]));
    }
    writeLine(output, prefix + ".dirtyBounds", rectText(command.dirtyBounds));
}

StrokeCommand readStrokeCommand(const std::map<std::string, std::string> &values,
                                const std::string &prefix)
{
    StrokeCommand command;
    command.path.rawInput = readStrokeInput(values, prefix + ".path.rawInput");
    command.path.renderedInput = readStrokeInput(values, prefix + ".path.renderedInput");
    command.path.renderedCurve = readStrokeCurve(values, prefix + ".path.renderedCurve");
    command.brush = readBrushState(values, prefix + ".brush");

    const std::size_t dabCount = readNumber<std::size_t>(values, prefix + ".dabs.count");
    command.dabs.reserve(dabCount);
    for (std::size_t index = 0; index < dabCount; ++index) {
        command.dabs.push_back(readBrushDab(values, prefix + ".dabs." + numberText(index)));
    }

    const std::size_t boundsCount = readNumber<std::size_t>(values, prefix + ".dabDirtyBounds.count");
    command.dabDirtyBounds.reserve(boundsCount);
    for (std::size_t index = 0; index < boundsCount; ++index) {
        command.dabDirtyBounds.push_back(readRect<DocumentRect>(values,
                                                               prefix + ".dabDirtyBounds." + numberText(index)));
    }
    command.dirtyBounds = readRect<DocumentRect>(values, prefix + ".dirtyBounds");
    return command;
}

void writeBrushSnapshot(std::ostringstream &output,
                        const std::string &prefix,
                        const BrushSnapshot &brush)
{
    writeLine(output, prefix + ".brushId", uuidText(brush.brushId));
    writeLine(output, prefix + ".tip.width", numberText(brush.tip.width));
    writeLine(output, prefix + ".tip.height", numberText(brush.tip.height));
    writeLine(output, prefix + ".tip.mask", byteVectorText(brush.tip.mask));
    writeLine(output, prefix + ".size", numberText(brush.size));
    writeLine(output, prefix + ".opacity", numberText(brush.opacity));
    writeLine(output, prefix + ".hardness", numberText(brush.hardness));
    writeLine(output, prefix + ".flow", numberText(brush.flow));
    writeLine(output, prefix + ".density", numberText(brush.density));
}

BrushSnapshot readBrushSnapshot(const std::map<std::string, std::string> &values,
                                const std::string &prefix)
{
    BrushSnapshot brush;
    brush.brushId = readUuid(values, prefix + ".brushId");
    brush.tip.width = readNumber<int>(values, prefix + ".tip.width");
    brush.tip.height = readNumber<int>(values, prefix + ".tip.height");
    brush.tip.mask = readByteVector(values, prefix + ".tip.mask");
    brush.size = readNumber<float>(values, prefix + ".size");
    brush.opacity = readNumber<float>(values, prefix + ".opacity");
    brush.hardness = readNumber<float>(values, prefix + ".hardness");
    brush.flow = readNumber<float>(values, prefix + ".flow");
    brush.density = readNumber<float>(values, prefix + ".density");
    return brush;
}

void writeCommand(std::ostringstream &output,
                  const std::string &prefix,
                  const Command &command)
{
    writeLine(output, prefix + ".sequence", numberText(command.sequence));
    writeLine(output, prefix + ".label", stringText(command.label));
    writeLine(output, prefix + ".targetId", uuidText(command.targetId));
}

Command readCommand(const std::map<std::string, std::string> &values,
                    const std::string &prefix)
{
    Command command;
    command.sequence = readNumber<std::uint64_t>(values, prefix + ".sequence");
    command.label = readString(values, prefix + ".label");
    command.targetId = readUuid(values, prefix + ".targetId");
    return command;
}

void writeHistory(std::ostringstream &output,
                  const std::string &prefix,
                  const HistoryStack &history)
{
    writeLine(output, prefix + ".cursor", numberText(history.cursor));
    writeLine(output, prefix + ".undo.count", numberText(history.undoCommands.size()));
    for (std::size_t index = 0; index < history.undoCommands.size(); ++index) {
        writeCommand(output, prefix + ".undo." + numberText(index), history.undoCommands[index]);
    }
    writeLine(output, prefix + ".redo.count", numberText(history.redoCommands.size()));
    for (std::size_t index = 0; index < history.redoCommands.size(); ++index) {
        writeCommand(output, prefix + ".redo." + numberText(index), history.redoCommands[index]);
    }
}

HistoryStack readHistory(const std::map<std::string, std::string> &values,
                         const std::string &prefix)
{
    HistoryStack history;
    history.cursor = readNumber<std::size_t>(values, prefix + ".cursor");
    const std::size_t undoCount = readNumber<std::size_t>(values, prefix + ".undo.count");
    history.undoCommands.reserve(undoCount);
    for (std::size_t index = 0; index < undoCount; ++index) {
        history.undoCommands.push_back(readCommand(values, prefix + ".undo." + numberText(index)));
    }
    const std::size_t redoCount = readNumber<std::size_t>(values, prefix + ".redo.count");
    history.redoCommands.reserve(redoCount);
    for (std::size_t index = 0; index < redoCount; ++index) {
        history.redoCommands.push_back(readCommand(values, prefix + ".redo." + numberText(index)));
    }
    return history;
}

void writeColorSpace(std::ostringstream &output,
                     const std::string &prefix,
                     const ColorSpace &colorSpace)
{
    writeLine(output, prefix + ".name", stringText(colorSpace.name));
    writeLine(output, prefix + ".iccProfile", byteVectorText(colorSpace.iccProfile));
    writeLine(output, prefix + ".linear", boolText(colorSpace.linear));
}

ColorSpace readColorSpace(const std::map<std::string, std::string> &values,
                          const std::string &prefix)
{
    ColorSpace colorSpace;
    colorSpace.name = readString(values, prefix + ".name", "sRGB");
    colorSpace.iccProfile = readByteVector(values, prefix + ".iccProfile");
    colorSpace.linear = readBool(values, prefix + ".linear");
    return colorSpace;
}

void writeAsset(std::ostringstream &output,
                const std::string &prefix,
                const DocumentAsset &asset)
{
    writeLine(output, prefix + ".id", uuidText(asset.id));
    writeLine(output, prefix + ".name", stringText(asset.name));
    writeLine(output, prefix + ".mimeType", stringText(asset.mimeType));
    writeLine(output, prefix + ".bytes", byteVectorText(asset.bytes));
}

DocumentAsset readAsset(const std::map<std::string, std::string> &values,
                        const std::string &prefix)
{
    DocumentAsset asset;
    asset.id = readUuid(values, prefix + ".id");
    asset.name = readString(values, prefix + ".name");
    asset.mimeType = readString(values, prefix + ".mimeType");
    asset.bytes = readByteVector(values, prefix + ".bytes");
    return asset;
}

} // namespace

DocumentArchive makeDocumentArchive(const PaintDocument &document)
{
    DocumentArchive archive;
    archive.document = document;
    return archive;
}

std::string serializeDocumentArchive(const DocumentArchive &archive)
{
    std::ostringstream output;
    writeLine(output, "formatMagic", stringText(archive.formatMagic));
    writeLine(output, "formatVersion", numberText(archive.formatVersion));
    writeDocumentMetadata(output, "document.metadata", archive.document.metadata);

    writeLine(output, "document.canvases.count", numberText(archive.document.canvases.size()));
    for (std::size_t canvasIndex = 0; canvasIndex < archive.document.canvases.size(); ++canvasIndex) {
        const std::string canvasPrefix = "document.canvases." + numberText(canvasIndex);
        const Canvas &canvas = archive.document.canvases[canvasIndex];
        writeSurface(output, canvasPrefix + ".surface", canvas.surface);
        writeCanvasMetadata(output, canvasPrefix + ".metadata", canvas.metadata);

        writeLine(output, canvasPrefix + ".layers.count", numberText(canvas.layers.layers.size()));
        for (std::size_t layerIndex = 0; layerIndex < canvas.layers.layers.size(); ++layerIndex) {
            const std::string layerPrefix = canvasPrefix + ".layers." + numberText(layerIndex);
            writeSurface(output, layerPrefix + ".surface", canvas.layers.layers[layerIndex].surface);
            writeLayerMetadata(output, layerPrefix + ".metadata", canvas.layers.layers[layerIndex].metadata);
        }
        writeLine(output, canvasPrefix + ".layers.activeLayerIndex", numberText(canvas.layers.activeLayerIndex));

        writeLine(output, canvasPrefix + ".strokes.count", numberText(canvas.strokes.strokes.size()));
        for (std::size_t strokeIndex = 0; strokeIndex < canvas.strokes.strokes.size(); ++strokeIndex) {
            writeStrokeCommand(output,
                               canvasPrefix + ".strokes." + numberText(strokeIndex),
                               canvas.strokes.strokes[strokeIndex]);
        }
    }

    writeLine(output, "brushSources.count", numberText(archive.brushSources.size()));
    for (std::size_t index = 0; index < archive.brushSources.size(); ++index) {
        writeBrushSnapshot(output, "brushSources." + numberText(index), archive.brushSources[index]);
    }

    writeLine(output, "colorSpaces.count", numberText(archive.colorSpaces.size()));
    for (std::size_t index = 0; index < archive.colorSpaces.size(); ++index) {
        writeColorSpace(output, "colorSpaces." + numberText(index), archive.colorSpaces[index]);
    }

    writeLine(output, "assets.count", numberText(archive.assets.size()));
    for (std::size_t index = 0; index < archive.assets.size(); ++index) {
        writeAsset(output, "assets." + numberText(index), archive.assets[index]);
    }

    writeHistory(output, "history", archive.history);
    return output.str();
}

DocumentArchive deserializeDocumentArchive(const std::string &payload)
{
    const std::map<std::string, std::string> values = parsePayload(payload);

    DocumentArchive archive;
    archive.formatMagic = readString(values, "formatMagic", "iiPaintDocument");
    archive.formatVersion = readNumber<std::uint32_t>(values, "formatVersion", 1);
    archive.document.metadata = readDocumentMetadata(values, "document.metadata");

    const std::size_t canvasCount = readNumber<std::size_t>(values, "document.canvases.count");
    archive.document.canvases.reserve(canvasCount);
    for (std::size_t canvasIndex = 0; canvasIndex < canvasCount; ++canvasIndex) {
        const std::string canvasPrefix = "document.canvases." + numberText(canvasIndex);
        Canvas canvas;
        canvas.surface = readSurface(values, canvasPrefix + ".surface");
        canvas.metadata = readCanvasMetadata(values, canvasPrefix + ".metadata");

        const std::size_t layerCount = readNumber<std::size_t>(values, canvasPrefix + ".layers.count");
        canvas.layers.layers.reserve(layerCount);
        for (std::size_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
            const std::string layerPrefix = canvasPrefix + ".layers." + numberText(layerIndex);
            Layer layer;
            layer.surface = readSurface(values, layerPrefix + ".surface");
            layer.metadata = readLayerMetadata(values, layerPrefix + ".metadata");
            canvas.layers.layers.push_back(layer);
        }
        canvas.layers.activeLayerIndex = readNumber<std::size_t>(values, canvasPrefix + ".layers.activeLayerIndex");

        const std::size_t strokeCount = readNumber<std::size_t>(values, canvasPrefix + ".strokes.count");
        canvas.strokes.strokes.reserve(strokeCount);
        for (std::size_t strokeIndex = 0; strokeIndex < strokeCount; ++strokeIndex) {
            canvas.strokes.strokes.push_back(readStrokeCommand(values,
                                                               canvasPrefix + ".strokes." + numberText(strokeIndex)));
        }
        archive.document.canvases.push_back(canvas);
    }

    const std::size_t brushCount = readNumber<std::size_t>(values, "brushSources.count");
    archive.brushSources.reserve(brushCount);
    for (std::size_t index = 0; index < brushCount; ++index) {
        archive.brushSources.push_back(readBrushSnapshot(values, "brushSources." + numberText(index)));
    }

    const std::size_t colorSpaceCount = readNumber<std::size_t>(values, "colorSpaces.count");
    archive.colorSpaces.reserve(colorSpaceCount);
    for (std::size_t index = 0; index < colorSpaceCount; ++index) {
        archive.colorSpaces.push_back(readColorSpace(values, "colorSpaces." + numberText(index)));
    }

    const std::size_t assetCount = readNumber<std::size_t>(values, "assets.count");
    archive.assets.reserve(assetCount);
    for (std::size_t index = 0; index < assetCount; ++index) {
        archive.assets.push_back(readAsset(values, "assets." + numberText(index)));
    }

    archive.history = readHistory(values, "history");
    return archive;
}
