//
// Created by Justmoong on 2026 May 26.
//

#include "BrushPresetSerializer.h"

#include <cstddef>
#include <cmath>
#include <locale>
#include <stdexcept>
#include <type_traits>
#include "Brush/BrushResolve.h"
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
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<long double>::digits10 + 1) << value;
    return output.str();
}

std::string boolText(bool value)
{
    return value ? "1" : "0";
}

std::string stringText(const std::string &value)
{
    std::string result = "\"";
    for (char c : value) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result + '"';
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
    throw std::invalid_argument("invalid hexadecimal byte");
}

std::uint8_t parseHexByte(const std::string &text, std::size_t offset)
{
    return static_cast<std::uint8_t>((hexValue(text[offset]) << 4) | hexValue(text[offset + 1]));
}

std::map<std::string, std::string> parsePayload(const std::string &payload)
{
    if (payload.size() > 40 * 1024 * 1024) throw std::invalid_argument("brush payload exceeds 40 MiB");
    std::map<std::string, std::string> values;
    std::istringstream input(payload);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const std::size_t separator = line.find('\t');
        if (separator == std::string::npos || separator == 0) throw std::invalid_argument("invalid brush field");
        if (values.size() >= 65536) throw std::invalid_argument("too many brush fields");
        if (!values.emplace(line.substr(0, separator), line.substr(separator + 1)).second)
            throw std::invalid_argument("duplicate brush field");
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
    input.imbue(std::locale::classic());
    if constexpr (std::is_unsigned_v<T>) {
        if (iterator->second.find('-') != std::string::npos) throw std::invalid_argument(key + ": negative unsigned value");
    }
    input >> result;
    if (!input) throw std::invalid_argument(key + ": invalid number");
    input >> std::ws;
    if (!input.eof()) throw std::invalid_argument(key + ": trailing data");
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(result)) throw std::invalid_argument(key + ": non-finite number");
    }
    return result;
}

bool readBool(const std::map<std::string, std::string> &values,
              const std::string &key,
              bool fallback = false)
{
    const auto value = readNumber<int>(values, key, fallback ? 1 : 0);
    if (value != 0 && value != 1) throw std::invalid_argument(key + ": expected boolean");
    return value != 0;
}

std::string readString(const std::map<std::string, std::string> &values,
                       const std::string &key,
                       const std::string &fallback = {})
{
    const auto iterator = values.find(key);
    if (iterator == values.end()) {
        return fallback;
    }

    const auto &text = iterator->second;
    if (text.size() < 2 || text.front() != '"' || text.back() != '"')
        throw std::invalid_argument(key + ": expected quoted string");
    const bool modern = readNumber<unsigned int>(values, "formatVersion", 1) >= 2;
    std::string result;
    for (std::size_t i = 1; i + 1 < text.size(); ++i) {
        char c = text[i];
        if (c == '\\') {
            if (++i + 1 >= text.size()) throw std::invalid_argument(key + ": incomplete escape");
            c = text[i];
            if (modern) {
                if (c == 'n') c = '\n';
                else if (c == 'r') c = '\r';
                else if (c == 't') c = '\t';
                else if (c != '\\' && c != '"') throw std::invalid_argument(key + ": invalid escape");
            }
        } else if (c == '"') throw std::invalid_argument(key + ": unescaped quote");
        result += c;
    }
    return result;
}

PaintUuid readUuid(const std::map<std::string, std::string> &values, const std::string &key)
{
    PaintUuid uuid{};
    const auto iterator = values.find(key);
    if (iterator == values.end()) return uuid;
    if (iterator->second.size() != uuid.bytes.size() * 2) throw std::invalid_argument(key + ": invalid uuid");

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
    if (text.size() % 2 || text.size() > 32 * 1024 * 1024) throw std::invalid_argument(key + ": invalid mask byte length");
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
    if (text.size() % 2 || text.size() > 32 * 1024 * 1024) throw std::invalid_argument(key + ": invalid mask byte length");
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
    writeLine(output, prefix + ".points.count", numberText(curve.points.size()));
    for (std::size_t i = 0; i < curve.points.size(); ++i) {
        writeLine(output, prefix + ".points." + numberText(i) + ".input", numberText(curve.points[i].input));
        writeLine(output, prefix + ".points." + numberText(i) + ".output", numberText(curve.points[i].output));
    }
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
    const auto count = readNumber<std::size_t>(values, prefix + ".points.count");
    if (count > 64) throw std::invalid_argument("curve exceeds 64 knots");
    for (std::size_t i = 0; i < count; ++i)
        curve.points.push_back({readNumber<double>(values, prefix + ".points." + numberText(i) + ".input"),
                                readNumber<double>(values, prefix + ".points." + numberText(i) + ".output")});
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

void writeAdvanced(std::ostringstream &output, const BrushPreset &p)
{
    writeLine(output, "shape.kind", numberText(static_cast<int>(p.shape.kind)));
    writeLine(output, "shape.angleMode", numberText(static_cast<int>(p.shape.angleMode)));
    writeLine(output, "shape.angleRadians", numberText(p.shape.angleRadians));
    writeLine(output, "shape.roundness", numberText(p.shape.roundness));
    writeLine(output, "shape.flipX", boolText(p.shape.flipX));
    writeLine(output, "shape.flipY", boolText(p.shape.flipY));
    writeLine(output, "stroke.spacing", numberText(p.stroke.spacing));
    writeLine(output, "stroke.spacingRatio", numberText(p.stroke.spacingRatio));
    writeLine(output, "stroke.spacingEnabled", boolText(p.stroke.spacingEnabled));
    writeLine(output, "stroke.spacingFollowsSize", boolText(p.stroke.spacingFollowsSize));
    writeLine(output, "stroke.flowEnabled", boolText(p.stroke.flowEnabled));
    writeLine(output, "stroke.opacityEnabled", boolText(p.stroke.opacityEnabled));
    writeLine(output, "stroke.hardnessEnabled", boolText(p.stroke.hardnessEnabled));
    writeLine(output, "stroke.warmupDistance", numberText(p.stroke.warmupDistance));
    writeLine(output, "stroke.taperMinimum", numberText(p.stroke.taperMinimum));
    writeLine(output, "stroke.warmupTaperShape", numberText(static_cast<int>(p.stroke.warmupTaperShape)));
    writeLine(output, "stroke.airbrushEnabled", boolText(p.stroke.airbrushEnabled));
    writeLine(output, "stroke.airbrushRate", numberText(p.stroke.airbrushRate));
    writeLine(output, "stroke.blendMode", numberText(static_cast<int>(p.stroke.blendMode)));
    writeLine(output, "color.enabled", boolText(p.color.enabled));
    writeLine(output, "color.secondaryArgb", numberText(p.color.secondaryArgb));
    writeLine(output, "color.mix", numberText(p.color.mix));
    writeLine(output, "color.hueJitter", numberText(p.color.hueJitter));
    writeLine(output, "color.saturationJitter", numberText(p.color.saturationJitter));
    writeLine(output, "color.valueJitter", numberText(p.color.valueJitter));
    writeLine(output, "color.perStroke", boolText(p.color.perStroke));
    writeLine(output, "tipSequence.selection", numberText(static_cast<int>(p.tipSequence.selection)));
    writeLine(output, "material.scatter.axes", numberText(static_cast<int>(p.material.scatter.axes)));
    writeLine(output, "material.scatter.distribution", numberText(static_cast<int>(p.material.scatter.distribution)));
    writeLine(output, "material.scatter.countJitter", numberText(p.material.scatter.countJitter));
    writeLine(output, "material.scatter.relativeToSize", boolText(p.material.scatter.relativeToSize));
    writeLine(output, "tipSequence.count", numberText(p.tipSequence.tips.size()));
    for (std::size_t i = 0; i < p.tipSequence.tips.size(); ++i) {
        const auto prefix = "tipSequence." + numberText(i);
        const auto &tip = p.tipSequence.tips[i];
        writeLine(output, prefix + ".width", numberText(tip.width));
        writeLine(output, prefix + ".height", numberText(tip.height));
        writeLine(output, prefix + ".mask", byteVectorText(tip.mask));
    }
    writeLine(output, "dynamics.bindings.count", numberText(p.dynamics.bindings.size()));
    for (std::size_t i = 0; i < p.dynamics.bindings.size(); ++i) {
        const auto prefix = "dynamics.bindings." + numberText(i);
        const auto &b = p.dynamics.bindings[i];
        writeLine(output, prefix + ".enabled", boolText(b.enabled));
        writeLine(output, prefix + ".source", numberText(static_cast<int>(b.source)));
        writeLine(output, prefix + ".target", numberText(static_cast<int>(b.target)));
        writeLine(output, prefix + ".combineMode", numberText(static_cast<int>(b.combineMode)));
        writeLine(output, prefix + ".inputMinimum", numberText(b.inputMinimum));
        writeLine(output, prefix + ".inputMaximum", numberText(b.inputMaximum));
        writeLine(output, prefix + ".customInput", numberText(b.customInput));
        writeDynamicsResponseCurve(output, prefix + ".curve", b.curve);
    }
}

void readAdvanced(const std::map<std::string, std::string> &values, BrushPreset &p)
{
    p.shape.kind = static_cast<BrushTipShape>(readNumber<int>(values, "shape.kind", static_cast<int>(p.shape.kind)));
    p.shape.angleMode = static_cast<BrushAngleMode>(readNumber<int>(values, "shape.angleMode", static_cast<int>(p.shape.angleMode)));
    p.shape.angleRadians = readNumber<double>(values, "shape.angleRadians", p.shape.angleRadians);
    p.shape.roundness = readNumber<double>(values, "shape.roundness", p.shape.roundness);
    p.shape.flipX = readBool(values, "shape.flipX", p.shape.flipX);
    p.shape.flipY = readBool(values, "shape.flipY", p.shape.flipY);
    p.stroke.spacing = readNumber<double>(values, "stroke.spacing", p.stroke.spacing);
    p.stroke.spacingRatio = readNumber<double>(values, "stroke.spacingRatio", p.stroke.spacingRatio);
    p.stroke.spacingEnabled = readBool(values, "stroke.spacingEnabled", p.stroke.spacingEnabled);
    p.stroke.spacingFollowsSize = readBool(values, "stroke.spacingFollowsSize", p.stroke.spacingFollowsSize);
    p.stroke.flowEnabled = readBool(values, "stroke.flowEnabled", p.stroke.flowEnabled);
    p.stroke.opacityEnabled = readBool(values, "stroke.opacityEnabled", p.stroke.opacityEnabled);
    p.stroke.hardnessEnabled = readBool(values, "stroke.hardnessEnabled", p.stroke.hardnessEnabled);
    p.stroke.warmupDistance = readNumber<double>(values, "stroke.warmupDistance", p.stroke.warmupDistance);
    p.stroke.taperMinimum = readNumber<double>(values, "stroke.taperMinimum", p.stroke.taperMinimum);
    p.stroke.warmupTaperShape = static_cast<StrokeTaperShape>(readNumber<int>(values, "stroke.warmupTaperShape", static_cast<int>(p.stroke.warmupTaperShape)));
    p.stroke.airbrushEnabled = readBool(values, "stroke.airbrushEnabled", p.stroke.airbrushEnabled);
    p.stroke.airbrushRate = readNumber<double>(values, "stroke.airbrushRate", p.stroke.airbrushRate);
    p.stroke.blendMode = static_cast<RasterBlendMode>(readNumber<int>(values, "stroke.blendMode", static_cast<int>(p.stroke.blendMode)));
    p.color.enabled = readBool(values, "color.enabled", p.color.enabled);
    p.color.secondaryArgb = readNumber<std::uint32_t>(values, "color.secondaryArgb", p.color.secondaryArgb);
    p.color.mix = readNumber<double>(values, "color.mix", p.color.mix);
    p.color.hueJitter = readNumber<double>(values, "color.hueJitter", p.color.hueJitter);
    p.color.saturationJitter = readNumber<double>(values, "color.saturationJitter", p.color.saturationJitter);
    p.color.valueJitter = readNumber<double>(values, "color.valueJitter", p.color.valueJitter);
    p.color.perStroke = readBool(values, "color.perStroke", p.color.perStroke);
    p.tipSequence.selection = static_cast<BrushTipSelection>(readNumber<int>(values, "tipSequence.selection", static_cast<int>(p.tipSequence.selection)));
    p.material.scatter.axes = static_cast<BrushScatterAxes>(readNumber<int>(values, "material.scatter.axes", static_cast<int>(p.material.scatter.axes)));
    p.material.scatter.distribution = static_cast<BrushScatterDistribution>(readNumber<int>(values, "material.scatter.distribution", static_cast<int>(p.material.scatter.distribution)));
    p.material.scatter.countJitter = readNumber<double>(values, "material.scatter.countJitter", p.material.scatter.countJitter);
    p.material.scatter.relativeToSize = readBool(values, "material.scatter.relativeToSize", p.material.scatter.relativeToSize);
    auto count = readNumber<std::size_t>(values, "tipSequence.count");
    if (count > 256) throw std::invalid_argument("tip bank exceeds 256 masks");
    for (std::size_t i = 0; i < count; ++i) {
        const auto prefix = "tipSequence." + numberText(i);
        p.tipSequence.tips.push_back({readNumber<int>(values, prefix + ".width"),
                readNumber<int>(values, prefix + ".height"), readStdByteVector(values, prefix + ".mask")});
    }
    count = readNumber<std::size_t>(values, "dynamics.bindings.count");
    if (count > 64) throw std::invalid_argument("dynamics exceeds 64 bindings");
    for (std::size_t i = 0; i < count; ++i) {
        const auto prefix = "dynamics.bindings." + numberText(i);
        BrushDynamicsBinding b;
        b.enabled = readBool(values, prefix + ".enabled", true);
        b.source = static_cast<BrushDynamicsSource>(readNumber<int>(values, prefix + ".source"));
        b.target = static_cast<BrushDynamicsTarget>(readNumber<int>(values, prefix + ".target"));
        b.combineMode = static_cast<BrushDynamicsCombineMode>(readNumber<int>(values, prefix + ".combineMode"));
        b.inputMinimum = readNumber<double>(values, prefix + ".inputMinimum");
        b.inputMaximum = readNumber<double>(values, prefix + ".inputMaximum", 1);
        b.customInput = readNumber<unsigned int>(values, prefix + ".customInput");
        b.curve = readDynamicsResponseCurve(values, prefix + ".curve", b.curve);
        p.dynamics.bindings.push_back(std::move(b));
    }
}

} // namespace

std::string serializeBrushPreset(const BrushPreset &preset)
{
    const auto errors = validateBrushPreset(preset);
    if (!errors.empty()) throw std::invalid_argument(errors.front());
    std::ostringstream output;
    writeLine(output, "formatMagic", stringText("iiPaintBrushPreset"));
    writeLine(output, "formatVersion", numberText(2));
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
    writeAdvanced(output, preset);
    return output.str();
}

BrushPreset deserializeBrushPreset(const std::string &payload)
{
    const std::map<std::string, std::string> values = parsePayload(payload);
    const auto version = readNumber<unsigned int>(values, "formatVersion");
    if (readString(values, "formatMagic") != "iiPaintBrushPreset" || version < 1 || version > 2)
        throw std::invalid_argument("unsupported brush preset format or version");
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
    readAdvanced(values, preset);
    const auto errors = validateBrushPreset(preset);
    if (!errors.empty()) throw std::invalid_argument(errors.front());
    return preset;
}

BrushPresetReadResult readBrushPreset(const std::string &payload)
{
    BrushPresetReadResult result;
    try { result.preset = deserializeBrushPreset(payload); }
    catch (const std::invalid_argument &error) { result.errors.push_back(error.what()); }
    return result;
}
