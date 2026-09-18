//
// Created by Justmoong on 2026 May 26.
//

#include "BrushPresetSerializer.h"

#include <cstddef>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>

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

std::string byteVectorText(const std::vector<Types::Byte> &bytes)
{
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const Types::Byte byte : bytes) {
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

std::vector<Types::Byte> readByteVector(const std::map<std::string, std::string> &values,
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

std::vector<std::byte> readStdByteVector(const std::map<std::string, std::string> &values,
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

void writeDynamicsResponseCurve(std::ostringstream &output,
                                const std::string &prefix,
                                const BrushDynamicsResponseCurve &curve)
{
    writeLine(output, prefix + ".enabled", boolText(curve.enabled));
    writeLine(output, prefix + ".min", numberText(curve.min));
    writeLine(output, prefix + ".center", numberText(curve.center));
    writeLine(output, prefix + ".max", numberText(curve.max));
    writeLine(output, prefix + ".jitter", numberText(curve.jitter));
    writeLine(output, prefix + ".easing", numberText(static_cast<int>(curve.easing)));
}

void writeDynamicsPropertyResponse(std::ostringstream &output,
                                   const std::string &prefix,
                                   const BrushDynamicsPropertyResponse &response)
{
    writeLine(output, prefix + ".enabled", boolText(response.enabled));
    writeLine(output, prefix + ".neutral", numberText(response.neutral));
    writeLine(output, prefix + ".combineMode", numberText(static_cast<int>(response.combineMode)));
    writeDynamicsResponseCurve(output, prefix + ".pressure", response.pressure);
    writeDynamicsResponseCurve(output, prefix + ".velocity", response.velocity);
    writeDynamicsResponseCurve(output, prefix + ".tilt", response.tilt);
    writeDynamicsResponseCurve(output, prefix + ".random", response.random);
}

void writeDynamics(std::ostringstream &output, const BrushDynamics &dynamics)
{
    writeLine(output, "dynamics.pressureInputEnabled", boolText(dynamics.pressureInputEnabled));
    writeLine(output, "dynamics.velocityInputEnabled", boolText(dynamics.velocityInputEnabled));
    writeLine(output, "dynamics.tiltInputEnabled", boolText(dynamics.tiltInputEnabled));
    writeLine(output, "dynamics.randomInputEnabled", boolText(dynamics.randomInputEnabled));
    writeLine(output, "dynamics.pressureToSizeEnabled", boolText(dynamics.pressureToSizeEnabled));
    writeLine(output, "dynamics.pressureToOpacityEnabled", boolText(dynamics.pressureToOpacityEnabled));
    writeLine(output, "dynamics.pressureToFlowEnabled", boolText(dynamics.pressureToFlowEnabled));
    writeLine(output, "dynamics.velocityToSpacingEnabled", boolText(dynamics.velocityToSpacingEnabled));
    writeLine(output, "dynamics.velocityToOpacityEnabled", boolText(dynamics.velocityToOpacityEnabled));
    writeLine(output, "dynamics.velocityToDryOutEnabled", boolText(dynamics.velocityToDryOutEnabled));
    writeLine(output, "dynamics.tiltToEllipseEnabled", boolText(dynamics.tiltToEllipseEnabled));
    writeLine(output, "dynamics.rotationJitterEnabled", boolText(dynamics.rotationJitterEnabled));
    writeLine(output, "dynamics.grainJitterEnabled", boolText(dynamics.grainJitterEnabled));
    writeLine(output, "dynamics.pressureToSize", numberText(dynamics.pressureToSize));
    writeLine(output, "dynamics.pressureToOpacity", numberText(dynamics.pressureToOpacity));
    writeLine(output, "dynamics.pressureToFlow", numberText(dynamics.pressureToFlow));
    writeLine(output, "dynamics.velocityToSpacing", numberText(dynamics.velocityToSpacing));
    writeLine(output, "dynamics.velocityToOpacity", numberText(dynamics.velocityToOpacity));
    writeLine(output, "dynamics.velocityToDryOut", numberText(dynamics.velocityToDryOut));
    writeLine(output, "dynamics.tiltToRotation", boolText(dynamics.tiltToRotation));
    writeLine(output, "dynamics.tiltToEllipse", numberText(dynamics.tiltToEllipse));
    writeLine(output, "dynamics.tiltToTextureDirection", boolText(dynamics.tiltToTextureDirection));
    writeLine(output, "dynamics.rotationJitter", numberText(dynamics.rotationJitter));
    writeLine(output, "dynamics.grainJitter", numberText(dynamics.grainJitter));
    writeDynamicsPropertyResponse(output, "dynamics.sizeResponse", dynamics.sizeResponse);
    writeDynamicsPropertyResponse(output, "dynamics.flowResponse", dynamics.flowResponse);
    writeDynamicsPropertyResponse(output, "dynamics.opacityResponse", dynamics.opacityResponse);
    writeDynamicsPropertyResponse(output, "dynamics.spacingResponse", dynamics.spacingResponse);
    writeDynamicsPropertyResponse(output, "dynamics.scatterResponse", dynamics.scatterResponse);
    writeDynamicsPropertyResponse(output, "dynamics.rotationResponse", dynamics.rotationResponse);
    writeDynamicsPropertyResponse(output, "dynamics.textureDepthResponse", dynamics.textureDepthResponse);
    writeDynamicsPropertyResponse(output, "dynamics.wetnessResponse", dynamics.wetnessResponse);
    writeDynamicsPropertyResponse(output, "dynamics.dryOutResponse", dynamics.dryOutResponse);
    writeDynamicsPropertyResponse(output, "dynamics.bristleSpreadResponse", dynamics.bristleSpreadResponse);
}

BrushDynamicsResponseCurve readDynamicsResponseCurve(const std::map<std::string, std::string> &values,
                                                     const std::string &prefix,
                                                     BrushDynamicsResponseCurve curve)
{
    curve.enabled = readBool(values, prefix + ".enabled", curve.enabled);
    curve.min = readNumber<Types::Scalar>(values, prefix + ".min", curve.min);
    curve.center = readNumber<Types::Scalar>(values, prefix + ".center", curve.center);
    curve.max = readNumber<Types::Scalar>(values, prefix + ".max", curve.max);
    curve.jitter = readNumber<Types::Scalar>(values, prefix + ".jitter", curve.jitter);
    curve.easing = static_cast<BrushDynamicsEasing>(
            readNumber<int>(values, prefix + ".easing", static_cast<int>(curve.easing)));
    return curve;
}

BrushDynamicsPropertyResponse readDynamicsPropertyResponse(const std::map<std::string, std::string> &values,
                                                           const std::string &prefix,
                                                           BrushDynamicsPropertyResponse response)
{
    response.enabled = readBool(values, prefix + ".enabled", response.enabled);
    response.neutral = readNumber<Types::Scalar>(values, prefix + ".neutral", response.neutral);
    response.combineMode = static_cast<BrushDynamicsCombineMode>(
            readNumber<int>(values, prefix + ".combineMode", static_cast<int>(response.combineMode)));
    response.pressure = readDynamicsResponseCurve(values, prefix + ".pressure", response.pressure);
    response.velocity = readDynamicsResponseCurve(values, prefix + ".velocity", response.velocity);
    response.tilt = readDynamicsResponseCurve(values, prefix + ".tilt", response.tilt);
    response.random = readDynamicsResponseCurve(values, prefix + ".random", response.random);
    return response;
}

BrushDynamics readDynamics(const std::map<std::string, std::string> &values)
{
    BrushDynamics dynamics;
    dynamics.pressureInputEnabled = readBool(values, "dynamics.pressureInputEnabled", true);
    dynamics.velocityInputEnabled = readBool(values, "dynamics.velocityInputEnabled", true);
    dynamics.tiltInputEnabled = readBool(values, "dynamics.tiltInputEnabled", true);
    dynamics.randomInputEnabled = readBool(values, "dynamics.randomInputEnabled", true);
    dynamics.pressureToSizeEnabled = readBool(values, "dynamics.pressureToSizeEnabled", true);
    dynamics.pressureToOpacityEnabled = readBool(values, "dynamics.pressureToOpacityEnabled", true);
    dynamics.pressureToFlowEnabled = readBool(values, "dynamics.pressureToFlowEnabled", true);
    dynamics.velocityToSpacingEnabled = readBool(values, "dynamics.velocityToSpacingEnabled", true);
    dynamics.velocityToOpacityEnabled = readBool(values, "dynamics.velocityToOpacityEnabled", true);
    dynamics.velocityToDryOutEnabled = readBool(values, "dynamics.velocityToDryOutEnabled", true);
    dynamics.tiltToEllipseEnabled = readBool(values, "dynamics.tiltToEllipseEnabled", true);
    dynamics.rotationJitterEnabled = readBool(values, "dynamics.rotationJitterEnabled", true);
    dynamics.grainJitterEnabled = readBool(values, "dynamics.grainJitterEnabled", true);
    dynamics.pressureToSize = readNumber<Types::Scalar>(values, "dynamics.pressureToSize");
    dynamics.pressureToOpacity = readNumber<Types::Scalar>(values, "dynamics.pressureToOpacity");
    dynamics.pressureToFlow = readNumber<Types::Scalar>(values, "dynamics.pressureToFlow");
    dynamics.velocityToSpacing = readNumber<Types::Scalar>(values, "dynamics.velocityToSpacing");
    dynamics.velocityToOpacity = readNumber<Types::Scalar>(values, "dynamics.velocityToOpacity");
    dynamics.velocityToDryOut = readNumber<Types::Scalar>(values, "dynamics.velocityToDryOut");
    dynamics.tiltToRotation = readBool(values, "dynamics.tiltToRotation");
    dynamics.tiltToEllipse = readNumber<Types::Scalar>(values, "dynamics.tiltToEllipse");
    dynamics.tiltToTextureDirection = readBool(values, "dynamics.tiltToTextureDirection");
    dynamics.rotationJitter = readNumber<Types::Scalar>(values, "dynamics.rotationJitter");
    dynamics.grainJitter = readNumber<Types::Scalar>(values, "dynamics.grainJitter");
    dynamics.sizeResponse = readDynamicsPropertyResponse(values, "dynamics.sizeResponse", dynamics.sizeResponse);
    dynamics.flowResponse = readDynamicsPropertyResponse(values, "dynamics.flowResponse", dynamics.flowResponse);
    dynamics.opacityResponse = readDynamicsPropertyResponse(values, "dynamics.opacityResponse", dynamics.opacityResponse);
    dynamics.spacingResponse = readDynamicsPropertyResponse(values, "dynamics.spacingResponse", dynamics.spacingResponse);
    dynamics.scatterResponse = readDynamicsPropertyResponse(values, "dynamics.scatterResponse", dynamics.scatterResponse);
    dynamics.rotationResponse = readDynamicsPropertyResponse(values, "dynamics.rotationResponse", dynamics.rotationResponse);
    dynamics.textureDepthResponse = readDynamicsPropertyResponse(values, "dynamics.textureDepthResponse", dynamics.textureDepthResponse);
    dynamics.wetnessResponse = readDynamicsPropertyResponse(values, "dynamics.wetnessResponse", dynamics.wetnessResponse);
    dynamics.dryOutResponse = readDynamicsPropertyResponse(values, "dynamics.dryOutResponse", dynamics.dryOutResponse);
    dynamics.bristleSpreadResponse = readDynamicsPropertyResponse(values, "dynamics.bristleSpreadResponse", dynamics.bristleSpreadResponse);
    return dynamics;
}

void writeTextureAssetCache(std::ostringstream &output,
                            const std::string &prefix,
                            const BrushTextureAssetCache &cache)
{
    writeLine(output, prefix + ".enabled", boolText(cache.enabled));
    writeLine(output, prefix + ".assetId", uuidText(cache.assetId));
    writeLine(output, prefix + ".cacheKey", stringText(cache.cacheKey));
    writeLine(output, prefix + ".revision", numberText(cache.revision));
    writeLine(output, prefix + ".width", numberText(cache.width));
    writeLine(output, prefix + ".height", numberText(cache.height));
    writeLine(output, prefix + ".alpha", byteVectorText(cache.alpha));
}

void writeTexture(std::ostringstream &output, const std::string &prefix, const BrushTexture &texture)
{
    writeLine(output, prefix + ".enabled", boolText(texture.enabled));
    writeLine(output, prefix + ".space", numberText(static_cast<int>(texture.space)));
    writeLine(output, prefix + ".width", numberText(texture.width));
    writeLine(output, prefix + ".height", numberText(texture.height));
    writeLine(output, prefix + ".alpha", byteVectorText(texture.alpha));
    writeLine(output, prefix + ".grainStrength", numberText(texture.grainStrength));
    writeLine(output, prefix + ".strength", numberText(texture.strength));
    writeLine(output, prefix + ".scale", numberText(texture.scale));
    writeLine(output, prefix + ".rotationRadians", numberText(texture.rotationRadians));
    writeLine(output, prefix + ".offsetX", numberText(texture.offsetX));
    writeLine(output, prefix + ".offsetY", numberText(texture.offsetY));
    writeLine(output, prefix + ".scaleJitter", numberText(texture.scaleJitter));
    writeLine(output, prefix + ".rotationJitter", numberText(texture.rotationJitter));
    writeTextureAssetCache(output, prefix + ".assetCache", texture.assetCache);
}

BrushTextureAssetCache readTextureAssetCache(const std::map<std::string, std::string> &values,
                                             const std::string &prefix)
{
    BrushTextureAssetCache cache;
    cache.enabled = readBool(values, prefix + ".enabled");
    cache.assetId = readUuid(values, prefix + ".assetId");
    cache.cacheKey = readString(values, prefix + ".cacheKey");
    cache.revision = readNumber<std::uint64_t>(values, prefix + ".revision");
    cache.width = readNumber<Types::Pixel>(values, prefix + ".width");
    cache.height = readNumber<Types::Pixel>(values, prefix + ".height");
    cache.alpha = readByteVector(values, prefix + ".alpha");
    return cache;
}

BrushTexture readTexture(const std::map<std::string, std::string> &values, const std::string &prefix)
{
    BrushTexture texture;
    texture.enabled = readBool(values, prefix + ".enabled");
    texture.space = static_cast<BrushTextureSpace>(
            readNumber<int>(values, prefix + ".space", static_cast<int>(texture.space)));
    texture.width = readNumber<Types::Pixel>(values, prefix + ".width");
    texture.height = readNumber<Types::Pixel>(values, prefix + ".height");
    texture.alpha = readByteVector(values, prefix + ".alpha");
    texture.grainStrength = readNumber<Types::Scalar>(values, prefix + ".grainStrength");
    texture.strength = readNumber<Types::Scalar>(values, prefix + ".strength", 1.0);
    texture.scale = readNumber<Types::Scalar>(values, prefix + ".scale", 1.0);
    texture.rotationRadians = readNumber<Types::Scalar>(values, prefix + ".rotationRadians");
    texture.offsetX = readNumber<Types::Scalar>(values, prefix + ".offsetX");
    texture.offsetY = readNumber<Types::Scalar>(values, prefix + ".offsetY");
    texture.scaleJitter = readNumber<Types::Scalar>(values, prefix + ".scaleJitter");
    texture.rotationJitter = readNumber<Types::Scalar>(values, prefix + ".rotationJitter");
    texture.assetCache = readTextureAssetCache(values, prefix + ".assetCache");
    return texture;
}

void writeMaterial(std::ostringstream &output, const BrushMaterial &material)
{
    writeTexture(output, "material.texture", material.texture);
    writeTexture(output, "material.paperGrain", material.paperGrain);
    writeLine(output, "material.dualBrush.enabled", boolText(material.dualBrush.enabled));
    writeLine(output, "material.dualBrush.compositeMode", numberText(static_cast<int>(material.dualBrush.compositeMode)));
    writeLine(output, "material.dualBrush.scale", numberText(material.dualBrush.scale));
    writeLine(output, "material.dualBrush.spacingRatio", numberText(material.dualBrush.spacingRatio));
    writeLine(output, "material.dualBrush.opacity", numberText(material.dualBrush.opacity));
    writeLine(output, "material.dualBrush.rotationRadians", numberText(material.dualBrush.rotationRadians));
    writeLine(output, "material.dualBrush.offsetX", numberText(material.dualBrush.offsetX));
    writeLine(output, "material.dualBrush.offsetY", numberText(material.dualBrush.offsetY));
    writeLine(output, "material.dualBrush.scaleJitter", numberText(material.dualBrush.scaleJitter));
    writeLine(output, "material.dualBrush.rotationJitter", numberText(material.dualBrush.rotationJitter));
    writeLine(output, "material.dualBrush.alpha", byteVectorText(material.dualBrush.alpha));
    writeLine(output, "material.dualBrush.width", numberText(material.dualBrush.width));
    writeLine(output, "material.dualBrush.height", numberText(material.dualBrush.height));
    writeLine(output, "material.scatter.enabled", boolText(material.scatter.enabled));
    writeLine(output, "material.scatter.radius", numberText(material.scatter.radius));
    writeLine(output, "material.scatter.count", numberText(material.scatter.count));
    writeLine(output, "material.simulation.enabled", boolText(material.simulation.enabled));
    writeLine(output, "material.simulation.model", numberText(static_cast<int>(material.simulation.model)));
    writeLine(output, "material.simulation.wetness", numberText(material.simulation.wetness));
    writeLine(output, "material.simulation.smudgeStrength", numberText(material.simulation.smudgeStrength));
    writeLine(output, "material.simulation.mixStrength", numberText(material.simulation.mixStrength));
    writeLine(output, "material.simulation.pickup", numberText(material.simulation.pickup));
    writeLine(output, "material.simulation.deposit", numberText(material.simulation.deposit));
    writeLine(output, "material.bristle.enabled", boolText(material.bristle.enabled));
    writeLine(output, "material.bristle.shape", numberText(static_cast<int>(material.bristle.shape)));
    writeLine(output, "material.bristle.count", numberText(material.bristle.count));
    writeLine(output, "material.bristle.length", numberText(material.bristle.length));
    writeLine(output, "material.bristle.stiffness", numberText(material.bristle.stiffness));
}

BrushMaterial readMaterial(const std::map<std::string, std::string> &values)
{
    BrushMaterial material;
    material.texture = readTexture(values, "material.texture");
    material.paperGrain = readTexture(values, "material.paperGrain");
    material.dualBrush.enabled = readBool(values, "material.dualBrush.enabled");
    material.dualBrush.compositeMode = static_cast<DualBrushCompositeMode>(
            readNumber<int>(values, "material.dualBrush.compositeMode", static_cast<int>(material.dualBrush.compositeMode)));
    material.dualBrush.scale = readNumber<Types::Scalar>(values, "material.dualBrush.scale", 1.0);
    material.dualBrush.spacingRatio = readNumber<Types::Scalar>(values, "material.dualBrush.spacingRatio", 1.0);
    material.dualBrush.opacity = readNumber<Types::Scalar>(values, "material.dualBrush.opacity", 1.0);
    material.dualBrush.rotationRadians = readNumber<Types::Scalar>(values, "material.dualBrush.rotationRadians");
    material.dualBrush.offsetX = readNumber<Types::Scalar>(values, "material.dualBrush.offsetX");
    material.dualBrush.offsetY = readNumber<Types::Scalar>(values, "material.dualBrush.offsetY");
    material.dualBrush.scaleJitter = readNumber<Types::Scalar>(values, "material.dualBrush.scaleJitter");
    material.dualBrush.rotationJitter = readNumber<Types::Scalar>(values, "material.dualBrush.rotationJitter");
    material.dualBrush.alpha = readByteVector(values, "material.dualBrush.alpha");
    material.dualBrush.width = readNumber<Types::Pixel>(values, "material.dualBrush.width");
    material.dualBrush.height = readNumber<Types::Pixel>(values, "material.dualBrush.height");
    material.scatter.enabled = readBool(values, "material.scatter.enabled");
    material.scatter.radius = readNumber<Types::Scalar>(values, "material.scatter.radius");
    material.scatter.count = readNumber<std::uint32_t>(values, "material.scatter.count", 1);
    material.simulation.enabled = readBool(values, "material.simulation.enabled");
    material.simulation.model = static_cast<BrushSimulationModel>(readNumber<int>(values, "material.simulation.model"));
    material.simulation.wetness = readNumber<Types::Scalar>(values, "material.simulation.wetness");
    material.simulation.smudgeStrength = readNumber<Types::Scalar>(values, "material.simulation.smudgeStrength");
    material.simulation.mixStrength = readNumber<Types::Scalar>(values, "material.simulation.mixStrength");
    material.simulation.pickup = readNumber<Types::Scalar>(values, "material.simulation.pickup");
    material.simulation.deposit = readNumber<Types::Scalar>(values, "material.simulation.deposit", 1.0);
    material.bristle.enabled = readBool(values, "material.bristle.enabled");
    material.bristle.shape = static_cast<BristleShape>(readNumber<int>(values, "material.bristle.shape"));
    material.bristle.count = readNumber<std::uint32_t>(values, "material.bristle.count");
    material.bristle.length = readNumber<Types::Scalar>(values, "material.bristle.length");
    material.bristle.stiffness = readNumber<Types::Scalar>(values, "material.bristle.stiffness", 1.0);
    return material;
}

} // namespace

std::string serializeBrushPreset(const BrushPreset &preset)
{
    std::ostringstream output;
    writeLine(output, "formatMagic", stringText("iiPaintBrushPreset"));
    writeLine(output, "formatVersion", numberText(1));
    writeLine(output, "brushId", uuidText(preset.brushId));
    writeLine(output, "name", stringText(preset.name));
    writeLine(output, "tip.width", numberText(preset.tip.width));
    writeLine(output, "tip.height", numberText(preset.tip.height));
    writeLine(output, "tip.mask", byteVectorText(preset.tip.mask));
    writeLine(output, "size", numberText(preset.size));
    writeLine(output, "opacity", numberText(preset.opacity));
    writeLine(output, "hardness", numberText(preset.hardness));
    writeLine(output, "flow", numberText(preset.flow));
    writeLine(output, "density", numberText(preset.density));
    writeDynamics(output, preset.dynamics);
    writeMaterial(output, preset.material);
    return output.str();
}

BrushPreset deserializeBrushPreset(const std::string &payload)
{
    const std::map<std::string, std::string> values = parsePayload(payload);
    BrushPreset preset;
    preset.brushId = readUuid(values, "brushId");
    preset.name = readString(values, "name");
    preset.tip.width = readNumber<int>(values, "tip.width");
    preset.tip.height = readNumber<int>(values, "tip.height");
    preset.tip.mask = readStdByteVector(values, "tip.mask");
    preset.size = readNumber<float>(values, "size");
    preset.opacity = readNumber<float>(values, "opacity");
    preset.hardness = readNumber<float>(values, "hardness");
    preset.flow = readNumber<float>(values, "flow");
    preset.density = readNumber<float>(values, "density");
    preset.dynamics = readDynamics(values);
    preset.material = readMaterial(values);
    return preset;
}
