#include <iiPaintEngine>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <ostream>
#include <span>
#include <streambuf>
#include <string_view>
#include <utility>

namespace {

class AllocationProbe;
thread_local AllocationProbe *activeProbe = nullptr;

// 크기가 서로 다른 이력 페이로드별로 최초 버퍼 주소와 할당 횟수를 기록한다.
struct PayloadAllocationCount {
    std::size_t bytes = 0;
    const void *firstBuffer = nullptr;
    std::size_t allocations = 0;
};

// 호출 전에 존재하던 버퍼를 제외하고 해당 호출이 만든 C++ 할당의 최대 동시 바이트를 센다.
class AllocationProbe final {
public:
    explicit AllocationProbe(std::span<PayloadAllocationCount> payloads = {}) : m_payloads(payloads)
    {
        activeProbe = this;
    }
    ~AllocationProbe() { activeProbe = nullptr; }
    AllocationProbe(const AllocationProbe &) = delete;
    AllocationProbe &operator=(const AllocationProbe &) = delete;

    // 추적 자료 자체는 스택에 두어 측정 대상에 영향을 주지 않는다.
    void allocated(void *pointer, std::size_t bytes) noexcept
    {
        for (auto &payload : m_payloads) {
            if (payload.bytes == bytes) {
                if (payload.allocations++ == 0) payload.firstBuffer = pointer;
                break;
            }
        }
        if (m_count == m_live.size()) {
            exhausted = true;
            return;
        }
        m_live[m_count++] = {pointer, bytes};
        live += bytes;
        peak = std::max(peak, live);
    }

    // 측정 전에 확보한 포인터의 해제는 이 호출의 동시 할당량을 줄이지 않는다.
    void freed(void *pointer) noexcept
    {
        for (std::size_t index = 0; index < m_count; ++index) {
            if (m_live[index].pointer == pointer) {
                live -= m_live[index].bytes;
                m_live[index] = m_live[--m_count];
                return;
            }
        }
    }

    std::size_t peak = 0;
    std::size_t live = 0;
    bool exhausted = false;

private:
    struct Entry { void *pointer; std::size_t bytes; };
    std::span<PayloadAllocationCount> m_payloads;
    std::array<Entry, 1024> m_live{};
    std::size_t m_count = 0;
};

} // namespace

// 엔진과 같은 스레드에서 실행한 일반 C++ 힙 할당을 추적한다.
void *operator new(std::size_t size)
{
    if (void *memory = std::malloc(std::max<std::size_t>(size, 1))) {
        if (activeProbe) activeProbe->allocated(memory, size);
        return memory;
    }
    throw std::bad_alloc();
}

void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *memory) noexcept
{
    if (activeProbe) activeProbe->freed(memory);
    std::free(memory);
}
void operator delete[](void *memory) noexcept { ::operator delete(memory); }
void operator delete(void *memory, std::size_t) noexcept { ::operator delete(memory); }
void operator delete[](void *memory, std::size_t) noexcept { ::operator delete(memory); }

namespace {

// Undo와 Redo의 패치를 실제로 읽어, 처음 디코딩한 바이트 벡터가 이력에 그대로 남는지 검사한다.
bool checkHistoryPayloadOwnership()
{
    constexpr std::size_t patchesPerCommand = 2;
    constexpr std::size_t basePayloadBytes = 256 * 1024;
    std::array<PayloadAllocationCount, 2 * patchesPerCommand * 2> counts{};
    DocumentArchive source;
    source.history.cursor = 1;
    std::size_t payloadIndex = 0;
    for (auto *commands : {&source.history.undoCommands, &source.history.redoCommands}) {
        Command &command = commands->emplace_back();
        command.sequence = source.history.nextSequence++;
        command.kind = CommandKind::RasterEdit;
        for (std::size_t index = 0; index < patchesPerCommand; ++index) {
            CommandPatch &patch = command.patches.emplace_back();
            for (auto *state : {&patch.beforeState, &patch.afterState}) {
                auto &expected = counts[payloadIndex];
                ++payloadIndex;
                expected.bytes = basePayloadBytes + payloadIndex;
                state->storage = CommandPayloadStorage::InlineBytes;
                state->bytes.assign(expected.bytes, static_cast<std::byte>(payloadIndex));
            }
        }
    }

    const auto payload = serializeDocumentArchive(source);
    const auto reopened = [&] {
        AllocationProbe probe(counts);
        auto result = deserializeDocumentArchiveView(payload);
        if (probe.exhausted) std::abort();
        return result;
    }();
    if (!reopened.compatible || serializeDocumentArchive(reopened) != payload) {
        std::cerr << "Undo and Redo payloads did not round-trip\n";
        return false;
    }
#if defined(_WIN32)
    // DLL 할당이 실행 파일의 추적기에 보이지 않으면 위의 바이트 일치 검사만 적용한다.
    if (std::ranges::all_of(counts, [](const auto &entry) { return entry.allocations == 0; })) {
        std::cout << "The DLL history payload allocations are not observable in this executable\n";
        return true;
    }
#endif
    payloadIndex = 0;
    for (const auto *commands : {&reopened.history.undoCommands, &reopened.history.redoCommands}) {
        for (const auto &command : *commands) {
            for (const auto &patch : command.patches) {
                for (const auto *state : {&patch.beforeState, &patch.afterState}) {
                    if (payloadIndex == counts.size()) return false;
                    const auto &measured = counts[payloadIndex++];
                    if (measured.allocations != 1 || measured.firstBuffer != state->bytes.data()
                            || measured.bytes != state->bytes.size()) {
                        std::cerr << "History payload " << measured.bytes << " bytes requested "
                                  << measured.allocations << " allocations; expected one retained allocation\n";
                        return false;
                    }
                }
            }
        }
    }
    if (payloadIndex != counts.size()) return false;
    std::cout << counts.size() << " Undo and Redo payloads each kept their only decoded allocation\n";
    return true;
}

// 한 화면과 그 위의 고해상도 레이어를 넣고 NUL, LF, 탭이 포함된 픽셀을 생성한다.
DocumentArchive largeArchive()
{
    constexpr int width = 4096;
    constexpr int height = 2048;
    DocumentArchive archive;
    archive.document.metadata.thumbnail = {0xFF090A00U, 0x0A002468U};
    archive.document.surface = makeDrawingSurface(width, height, 0xFF090A00U);
    Layer layer;
    layer.surface = makeDrawingSurface(width, height, 0x0A091BFFU);
    for (std::size_t index = 0; index < layer.surface.pixels.size(); ++index) {
        layer.surface.pixels[index] ^= static_cast<std::uint32_t>(index) * 0x9E3779B9U;
    }
    layer.mask.width = width;
    layer.mask.height = height;
    layer.mask.enabled = true;
    layer.mask.alpha.assign(layer.surface.pixels.size(), 0x0A);
    archive.document.layers.layers.push_back(std::move(layer));
    return archive;
}

// 두 해상도와 픽셀, 썸네일, 마스크가 각 버퍼에서 비트 단위로 복원됐는지 확인한다.
bool matchesRaster(const DocumentArchive &before, const DocumentArchive &after)
{
    if (!after.compatible || after.document.layers.layers.size() != 1) return false;
    const auto &source = before.document.layers.layers.front();
    const auto &reopened = after.document.layers.layers.front();
    return after.document.surface.width == before.document.surface.width
            && after.document.surface.height == before.document.surface.height
            && after.document.surface.pixels == before.document.surface.pixels
            && reopened.surface.width == source.surface.width && reopened.surface.height == source.surface.height
            && reopened.surface.pixels == source.surface.pixels && reopened.mask.alpha == source.mask.alpha
            && after.document.metadata.thumbnail == before.document.metadata.thumbnail;
}

// 스트림에 전달되는 바이트를 기존 뷰와 비교한다. 조기 EOF를 주면 짧은 쓰기도 모의할 수 있다.
class ComparingArchiveSink final : public std::streambuf {
public:
    explicit ComparingArchiveSink(std::string_view expected) : m_remaining(expected) {}

    // 모든 바이트가 바른 순서로 도착했는지 확인한다.
    bool complete() const noexcept { return m_remaining.empty(); }

private:
    // 출력 사본 없이 원본 범위와 접두사를 비교하고 남은 바이트 수를 갱신한다.
    std::streamsize xsputn(const char *bytes, std::streamsize size) override
    {
        const auto count = std::min(static_cast<std::size_t>(size), m_remaining.size());
        if (count && std::memcmp(bytes, m_remaining.data(), count) != 0) return 0;
        m_remaining.remove_prefix(count);
        return static_cast<std::streamsize>(count);
    }

    std::string_view m_remaining;
};

} // namespace

int main()
{
    if (!checkHistoryPayloadOwnership()) return 7;
    const auto source = largeArchive();
    std::size_t writePeak = 0;
    std::size_t writeLive = 0;
    auto payload = [&] {
        AllocationProbe probe;
        auto result = serializeDocumentArchive(source);
        if (probe.exhausted) std::abort();
        writePeak = probe.peak;
        writeLive = probe.live;
        return result;
    }();

    std::size_t readPeak = 0;
    std::size_t readLive = 0;
    const auto reopened = [&] {
        AllocationProbe probe;
        auto result = deserializeDocumentArchiveView(payload);
        if (probe.exhausted) std::abort();
        readPeak = probe.peak;
        readLive = probe.live;
        return result;
    }();
    if (payload.empty() || !matchesRaster(source, reopened)) {
        std::cerr << "The large document did not round-trip\n";
        return 1;
    }

    const auto &layer = source.document.layers.layers.front();
    const auto rasterBytes = (source.document.surface.pixels.size() + layer.surface.pixels.size())
            * sizeof(std::uint32_t) + layer.mask.alpha.size();
    if (payload.size() < rasterBytes || payload.size() - rasterBytes >= 8192) {
        std::cerr << "The archive does not store one raw copy of raster data\n";
        return 2;
    }

    // 복원된 문서의 스트림 출력까지 최초 아카이브와 동일해야 한다.
    ComparingArchiveSink expected(payload);
    std::ostream stream(&expected);
    std::size_t streamPeak = 0;
    {
        AllocationProbe probe;
        if (!serializeDocumentArchiveTo(reopened, stream) || !expected.complete() || probe.exhausted) return 3;
        streamPeak = probe.peak;
    }
    ComparingArchiveSink truncated(std::string_view(payload).substr(0, 1024 * 1024));
    std::ostream shortWrite(&truncated);
    shortWrite.exceptions(std::ios::badbit);
    if (serializeDocumentArchiveTo(source, shortWrite) || !shortWrite.bad() || !truncated.complete()) return 4;

    std::cout << "Archive " << payload.size() << " bytes; write peak " << writePeak
              << " bytes; read peak " << readPeak << " bytes; stream peak " << streamPeak << " bytes\n";
#if defined(_WIN32)
    // DLL 내부에서 CRT 할당 함수를 직접 이용한 경우에는 픽셀 검증만 적용한다.
    if (writeLive < payload.size() || readLive < rasterBytes) {
        std::cout << "The DLL allocator is not observable in this executable\n";
        return 0;
    }
#endif
    if (writeLive < payload.size() || readLive < rasterBytes) {
        std::cerr << "The probe did not observe the retained archive and pixel allocations\n";
        return 5;
    }
    const auto limit = payload.size() + payload.size() / 2;
    if (writePeak > limit || readPeak > limit || streamPeak > 64 * 1024) {
        std::cerr << "The round-trip exceeded 1.5 times the archive bytes per phase\n";
        return 6;
    }
    return 0;
}
