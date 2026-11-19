#include <iiPaintEngine>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

// Version 3 bytes captured with the stream-based serializer.
constexpr std::string_view goldenPayload =
        "formatMagic\t\"iiPaintDocument\"\n"
        "formatVersion\t3\n"
        "document.metadata.title\t\"Ink \\\"proof\\\" \\\\ draft\tA\"\n"
        "document.metadata.author\t\"\"\n"
        "document.metadata.storagePath\t\"ink\\\\draft.ipe\"\n"
        "document.metadata.createdAt\t\"\"\n"
        "document.metadata.modifiedAt\t\"\"\n"
        "document.metadata.documentId\t00000000000000000000000000000000\n"
        "document.metadata.version\t1\n"
        "document.metadata.thumbnail\t\n"
        "document.metadata.backgroundColor\t0\n"
        "document.metadata.unit\t0\n"
        "document.metadata.intendedExportWidth\t0\n"
        "document.metadata.intendedExportHeight\t0\n"
        "document.metadata.dpiX\t144.5\n"
        "document.metadata.dpiY\t-0\n"
        "document.metadata.colorSpace\t\"sRGB\"\n"
        "document.metadata.appVersion\t\"\"\n"
        "document.surface.width\t2\n"
        "document.surface.height\t1\n"
        "document.surface.pixelFormat\t0\n"
        "document.surface.colorSpace\t0\n"
        "document.surface.dpiX\t72\n"
        "document.surface.dpiY\t72\n"
        "document.surface.backingStore\t0\n"
        "document.surface.pixels\t802468AC802468AC\n"
        "document.surface.dirtyRegion.count\t1\n"
        "document.surface.dirtyRegion.0\t-3 4 2 1\n"
        "document.surface.dirtyBounds\t-3 4 2 1\n"
        "document.surface.textureHandle\t18446744073709551615\n"
        "document.layers.count\t1\n"
        "document.layers.0.surface.width\t0\n"
        "document.layers.0.surface.height\t0\n"
        "document.layers.0.surface.pixelFormat\t0\n"
        "document.layers.0.surface.colorSpace\t0\n"
        "document.layers.0.surface.dpiX\t72\n"
        "document.layers.0.surface.dpiY\t72\n"
        "document.layers.0.surface.backingStore\t0\n"
        "document.layers.0.surface.pixels\t\n"
        "document.layers.0.surface.dirtyRegion.count\t0\n"
        "document.layers.0.surface.dirtyBounds\t0 0 0 0\n"
        "document.layers.0.surface.textureHandle\t0\n"
        "document.layers.0.metadata.id\t00000000000000000000000000000000\n"
        "document.layers.0.metadata.name\t\"Ink\"\n"
        "document.layers.0.metadata.visible\t1\n"
        "document.layers.0.metadata.opacity\t0.875\n"
        "document.layers.0.metadata.blendMode\t0\n"
        "document.layers.0.metadata.kind\t0\n"
        "document.layers.0.metadata.clipsToBelow\t0\n"
        "document.layers.0.metadata.alphaLock\t0\n"
        "document.layers.0.mask.enabled\t0\n"
        "document.layers.0.mask.width\t0\n"
        "document.layers.0.mask.height\t0\n"
        "document.layers.0.mask.alpha\t\n"
        "document.layers.0.children.count\t0\n"
        "document.layers.activeLayerIndex\t0\n"
        "brushSources.count\t0\n"
        "colorSpaces.count\t1\n"
        "colorSpaces.0.name\t\"sRGB\"\n"
        "colorSpaces.0.primaries\t0\n"
        "colorSpaces.0.transferFunction\t0\n"
        "colorSpaces.0.componentEncoding\t0\n"
        "colorSpaces.0.iccProfile\t\n"
        "colorSpaces.0.linear\t0\n"
        "colorSpaces.0.hdr\t0\n"
        "colorSpaces.0.minComponentValue\t0\n"
        "colorSpaces.0.maxComponentValue\t1\n"
        "colorSpaces.0.referenceWhiteNits\t80\n"
        "colorSpaces.0.redPrimary\t0.625 0.25\n"
        "colorSpaces.0.greenPrimary\t0.125 0.75\n"
        "colorSpaces.0.bluePrimary\t0.25 0.125\n"
        "colorSpaces.0.whitePoint\t0.3125 0.328125\n"
        "assets.count\t1\n"
        "assets.0.id\t00000000000000000000000000000000\n"
        "assets.0.name\t\"Stamp\"\n"
        "assets.0.mimeType\t\"image/png\"\n"
        "assets.0.bytes\t00ABFF\n"
        "history.cursor\t1\n"
        "history.nextSequence\t2\n"
        "history.maxUndoCommands\t0\n"
        "history.undo.count\t1\n"
        "history.undo.0.sequence\t1\n"
        "history.undo.0.label\t\"Stroke\"\n"
        "history.undo.0.targetId\t00000000000000000000000000000000\n"
        "history.undo.0.kind\t0\n"
        "history.undo.0.scope\t0\n"
        "history.undo.0.transactionId\t00000000000000000000000000000000\n"
        "history.undo.0.timestamp\t9.5367431640625e-07\n"
        "history.undo.0.coalescingKey\t\"\"\n"
        "history.undo.0.reversible\t1\n"
        "history.undo.0.committed\t1\n"
        "history.undo.0.dirtyBounds\t-0 -2.5 1234.5 4.25\n"
        "history.undo.0.patches.count\t0\n"
        "history.redo.count\t0\n";

// Use exactly representable values so the archived bytes are stable across long double implementations.
DocumentArchive goldenArchive()
{
    DocumentArchive archive;
    archive.document.metadata.title = "Ink \"proof\" \\ draft\tA";
    archive.document.metadata.storagePath = "ink\\draft.ipe";
    archive.document.metadata.dpiX = 144.5;
    archive.document.metadata.dpiY = -0.0;
    archive.document.surface = makeDrawingSurface(2, 1, 0x802468ACU);
    archive.document.surface.dirtyRegion = {{{-3, 4}, 2, 1}};
    archive.document.surface.dirtyBounds = archive.document.surface.dirtyRegion.front();
    archive.document.surface.textureHandle = std::numeric_limits<std::uint64_t>::max();
    Layer layer;
    layer.metadata.name = "Ink";
    layer.metadata.opacity = 0.875;
    archive.document.layers.layers.push_back(layer);

    ColorSpace color;
    color.redPrimary = {0.625, 0.25};
    color.greenPrimary = {0.125, 0.75};
    color.bluePrimary = {0.25, 0.125};
    color.whitePoint = {0.3125, 0.328125};
    archive.colorSpaces.push_back(color);
    archive.assets.push_back({{}, "Stamp", "image/png", {std::byte{0x00}, std::byte{0xAB}, std::byte{0xFF}}});

    Command edit;
    edit.sequence = 1;
    edit.label = "Stroke";
    edit.timestamp = 0x1p-20;
    edit.dirtyBounds = {{-0.0, -2.5}, 1234.5, 4.25};
    archive.history.undoCommands.push_back(edit);
    archive.history.cursor = 1;
    archive.history.nextSequence = 2;
    return archive;
}

// Exercise a legacy process locale without depending on locales installed on the test machine.
class LegacyPunctuation final : public std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
    char do_thousands_sep() const override { return '.'; }
    std::string do_grouping() const override { return "\3"; }
};

// The former writer used the process locale for floating-point and geometry fields.
bool preservesLocalizedNumbers(const DocumentArchive &archive)
{
    const auto previous = std::locale::global(std::locale(std::locale::classic(), new LegacyPunctuation));
    const auto localized = serializeDocumentArchive(archive);
    const auto reopened = deserializeDocumentArchive(localized);
    const auto roundTrip = serializeDocumentArchive(reopened);
    std::locale::global(previous);
    return reopened.compatible && reopened.history.undoCommands.size() == 1
            && localized.contains("document.metadata.dpiX\t144,5\n")
            && localized.contains("history.undo.0.dirtyBounds\t-0 -2,5 1.234,5 4,25\n")
            && reopened.history.undoCommands.front().dirtyBounds.width == 1234.5
            && localized == roundTrip;
}

// Older input can use unquoted words, out-of-order and repeated records, and relaxed numeric fields.
bool preservesLegacyFieldReads()
{
    const auto archive = deserializeDocumentArchive(
            "document.metadata.title\t\"earlier\"\r\n"
            "document.metadata.dpiY\t1e\r\n"
            "document.metadata.dpiX\t +96.5 legacy\r\n"
            "document.metadata.author\t Painter extra\r\n"
            "document.surface.dirtyBounds\t-1 +2 3 invalid\r\n"
            "document.metadata.intendedExportWidth\t2147483648\r\n"
            "formatVersion\t3\r\n"
            "document.metadata.title\t\"Latest \\\"ink\\\"\\!\"");
    const auto &metadata = archive.document.metadata;
    const auto &surface = archive.document.surface;
    return archive.compatible && metadata.title == "Latest \"ink\"!" && metadata.author == "Painter"
            && metadata.dpiX == 96.5 && metadata.dpiY == 72.0 && metadata.intendedExportWidth == 0
            && surface.dirtyBounds.origin.x == -1 && surface.dirtyBounds.origin.y == 2
            && surface.dirtyBounds.width == 3 && surface.dirtyBounds.height == 0;
}

// Compare boundary values with the former stream conversion at the platform's archive precision.
bool preservesNumberConversion()
{
    constexpr auto key = "document.metadata.dpiX\t";
    constexpr std::array values{
        -0.0, 0.1, 1.0000000000000002, 0.0001, 1.0e-5, 1.0e30,
        std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::min(),
        std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(),
    };
    DocumentArchive archive;
    for (const double value : values) {
        std::ostringstream legacyWriter;
        legacyWriter << std::setprecision(std::numeric_limits<long double>::digits10 + 1) << value;
        const auto record = key + legacyWriter.str() + '\n';
        archive.document.metadata.dpiX = value;
        if (!serializeDocumentArchive(archive).contains(record)) {
            std::cerr << "Archive numeric spelling differs from the old writer for " << record;
            return false;
        }

        double expected{};
        std::istringstream legacyReader(legacyWriter.str());
        if (!(legacyReader >> expected)) expected = 72.0;
        const auto reopened = deserializeDocumentArchive("formatVersion\t3\n" + record);
        const auto actual = reopened.document.metadata.dpiX;
        if (!reopened.compatible || actual != expected || std::signbit(actual) != std::signbit(expected)) {
            std::cerr << "Archive numeric read differs from the old reader for " << record;
            return false;
        }
    }
    return true;
}

// Create 4,000 populated layers in moderately nested groups using small surfaces.
// There are about 100,000 real schema fields, with floats, rectangles and a text name on every layer.
DocumentArchive benchmarkArchive()
{
    DocumentArchive archive;
    Layer layer;
    layer.metadata.name = "Fine ink strokes";
    layer.metadata.opacity = 0.8125;
    layer.surface = makeDrawingSurface(3, 2, 0x802468ACU);
    layer.surface.dpiX = 96.25;
    layer.surface.dirtyRegion = {{{-1, 0}, 2, 1}};
    layer.surface.dirtyBounds = layer.surface.dirtyRegion.front();
    for (std::size_t groupIndex = 0; groupIndex < 100; ++groupIndex) {
        Layer group = layer;
        group.metadata.kind = LayerKind::Group;
        group.children.assign(39, layer);
        archive.document.layers.layers.push_back(std::move(group));
    }
    return archive;
}

// Collect medians over the same generated payload, excluding fixture construction and validation.
bool measureDocumentArchive()
{
    const auto archive = benchmarkArchive();
    const auto payload = serializeDocumentArchive(archive);
    // 이 벤치의 데이터에는 LF와 @가 없다. 청크 구분 LF와 서명 줄을 빼면 필드 수가 된다.
    const auto fieldCount = std::ranges::count(payload, '\n') - std::ranges::count(payload, '@') - 1;
    if (fieldCount < 100'000 || fieldCount > 100'200 || !deserializeDocumentArchive(payload).compatible) {
        std::cerr << "Unexpected parse fixture with " << fieldCount << " fields\n";
        return false;
    }

    using Clock = std::chrono::steady_clock;
    using Milliseconds = std::chrono::duration<double, std::milli>;
    std::array<double, 9> parseTimes{};
    std::array<double, 9> writeTimes{};
    for (std::size_t index = 0; index < parseTimes.size(); ++index) {
        const auto parseStart = Clock::now();
        const auto reopened = deserializeDocumentArchive(payload);
        parseTimes[index] = Milliseconds(Clock::now() - parseStart).count();
        if (!reopened.compatible || reopened.document.layers.layers.size() != 100
                || reopened.document.layers.layers.back().children.size() != 39
                || reopened.document.layers.layers.back().children.back().metadata.opacity != 0.8125) {
            return false;
        }

        const auto writeStart = Clock::now();
        const auto written = serializeDocumentArchive(reopened);
        writeTimes[index] = Milliseconds(Clock::now() - writeStart).count();
        if (written != payload) return false;
    }
    std::ranges::sort(parseTimes);
    std::ranges::sort(writeTimes);
    const auto medianParse = parseTimes[parseTimes.size() / 2];
    std::cout << fieldCount << " fields, " << payload.size() << " bytes, " << parseTimes.size()
              << " samples: parse median " << medianParse << " ms, write median "
              << writeTimes[writeTimes.size() / 2] << " ms\n";

    // Let CI pin a measured Release ceiling for its own hardware and standard library.
    if (const char *setting = std::getenv("IIPAINTENGINE_ARCHIVE_PARSE_MAX_MS")) {
        const std::string_view text(setting);
        unsigned int limit = 0;
        const auto end = text.data() + text.size();
        const auto parsed = std::from_chars(text.data(), end, limit);
        if (parsed.ec != std::errc{} || parsed.ptr != end || limit == 0 || medianParse > limit) {
            std::cerr << "Archive parse median exceeded its ceiling or the ceiling is invalid: " << text << '\n';
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    const auto archive = goldenArchive();
    const auto reopened = deserializeDocumentArchiveView(goldenPayload);
    const auto currentPayload = serializeDocumentArchive(archive);
    if (!reopened.compatible || !std::signbit(reopened.document.metadata.dpiY)
            || serializeDocumentArchive(reopened) != currentPayload
            || serializeDocumentArchive(deserializeDocumentArchiveView(currentPayload)) != currentPayload) {
        std::cerr << "The version 3 golden archive did not migrate losslessly to version 4\n";
        return 1;
    }
    if (!preservesLocalizedNumbers(archive) || !preservesLegacyFieldReads() || !preservesNumberConversion()) {
        std::cerr << "Legacy archive field behavior changed\n";
        return 2;
    }
    return measureDocumentArchive() ? 0 : 3;
}
