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

void writeMaterial(std::ostringstream &output, const BrushMaterial &material)
{
    writeLine(output, "material.texture.enabled", boolText(material.texture.enabled));
    writeLine(output, "material.texture.width", numberText(material.texture.width));
    writeLine(output, "material.texture.height", numberText(material.texture.height));
    writeLine(output, "material.texture.alpha", byteVectorText(material.texture.alpha));
    writeLine(output, "material.texture.grainStrength", numberText(material.texture.grainStrength));
    writeLine(output, "material.texture.scale", numberText(material.texture.scale));
    writeLine(output, "material.dualBrush.enabled", boolText(material.dualBrush.enabled));
    writeLine(output, "material.dualBrush.scale", numberText(material.dualBrush.scale));
    writeLine(output, "material.dualBrush.spacingRatio", numberText(material.dualBrush.spacingRatio));
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
    writeLine(output, "material.bristle.enabled", boolText(material.bristle.enabled));
    writeLine(output, "material.bristle.shape", numberText(static_cast<int>(material.bristle.shape)));
    writeLine(output, "material.bristle.count", numberText(material.bristle.count));
    writeLine(output, "material.bristle.length", numberText(material.bristle.length));
    writeLine(output, "material.bristle.stiffness", numberText(material.bristle.stiffness));
}

BrushMaterial readMaterial(const std::map<std::string, std::string> &values)
{
    BrushMaterial material;
    material.texture.enabled = readBool(values, "material.texture.enabled");
    material.texture.width = readNumber<Types::Pixel>(values, "material.texture.width");
    material.texture.height = readNumber<Types::Pixel>(values, "material.texture.height");
    material.texture.alpha = readByteVector(values, "material.texture.alpha");
    material.texture.grainStrength = readNumber<Types::Scalar>(values, "material.texture.grainStrength");
    material.texture.scale = readNumber<Types::Scalar>(values, "material.texture.scale", 1.0);
    material.dualBrush.enabled = readBool(values, "material.dualBrush.enabled");
    material.dualBrush.scale = readNumber<Types::Scalar>(values, "material.dualBrush.scale", 1.0);
    material.dualBrush.spacingRatio = readNumber<Types::Scalar>(values, "material.dualBrush.spacingRatio", 1.0);
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
    preset.material = readMaterial(values);
    return preset;
}
