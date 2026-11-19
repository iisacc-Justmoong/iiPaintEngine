//
// Created by Justmoong on 2026 May 24.
//

#include "DocumentSerializer.h"
#include "Brush/BrushPresetSerializer.h"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <memory>
#include <new>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr char kHexDigits[] = "0123456789ABCDEF";
constexpr int kArchivePrecision = std::numeric_limits<long double>::digits10 + 1;
constexpr std::string_view kBinaryArchiveHeader = "iiPaintDocumentBinary\n";
constexpr std::uint32_t kFirstFlatArchiveVersion = 3;

// Treat a field name as a borrowed record prefix and a borrowed field suffix.
struct ArchiveKey {
    std::string_view prefix;
    std::string_view suffix{};
};

// Build a path for a nested record or a diagnostic. Regular fields use both key parts directly.
std::string joinedKey(ArchiveKey key)
{
    std::string result;
    result.reserve(key.prefix.size() + key.suffix.size());
    result.append(key.prefix).append(key.suffix);
    return result;
}

// The bounded decimal conversion and one allocation cover the complete indexed path.
std::string indexedKey(ArchiveKey prefix, std::size_t index)
{
    char digits[std::numeric_limits<std::size_t>::digits10 + 1];
    const auto end = std::to_chars(digits, digits + sizeof(digits), index).ptr;
    std::string result;
    result.reserve(prefix.prefix.size() + prefix.suffix.size() + 1 + static_cast<std::size_t>(end - digits));
    result.append(prefix.prefix).append(prefix.suffix).push_back('.');
    result.append(digits, end);
    return result;
}

// Older standard libraries and deployment targets can omit the floating-point overload.
template <typename T>
char *formatArchiveFloat(char *first, char *last, T value)
{
    if constexpr (requires { std::to_chars(first, last, value, std::chars_format::general, kArchivePrecision); }) {
        const auto result = std::to_chars(first, last, value, std::chars_format::general, kArchivePrecision);
        if (result.ec == std::errc{}) return result.ptr;
    }
    return nullptr;
}

// 하나의 레코드 작성기로 길이 산정, 문자열 직렬화, 스트림 직렬화를 일치시킨다.
class ArchiveOutput final : private std::streambuf {
public:
    // 두 목적지가 모두 없으면 바이트 수만 센다. 두 패스에는 같은 로케일을 넘긴다.
    explicit ArchiveOutput(const std::locale &locale, std::string *payload = nullptr, std::ostream *stream = nullptr)
        : m_payload(payload), m_stream(stream), m_classic(locale == std::locale::classic())
    {
        m_legacy.imbue(locale);
        m_legacy.precision(kArchivePrecision);
        m_legacy.exceptions(std::ios::badbit | std::ios::failbit);
    }

    // 아카이브의 바이너리 프레이밍 서명을 기록한다.
    void signature() { append(kBinaryArchiveHeader); }

    // UUID와 논리값처럼 이미 인코딩된 텍스트를 한 줄에 기록한다.
    void line(ArchiveKey key, std::string_view value)
    {
        beginLine(key);
        append(value);
        append('\n');
    }

    // 인용 문자열 내부의 개행을 이스케이프하여 레코드 경계와 구분한다.
    void quoted(ArchiveKey key, std::string_view value)
    {
        beginLine(key);
        append('"');
        for (auto escaped = value.find_first_of("\\\"\r\n"); escaped != std::string_view::npos;
             escaped = value.find_first_of("\\\"\r\n")) {
            append(value.substr(0, escaped));
            append('\\');
            const char byte = value[escaped];
            append(byte == '\r' ? 'r' : byte == '\n' ? 'n' : byte);
            value.remove_prefix(escaped + 1);
        }
        append(value);
        append("\"\n");
    }

    // 단일 정수 필드는 항상 로케일에 독립적인 10진수로 저장한다.
    template <typename T, typename... Rest>
    void numbers(ArchiveKey key, T value, Rest... rest)
    {
        beginLine(key);
        const bool localized = !m_classic && (sizeof...(rest) != 0 || std::is_floating_point_v<T>);
        appendNumber(value, localized);
        ((append(' '), appendNumber(rest, localized)), ...);
        append('\n');
    }

    // 바이너리 블록은 헤더, 정확히 지정된 바이트 수, LF 순서로 구성한다.
    void binary(ArchiveKey key, std::string_view bytes)
    {
        beginBinary(key, bytes.size());
        append(bytes);
        append('\n');
    }

    // 바이트 배열은 기존 소유 버퍼를 빌려 그대로 전송한다.
    void binary(ArchiveKey key, std::span<const std::byte> bytes)
    {
        binary(key, std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
    }

    // ARGB32 워드는 플랫폼에 독립적인 리틀엔디언 순서로 기록한다.
    void pixels(ArchiveKey key, std::span<const std::uint32_t> values)
    {
        if constexpr (std::endian::native == std::endian::little) {
            binary(key, std::as_bytes(values));
        } else {
            beginBinary(key, values.size_bytes());
            if (!m_payload && !m_stream) {
                count(values.size_bytes());
            } else {
                std::array<char, 4096> buffer;
                std::size_t used = 0;
                for (const auto value : values) {
                    for (unsigned int shift = 0; shift < 32; shift += 8) {
                        buffer[used++] = static_cast<char>((value >> shift) & 0xFFU);
                    }
                    if (used == buffer.size()) {
                        append({buffer.data(), used});
                        used = 0;
                    }
                }
                append({buffer.data(), used});
            }
            append('\n');
        }
    }

    // 길이 산정 패스가 확보해야 할 최종 문자열 크기를 반환한다.
    std::size_t measuredSize() const noexcept { return m_size; }

private:
    // 두 키 조각을 임시 문자열 없이 이어 기록한다.
    void beginLine(ArchiveKey key)
    {
        append(key.prefix);
        append(key.suffix);
        append('\t');
    }

    // 블록 길이에는 데이터 뒤의 LF가 포함되지 않는다.
    void beginBinary(ArchiveKey key, std::size_t length)
    {
        append('@');
        numbers(key, length);
    }

    // 숫자 토큰은 스택에서 변환하고 기존 로케일 경로에는 공유 스트림을 쓴다.
    template <typename T>
    void appendNumber(T value, bool localized)
    {
        char text[64];
        if (!localized) {
            if constexpr (std::is_integral_v<T>) {
                const auto end = std::to_chars(text, text + sizeof(text), value).ptr;
                append({text, static_cast<std::size_t>(end - text)});
                return;
            } else if (std::isfinite(value)) {
                if (const auto end = formatArchiveFloat(text, text + sizeof(text), value)) {
                    append({text, static_cast<std::size_t>(end - text)});
                    return;
                }
            }
        }
        m_legacy << value;
    }

    // 데이터 없이 길이만 받는 패스에서도 크기 합산의 오버플로를 먼저 검사한다.
    void count(std::size_t bytes)
    {
        if (bytes > std::numeric_limits<std::size_t>::max() - m_size) {
            throw std::length_error("Document archive exceeds addressable size.");
        }
        m_size += bytes;
    }

    // 문자열과 스트림 모두 같은 바이트를 받아들이며 실패하면 현재 기록을 즉시 중단한다.
    void append(std::string_view bytes)
    {
        if (m_payload) {
            m_payload->append(bytes);
        } else if (m_stream) {
            constexpr auto maximumWrite = static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max());
            while (!bytes.empty()) {
                const auto size = std::min(bytes.size(), maximumWrite);
                if (!m_stream->write(bytes.data(), static_cast<std::streamsize>(size))) {
                    throw std::ios_base::failure("Document archive write failed.");
                }
                bytes.remove_prefix(size);
            }
        } else {
            count(bytes.size());
        }
    }

    // 단일 구분자도 다른 바이트와 같은 대상에 보낸다.
    void append(char byte) { append(std::string_view(&byte, 1)); }

    // 기존 숫자 포매터가 방출한 바이트를 현재 출력 경로에 연결한다.
    std::streamsize xsputn(const char *data, std::streamsize size) override
    {
        append({data, static_cast<std::size_t>(size)});
        return size;
    }

    // 숫자 포매터의 단일 문자 쓰기를 같은 경로에서 처리한다.
    int_type overflow(int_type value) override
    {
        if (!traits_type::eq_int_type(value, traits_type::eof())) append(traits_type::to_char_type(value));
        return traits_type::not_eof(value);
    }

    std::string *m_payload;
    std::ostream *m_stream;
    std::size_t m_size = 0;
    std::ostream m_legacy{this};
    const bool m_classic;
};

std::string_view boolText(bool value)
{
    return value ? "1" : "0";
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

// 두 대소문자 표기를 받아들이고, 디코딩할 수 없으면 필드 경로를 오류로 전달한다.
unsigned int hexValue(char value, ArchiveKey key)
{
    if (value >= '0' && value <= '9') {
        return static_cast<unsigned int>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<unsigned int>(value - 'a' + 10);
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<unsigned int>(value - 'A' + 10);
    }
    throw std::invalid_argument("Invalid hexadecimal digit for " + joinedKey(key) + ".");
}

// 호출부에서 길이를 검증한 두 자리의 16진수를 바이트 하나로 변환한다.
std::uint8_t parseHexByte(std::string_view text, std::size_t offset, ArchiveKey key)
{
    return static_cast<std::uint8_t>((hexValue(text[offset], key) << 4U) | hexValue(text[offset + 1], key));
}

// Give legacy formatted extraction a bounded, read-only view into one archive field.
class ArchiveInputBuffer final : public std::streambuf {
public:
    void reset(std::string_view value)
    {
        // setg takes mutable pointers, but formatted extraction only reads this get area.
        char *const start = const_cast<char *>(value.data());
        setg(start, start, start + value.size());
    }
};

// The field index only inserts or replaces values. Its nodes all expire at the end of the read.
class ArchiveIndexStorage final {
public:
    // Carve aligned nodes from stable blocks using the same allocator on every deployment target.
    void *claim(std::size_t bytes, std::size_t alignment)
    {
        if (m_available < bytes || std::align(alignment, bytes, m_next, m_available) == nullptr) {
            const auto capacity = std::max<std::size_t>(4096, bytes + alignment - 1);
            m_blocks.push_back(std::make_unique_for_overwrite<std::byte[]>(capacity));
            m_next = m_blocks.back().get();
            m_available = capacity;
            std::align(alignment, bytes, m_next, m_available);
        }
        void *const node = m_next;
        m_next = static_cast<std::byte *>(m_next) + bytes;
        m_available -= bytes;
        return node;
    }

private:
    std::vector<std::unique_ptr<std::byte[]>> m_blocks;
    void *m_next = nullptr;
    std::size_t m_available = 0;
};

// Rebound map node allocators share the read's blocks. Bucket arrays retain normal deallocation on rehash.
template <typename T>
struct ArchiveIndexAllocator {
    using value_type = T;
    ArchiveIndexStorage *storage;

    explicit ArchiveIndexAllocator(ArchiveIndexStorage &blocks) noexcept : storage(&blocks) {}

    template <typename U>
    ArchiveIndexAllocator(const ArchiveIndexAllocator<U> &other) noexcept : storage(other.storage) {}

    [[nodiscard]] T *allocate(std::size_t count)
    {
        if (count == 1) return static_cast<T *>(storage->claim(sizeof(T), alignof(T)));
        return std::allocator<T>{}.allocate(count);
    }

    void deallocate(T *data, std::size_t count) noexcept
    {
        if (count != 1) std::allocator<T>{}.deallocate(data, count);
    }

    template <typename U>
    bool operator==(const ArchiveIndexAllocator<U> &other) const noexcept { return storage == other.storage; }
};

// 입력 버퍼의 필드 위치와 텍스트·바이너리 구분을 동기 읽기 동안만 보관한다.
class ArchiveFields final {
    // 값의 뷰는 청크 헤더나 구분자 없이 인코딩된 본문만 가리킨다.
    struct Field {
        std::string_view bytes;
        bool binary = false;
    };
    using Key = std::string_view;
    using Allocator = ArchiveIndexAllocator<std::pair<const Key, Field>>;
    using Index = std::unordered_map<Key, Field, std::hash<Key>, std::equal_to<Key>, Allocator>;

public:
    // 기존 레코드는 줄로 읽고 새 청크 본문은 데이터 길이만큼 한 번에 건너뛴다.
    explicit ArchiveFields(std::string_view remaining)
        : m_framed(remaining.starts_with(kBinaryArchiveHeader))
    {
        if (m_framed) remaining.remove_prefix(kBinaryArchiveHeader.size());
        while (!remaining.empty()) {
            const auto newline = remaining.find('\n');
            auto line = remaining.substr(0, newline);
            if (line.ends_with('\r')) line.remove_suffix(1);
            const auto separator = line.find('\t');
            remaining = newline == std::string_view::npos ? std::string_view{} : remaining.substr(newline + 1);
            const bool binary = m_framed && line.starts_with('@');
            if (separator == std::string_view::npos) {
                if (binary) throw std::invalid_argument("Missing archive chunk length.");
                continue;
            }
            auto key = line.substr(0, separator);
            auto data = line.substr(separator + 1);
            if (binary) {
                if (key.size() < 2) throw std::invalid_argument("Missing archive chunk name.");
                key.remove_prefix(1);
                data = takeChunk(data, remaining);
            }
            m_index.insert_or_assign(key, Field{data, binary});
        }
    }

    // 구조 필드가 바이너리 페이로드로 잘못 지정됐는지 검사한다.
    std::optional<std::string_view> text(ArchiveKey key) const { return field(key, false); }

    // 같은 논리 필드의 기존 16진수 표현과 새 바이너리 표현을 모드에 따라 받는다.
    std::optional<std::string_view> blob(ArchiveKey key) const { return field(key, m_framed); }

    // 예산 확인에는 실제로 파싱된 고유 키만 사용한다.
    bool contains(ArchiveKey key) const { return find(key) != m_index.end(); }
    std::size_t size() const { return m_index.size(); }
    bool usesClassicLocale() const { return m_classic; }
    bool usesBinaryFraming() const noexcept { return m_framed; }

    // Reuse one stream and consume the original view, including on older standard libraries.
    std::istream &legacyInput(std::string_view value) const
    {
        m_inputBuffer.reset(value);
        m_input.clear();
        return m_input;
    }

private:
    // 길이의 부호, 오버플로, 입력 끝과 후행 LF를 포인터 접근 전에 검증한다.
    static std::string_view takeChunk(std::string_view length, std::string_view &remaining)
    {
        std::size_t bytes = 0;
        const char *const end = length.data() + length.size();
        const auto parsed = std::from_chars(length.data(), end, bytes);
        if (parsed.ec != std::errc{} || parsed.ptr != end) {
            throw std::invalid_argument("Invalid archive chunk length.");
        }
        if (bytes >= remaining.size() || remaining[bytes] != '\n') {
            throw std::invalid_argument("Truncated archive chunk or missing delimiter.");
        }
        const auto chunk = remaining.substr(0, bytes);
        remaining.remove_prefix(bytes + 1);
        return chunk;
    }

    // 키 조각의 버퍼를 재사용하고 인덱스에 별도 키 문자열을 등록하지 않는다.
    Index::const_iterator find(ArchiveKey key) const
    {
        if (key.suffix.empty()) return m_index.find(key.prefix);
        m_lookup.assign(key.prefix).append(key.suffix);
        return m_index.find(m_lookup);
    }

    // 선택적 필드의 부재와 잘못된 저장 형식을 구분한다.
    std::optional<std::string_view> field(ArchiveKey key, bool binary) const
    {
        const auto found = find(key);
        if (found == m_index.end()) return std::nullopt;
        if (found->second.binary != binary) {
            throw std::invalid_argument("Invalid archive field storage for " + joinedKey(key) + ".");
        }
        return found->second.bytes;
    }

    const bool m_framed;
    ArchiveIndexStorage m_indexStorage;
    Index m_index{Allocator{m_indexStorage}};
    mutable std::string m_lookup;
    mutable ArchiveInputBuffer m_inputBuffer;
    mutable std::istream m_input{&m_inputBuffer};
    const bool m_classic = m_input.getloc() == std::locale::classic();
};

// The classic locale recognizes these ASCII whitespace bytes.
bool archiveWhitespace(char value)
{
    return value == ' ' || (value >= '\t' && value <= '\r');
}

// Remove only leading whitespace, leaving any suffix for the next coordinate or legacy fallback.
std::string_view skipArchiveWhitespace(std::string_view value)
{
    while (!value.empty() && archiveWhitespace(value.front())) value.remove_prefix(1);
    return value;
}

// Use floating-point charconv when the standard library and deployment target support it.
template <typename T>
std::from_chars_result parseArchiveFloat(const char *first, const char *last, T &value)
{
    if constexpr (requires { std::from_chars(first, last, value, std::chars_format::general); }) {
        return std::from_chars(first, last, value, std::chars_format::general);
    }
    return {first, std::errc::invalid_argument};
}

// Parse one classic decimal token. Leave unusual spellings and range failures to legacy extraction.
template <typename T>
bool parseArchiveToken(std::string_view &value, T &number)
{
    auto text = skipArchiveWhitespace(value);
    if (text.starts_with('+')) {
        text.remove_prefix(1);
        if (text.starts_with('-')) return false;
    }
    if (text.empty()) return false;
    const char *const end = text.data() + text.size();
    std::from_chars_result parsed;
    if constexpr (std::is_integral_v<T>) {
        parsed = std::from_chars(text.data(), end, number);
    } else {
        parsed = parseArchiveFloat(text.data(), end, number);
        // Formatted extraction can signal underflow for a representable subnormal.
        const auto magnitude = std::abs(number);
        if (!std::isfinite(number) || (magnitude > 0 && magnitude <= std::numeric_limits<T>::min())) return false;
    }
    if (parsed.ec != std::errc{} || (parsed.ptr != end && !archiveWhitespace(*parsed.ptr))) return false;
    value.remove_prefix(static_cast<std::size_t>(parsed.ptr - value.data()));
    return true;
}

// Distinguish an absent optional size from an invalid token. Parse before narrowing to size_t.
std::size_t readArchiveSize(const ArchiveFields &values,
                            ArchiveKey key,
                            std::size_t limit = std::numeric_limits<std::size_t>::max())
{
    const auto record = values.text(key);
    if (!record) {
        return 0;
    }

    const auto text = *record;
    const char *const end = text.data() + text.size();
    std::uint64_t count = 0;
    const auto parsed = std::from_chars(text.data(), end, count);
    if (parsed.ec == std::errc::invalid_argument || parsed.ptr != end) {
        throw std::invalid_argument("Invalid unsigned decimal for " + joinedKey(key) + ".");
    }
    if (parsed.ec == std::errc::result_out_of_range || count > limit) {
        throw std::invalid_argument("Size exceeds archive limit for " + joinedKey(key) + ".");
    }
    return static_cast<std::size_t>(count);
}

// Every slot must have its own serialized field before a reader allocates the collection.
// The budget is shared across lists of the same kind, including descendants and undo/redo.
std::size_t readCollectionCount(const ArchiveFields &values,
                                std::string_view prefix,
                                std::size_t &remaining,
                                std::string_view entrySuffix,
                                std::string_view alternateSuffix = {})
{
    const ArchiveKey countKey{prefix, ".count"};
    const std::size_t count = readArchiveSize(values, countKey, remaining);
    if (count > values.size()) {
        throw std::invalid_argument("Insufficient archive fields for " + joinedKey(countKey) + ".");
    }
    for (std::size_t index = 0; index < count; ++index) {
        const auto entryPrefix = indexedKey({prefix}, index);
        const ArchiveKey entryKey{entryPrefix, entrySuffix};
        if (!values.contains(entryKey)
                && (alternateSuffix.empty() || !values.contains({entryPrefix, alternateSuffix}))) {
            throw std::invalid_argument("Missing archive field " + joinedKey(entryKey) + ".");
        }
    }
    remaining -= count;
    return count;
}

// Retain the existing fallbacks for missing values and legacy stream extraction failures.
template <typename T>
T readNumber(const ArchiveFields &values,
             ArchiveKey key,
             T fallback = {})
{
    const auto record = values.text(key);
    if (!record) {
        return fallback;
    }

    T result{};
    auto text = *record;
    if (values.usesClassicLocale() && parseArchiveToken(text, result)) return result;
    return (values.legacyInput(*record) >> result) ? result : fallback;
}

bool readBool(const ArchiveFields &values,
              ArchiveKey key,
              bool fallback = false)
{
    return readNumber<int>(values, key, fallback ? 1 : 0) != 0;
}

// 이전 인용 문법을 읽으며 버전 4의 인용 문자열에서는 개행 이스케이프도 복원한다.
std::string readString(const ArchiveFields &values, ArchiveKey key, std::string_view fallback = {})
{
    const auto record = values.text(key);
    if (!record) return std::string(fallback);
    if (!values.usesBinaryFraming() && !values.usesClassicLocale()) {
        std::string result;
        if (values.legacyInput(*record) >> std::quoted(result)) return result;
        return std::string(fallback);
    }

    auto text = skipArchiveWhitespace(*record);
    if (text.empty()) return std::string(fallback);
    if (text.front() != '"') return std::string(text.substr(0, text.find_first_of(" \t\n\v\f\r")));

    text.remove_prefix(1);
    std::string result;
    while (!text.empty()) {
        const auto next = text.find_first_of("\\\"");
        if (next == std::string_view::npos) break;
        result.append(text.substr(0, next));
        if (text[next] == '"') return result;
        text.remove_prefix(next + 1);
        if (text.empty()) break;
        auto escaped = text.front();
        if (values.usesBinaryFraming()) {
            if (escaped == 'n') escaped = '\n';
            else if (escaped == 'r') escaped = '\r';
        }
        result.push_back(escaped);
        text.remove_prefix(1);
    }
    return std::string(fallback);
}

// UUID를 생략하면 기본값을 쓰고, 명시한 UUID는 16바이트 전체를 검증한다.
PaintUuid readUuid(const ArchiveFields &values, ArchiveKey key)
{
    PaintUuid uuid{};
    const auto record = values.text(key);
    if (!record) return uuid;
    if (record->size() != uuid.bytes.size() * 2) {
        throw std::invalid_argument("Invalid UUID length for " + joinedKey(key) + ".");
    }

    for (std::size_t index = 0; index < uuid.bytes.size(); ++index) {
        uuid.bytes[index] = parseHexByte(*record, index * 2, key);
    }
    return uuid;
}

// 인코딩 길이와 표면의 픽셀 수를 먼저 검증한 뒤 최종 벡터에 직접 복원한다.
template <typename T>
std::vector<T> readArchiveVector(const ArchiveFields &values,
                                 ArchiveKey key,
                                 std::optional<std::uint64_t> expectedPixelCount = {})
{
    static_assert(sizeof(T) == 1 || std::is_same_v<T, std::uint32_t>);
    const auto record = values.blob(key).value_or(std::string_view{});
    const bool binary = values.usesBinaryFraming();
    constexpr std::size_t hexDigits = sizeof(T) * 2;
    const std::size_t elementWidth = binary ? sizeof(T) : hexDigits;
    if (record.size() % elementWidth != 0) {
        const char *error = binary ? "Invalid ARGB32 chunk length for " : "Invalid hexadecimal length for ";
        throw std::invalid_argument(error + joinedKey(key) + ".");
    }
    const std::size_t count = record.size() / elementWidth;
    if (expectedPixelCount && count != *expectedPixelCount) {
        throw std::invalid_argument("Surface pixel count mismatch for " + joinedKey(key)
                + " (expected " + std::to_string(*expectedPixelCount) + ", found " + std::to_string(count) + ").");
    }
    if (count == 0) return {};

    if (binary) {
        std::vector<T> output(count);
        if constexpr (sizeof(T) == 1 || std::endian::native == std::endian::little) {
            std::memcpy(output.data(), record.data(), record.size());
        } else {
            // 입력 뷰의 바이트 정렬과 무관하게 리틀엔디언 워드를 조립한다.
            for (std::size_t index = 0; index < output.size(); ++index) {
                const auto offset = index * sizeof(T);
                std::uint32_t value = 0;
                for (unsigned int byte = 0; byte < sizeof(T); ++byte) {
                    value |= static_cast<std::uint32_t>(static_cast<unsigned char>(record[offset + byte])) << (byte * 8);
                }
                output[index] = value;
            }
        }
        return output;
    }

    std::vector<T> output;
    output.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        std::uint32_t value = 0;
        for (std::size_t digit = 0; digit < hexDigits; ++digit) {
            value = (value << 4U) | hexValue(record[index * hexDigits + digit], key);
        }
        output.push_back(static_cast<T>(value));
    }
    return output;
}

// Keep the archive's x-y order for point values.
template <typename Point>
void writePoint(ArchiveOutput &output, ArchiveKey key, const Point &point)
{
    output.numbers(key, point.x, point.y);
}

// Write origin, width and height as one space-delimited record.
template <typename Rect>
void writeRect(ArchiveOutput &output, ArchiveKey key, const Rect &rect)
{
    output.numbers(key, rect.origin.x, rect.origin.y, rect.width, rect.height);
}

// Read tuple fields without constructing a stream for canonical coordinates. Legacy partial tuples
// still keep the components that formatted extraction consumed before a failure.
template <typename... T>
void readCoordinates(const ArchiveFields &values, ArchiveKey key, T &...coordinates)
{
    const auto record = values.text(key);
    if (!record) return;
    auto text = *record;
    if (values.usesClassicLocale() && (parseArchiveToken(text, coordinates) && ...)) return;
    ((coordinates = T{}), ...);
    (values.legacyInput(*record) >> ... >> coordinates);
}

template <typename Point>
Point readPoint(const ArchiveFields &values, ArchiveKey key)
{
    Point point{};
    readCoordinates(values, key, point.x, point.y);
    return point;
}

template <typename Rect>
Rect readRect(const ArchiveFields &values, ArchiveKey key)
{
    Rect rect{};
    readCoordinates(values, key, rect.origin.x, rect.origin.y, rect.width, rect.height);
    return rect;
}

void writeDocumentMetadata(ArchiveOutput &output,
                           std::string_view prefix,
                           const DocumentMetadata &metadata)
{
    output.quoted({prefix, ".title"}, metadata.title);
    output.quoted({prefix, ".author"}, metadata.author);
    output.quoted({prefix, ".storagePath"}, metadata.storagePath);
    output.quoted({prefix, ".createdAt"}, metadata.createdAt);
    output.quoted({prefix, ".modifiedAt"}, metadata.modifiedAt);
    output.line({prefix, ".documentId"}, uuidText(metadata.documentId));
    output.numbers({prefix, ".version"}, metadata.version);
    output.pixels({prefix, ".thumbnail"}, metadata.thumbnail);
    output.numbers({prefix, ".backgroundColor"}, metadata.backgroundColor);
    output.numbers({prefix, ".unit"}, static_cast<int>(metadata.unit));
    output.numbers({prefix, ".intendedExportWidth"}, metadata.intendedExportWidth);
    output.numbers({prefix, ".intendedExportHeight"}, metadata.intendedExportHeight);
    output.numbers({prefix, ".dpiX"}, metadata.dpiX);
    output.numbers({prefix, ".dpiY"}, metadata.dpiY);
    output.quoted({prefix, ".colorSpace"}, metadata.colorSpace);
    output.quoted({prefix, ".appVersion"}, metadata.appVersion);
}

DocumentMetadata readDocumentMetadata(const ArchiveFields &values,
                                      std::string_view prefix)
{
    DocumentMetadata metadata;
    metadata.title = readString(values, {prefix, ".title"});
    metadata.author = readString(values, {prefix, ".author"});
    metadata.storagePath = readString(values, {prefix, ".storagePath"});
    metadata.createdAt = readString(values, {prefix, ".createdAt"});
    metadata.modifiedAt = readString(values, {prefix, ".modifiedAt"});
    metadata.documentId = readUuid(values, {prefix, ".documentId"});
    metadata.version = readNumber<std::uint32_t>(values, {prefix, ".version"}, 1);
    metadata.thumbnail = readArchiveVector<std::uint32_t>(values, {prefix, ".thumbnail"});
    metadata.backgroundColor = readNumber<std::uint32_t>(values, {prefix, ".backgroundColor"});
    metadata.unit = static_cast<DocumentUnit>(readNumber<int>(values, {prefix, ".unit"}));
    metadata.intendedExportWidth = readNumber<Types::Pixel>(values, {prefix, ".intendedExportWidth"});
    metadata.intendedExportHeight = readNumber<Types::Pixel>(values, {prefix, ".intendedExportHeight"});
    metadata.dpiX = readNumber<Types::Scalar>(values, {prefix, ".dpiX"}, 72.0);
    metadata.dpiY = readNumber<Types::Scalar>(values, {prefix, ".dpiY"}, 72.0);
    metadata.colorSpace = readString(values, {prefix, ".colorSpace"}, "sRGB");
    metadata.appVersion = readString(values, {prefix, ".appVersion"});
    return metadata;
}

void readLegacySurfaceMetadata(const ArchiveFields &values,
                               std::string_view prefix,
                               DocumentMetadata &metadata)
{
    if (metadata.title.empty()) {
        metadata.title = readString(values, {prefix, ".title"});
    }
    if (metadata.author.empty()) {
        metadata.author = readString(values, {prefix, ".author"});
    }
    if (metadata.createdAt.empty()) {
        metadata.createdAt = readString(values, {prefix, ".createdAt"});
    }
    if (metadata.modifiedAt.empty()) {
        metadata.modifiedAt = readString(values, {prefix, ".modifiedAt"});
    }
    if (metadata.documentId.bytes == PaintUuid{}.bytes) {
        metadata.documentId = readUuid(values, {prefix, ".documentId"});
    }
    metadata.thumbnail = readArchiveVector<std::uint32_t>(values, {prefix, ".thumbnail"});
    metadata.backgroundColor = readNumber<std::uint32_t>(values, {prefix, ".backgroundColor"});
    metadata.unit = static_cast<DocumentUnit>(readNumber<int>(values, {prefix, ".unit"}));
    metadata.intendedExportWidth = readNumber<Types::Pixel>(values, {prefix, ".intendedExportWidth"});
    metadata.intendedExportHeight = readNumber<Types::Pixel>(values, {prefix, ".intendedExportHeight"});
    metadata.dpiX = readNumber<Types::Scalar>(values, {prefix, ".dpiX"}, 72.0);
    metadata.dpiY = readNumber<Types::Scalar>(values, {prefix, ".dpiY"}, 72.0);
    metadata.colorSpace = readString(values, {prefix, ".colorSpace"}, "sRGB");
    if (metadata.appVersion.empty()) {
        metadata.appVersion = readString(values, {prefix, ".appVersion"});
    }
}

void writeLayerMetadata(ArchiveOutput &output,
                        std::string_view prefix,
                        const LayerMetadata &metadata)
{
    output.line({prefix, ".id"}, uuidText(metadata.id));
    output.quoted({prefix, ".name"}, metadata.name);
    output.line({prefix, ".visible"}, boolText(metadata.visible));
    output.numbers({prefix, ".opacity"}, metadata.opacity);
    output.numbers({prefix, ".blendMode"}, static_cast<int>(metadata.blendMode));
    output.numbers({prefix, ".kind"}, static_cast<int>(metadata.kind));
    output.line({prefix, ".clipsToBelow"}, boolText(metadata.clipsToBelow));
    output.line({prefix, ".alphaLock"}, boolText(metadata.alphaLock));
}

LayerMetadata readLayerMetadata(const ArchiveFields &values,
                                std::string_view prefix)
{
    LayerMetadata metadata;
    metadata.id = readUuid(values, {prefix, ".id"});
    metadata.name = readString(values, {prefix, ".name"});
    metadata.visible = readBool(values, {prefix, ".visible"}, true);
    metadata.opacity = readNumber<Types::Scalar>(values, {prefix, ".opacity"}, 1.0);
    metadata.blendMode = static_cast<RasterBlendMode>(readNumber<int>(values, {prefix, ".blendMode"}));
    metadata.kind = static_cast<LayerKind>(readNumber<int>(values, {prefix, ".kind"}));
    metadata.clipsToBelow = readBool(values, {prefix, ".clipsToBelow"});
    metadata.alphaLock = readBool(values, {prefix, ".alphaLock"});
    return metadata;
}

void writeLayerMask(ArchiveOutput &output,
                    std::string_view prefix,
                    const LayerMask &mask)
{
    output.line({prefix, ".enabled"}, boolText(mask.enabled));
    output.numbers({prefix, ".width"}, mask.width);
    output.numbers({prefix, ".height"}, mask.height);
    output.binary({prefix, ".alpha"}, std::as_bytes(std::span(mask.alpha)));
}

LayerMask readLayerMask(const ArchiveFields &values,
                        std::string_view prefix)
{
    LayerMask mask;
    mask.enabled = readBool(values, {prefix, ".enabled"});
    mask.width = readNumber<Types::Pixel>(values, {prefix, ".width"});
    mask.height = readNumber<Types::Pixel>(values, {prefix, ".height"});
    mask.alpha = readArchiveVector<Types::Byte>(values, {prefix, ".alpha"});
    return mask;
}

void writeSurface(ArchiveOutput &output,
                  std::string_view prefix,
                  const DrawingSurface &surface)
{
    output.numbers({prefix, ".width"}, surface.width);
    output.numbers({prefix, ".height"}, surface.height);
    output.numbers({prefix, ".pixelFormat"}, static_cast<int>(surface.pixelFormat));
    output.numbers({prefix, ".colorSpace"}, static_cast<int>(surface.colorSpace));
    output.numbers({prefix, ".dpiX"}, surface.dpiX);
    output.numbers({prefix, ".dpiY"}, surface.dpiY);
    output.numbers({prefix, ".backingStore"}, static_cast<int>(surface.backingStore));
    output.pixels({prefix, ".pixels"}, surface.pixels);
    output.numbers({prefix, ".dirtyRegion.count"}, surface.dirtyRegion.size());
    for (std::size_t index = 0; index < surface.dirtyRegion.size(); ++index) {
        const auto key = indexedKey({prefix, ".dirtyRegion"}, index);
        writeRect(output, {key}, surface.dirtyRegion[index]);
    }
    writeRect(output, {prefix, ".dirtyBounds"}, surface.dirtyBounds);
    output.numbers({prefix, ".textureHandle"}, surface.textureHandle);
}

// 32비트 치수의 곱을 64비트로 계산해 선언한 면적과 실제 픽셀 레코드를 비교한다.
DrawingSurface readSurface(const ArchiveFields &values,
                           std::string_view prefix,
                           std::size_t &remainingDirtyRects)
{
    const std::string dirtyPrefix = joinedKey({prefix, ".dirtyRegion"});
    const std::size_t dirtyCount = readCollectionCount(values, dirtyPrefix, remainingDirtyRects, "");
    constexpr auto maxDimension = static_cast<std::size_t>(std::numeric_limits<Types::Pixel>::max());
    const auto width = readArchiveSize(values, {prefix, ".width"}, maxDimension);
    const auto height = readArchiveSize(values, {prefix, ".height"}, maxDimension);
    const auto pixelCount = static_cast<std::uint64_t>(width) * height;
    DrawingSurface surface;
    surface.width = static_cast<Types::Pixel>(width);
    surface.height = static_cast<Types::Pixel>(height);
    surface.pixelFormat = static_cast<SurfacePixelFormat>(readNumber<int>(values, {prefix, ".pixelFormat"}));
    surface.colorSpace = static_cast<SurfaceColorSpace>(readNumber<int>(values, {prefix, ".colorSpace"}));
    surface.dpiX = readNumber<Types::Scalar>(values, {prefix, ".dpiX"}, 72.0);
    surface.dpiY = readNumber<Types::Scalar>(values, {prefix, ".dpiY"}, 72.0);
    surface.backingStore = static_cast<SurfaceBackingStore>(readNumber<int>(values, {prefix, ".backingStore"}));
    surface.pixels = readArchiveVector<std::uint32_t>(values, {prefix, ".pixels"}, pixelCount);
    surface.dirtyRegion.reserve(dirtyCount);
    for (std::size_t index = 0; index < dirtyCount; ++index) {
        const auto key = indexedKey({dirtyPrefix}, index);
        surface.dirtyRegion.push_back(readRect<DevicePixelRect>(values, {key}));
    }
    surface.dirtyBounds = readRect<DevicePixelRect>(values, {prefix, ".dirtyBounds"});
    surface.textureHandle = readNumber<std::uint64_t>(values, {prefix, ".textureHandle"});
    return surface;
}

void writeLayer(ArchiveOutput &output,
                std::string_view prefix,
                const Layer &layer)
{
    writeSurface(output, joinedKey({prefix, ".surface"}), layer.surface);
    writeLayerMetadata(output, joinedKey({prefix, ".metadata"}), layer.metadata);
    writeLayerMask(output, joinedKey({prefix, ".mask"}), layer.mask);
    output.numbers({prefix, ".children.count"}, layer.children.size());
    for (std::size_t index = 0; index < layer.children.size(); ++index) {
        writeLayer(output, indexedKey({prefix, ".children"}, index), layer.children[index]);
    }
}

std::vector<Layer> readLayers(const ArchiveFields &values,
                             std::string_view prefix,
                             DocumentArchiveReadLimits &remaining,
                             std::size_t depth);

// A layer receives only children from a depth-checked list and shares the archive's collection limits.
Layer readLayer(const ArchiveFields &values,
                std::string_view prefix,
                DocumentArchiveReadLimits &remaining,
                std::size_t depth)
{
    Layer layer;
    layer.children = readLayers(values, joinedKey({prefix, ".children"}), remaining, depth + 1);
    layer.surface = readSurface(values, joinedKey({prefix, ".surface"}), remaining.maxDirtyRects);
    layer.metadata = readLayerMetadata(values, joinedKey({prefix, ".metadata"}));
    layer.mask = readLayerMask(values, joinedKey({prefix, ".mask"}));
    return layer;
}

// Check the root and child lists through the same gate before reserving or entering any layer.
std::vector<Layer> readLayers(const ArchiveFields &values,
                             std::string_view prefix,
                             DocumentArchiveReadLimits &remaining,
                             std::size_t depth)
{
    const std::size_t layerCount = readCollectionCount(values, prefix, remaining.maxLayers, ".surface.width");
    if (layerCount != 0 && depth > remaining.maxLayerDepth) {
        throw std::invalid_argument("Layer depth exceeds archive limit.");
    }
    std::vector<Layer> layers;
    layers.reserve(layerCount);
    for (std::size_t index = 0; index < layerCount; ++index) {
        layers.push_back(readLayer(values, indexedKey({prefix}, index), remaining, depth));
    }
    return layers;
}

void writeBrushDynamicsResponses(ArchiveOutput &output,
                                 std::string_view prefix,
                                 const BrushDynamics &dynamics);

void writeBrushDynamics(ArchiveOutput &output,
                        std::string_view prefix,
                        const BrushDynamics &dynamics)
{
    output.line({prefix, ".pressureInputEnabled"}, boolText(dynamics.pressureInputEnabled));
    output.line({prefix, ".velocityInputEnabled"}, boolText(dynamics.velocityInputEnabled));
    output.line({prefix, ".tiltInputEnabled"}, boolText(dynamics.tiltInputEnabled));
    output.line({prefix, ".randomInputEnabled"}, boolText(dynamics.randomInputEnabled));
    output.line({prefix, ".pressureToSizeEnabled"}, boolText(dynamics.pressureToSizeEnabled));
    output.line({prefix, ".pressureToOpacityEnabled"}, boolText(dynamics.pressureToOpacityEnabled));
    output.line({prefix, ".pressureToFlowEnabled"}, boolText(dynamics.pressureToFlowEnabled));
    output.line({prefix, ".velocityToSpacingEnabled"}, boolText(dynamics.velocityToSpacingEnabled));
    output.line({prefix, ".velocityToOpacityEnabled"}, boolText(dynamics.velocityToOpacityEnabled));
    output.line({prefix, ".velocityToDryOutEnabled"}, boolText(dynamics.velocityToDryOutEnabled));
    output.line({prefix, ".tiltToEllipseEnabled"}, boolText(dynamics.tiltToEllipseEnabled));
    output.line({prefix, ".rotationJitterEnabled"}, boolText(dynamics.rotationJitterEnabled));
    output.line({prefix, ".grainJitterEnabled"}, boolText(dynamics.grainJitterEnabled));
    output.numbers({prefix, ".pressureToSize"}, dynamics.pressureToSize);
    output.numbers({prefix, ".pressureToOpacity"}, dynamics.pressureToOpacity);
    output.numbers({prefix, ".pressureToFlow"}, dynamics.pressureToFlow);
    output.numbers({prefix, ".velocityToSpacing"}, dynamics.velocityToSpacing);
    output.numbers({prefix, ".velocityToOpacity"}, dynamics.velocityToOpacity);
    output.numbers({prefix, ".velocityToDryOut"}, dynamics.velocityToDryOut);
    output.line({prefix, ".tiltToRotation"}, boolText(dynamics.tiltToRotation));
    output.numbers({prefix, ".tiltToEllipse"}, dynamics.tiltToEllipse);
    output.line({prefix, ".tiltToTextureDirection"}, boolText(dynamics.tiltToTextureDirection));
    output.numbers({prefix, ".rotationJitter"}, dynamics.rotationJitter);
    output.numbers({prefix, ".grainJitter"}, dynamics.grainJitter);
    writeBrushDynamicsResponses(output, prefix, dynamics);
}

void writeBrushDynamicsResponseCurve(ArchiveOutput &output,
                                     std::string_view prefix,
                                     const BrushDynamicsResponseCurve &curve)
{
    output.line({prefix, ".enabled"}, boolText(curve.enabled));
    output.numbers({prefix, ".min"}, curve.min);
    output.numbers({prefix, ".center"}, curve.center);
    output.numbers({prefix, ".max"}, curve.max);
    output.numbers({prefix, ".jitter"}, curve.jitter);
    output.numbers({prefix, ".easing"}, static_cast<int>(curve.easing));
}

void writeBrushDynamicsPropertyResponse(ArchiveOutput &output,
                                        std::string_view prefix,
                                        const BrushDynamicsPropertyResponse &response)
{
    output.line({prefix, ".enabled"}, boolText(response.enabled));
    output.numbers({prefix, ".neutral"}, response.neutral);
    output.numbers({prefix, ".combineMode"}, static_cast<int>(response.combineMode));
    writeBrushDynamicsResponseCurve(output, joinedKey({prefix, ".pressure"}), response.pressure);
    writeBrushDynamicsResponseCurve(output, joinedKey({prefix, ".velocity"}), response.velocity);
    writeBrushDynamicsResponseCurve(output, joinedKey({prefix, ".tilt"}), response.tilt);
    writeBrushDynamicsResponseCurve(output, joinedKey({prefix, ".random"}), response.random);
}

void writeBrushDynamicsResponses(ArchiveOutput &output,
                                 std::string_view prefix,
                                 const BrushDynamics &dynamics)
{
    writeBrushDynamicsPropertyResponse(output, joinedKey({prefix, ".sizeResponse"}), dynamics.sizeResponse);
    writeBrushDynamicsPropertyResponse(output, joinedKey({prefix, ".flowResponse"}), dynamics.flowResponse);
    writeBrushDynamicsPropertyResponse(output, joinedKey({prefix, ".opacityResponse"}), dynamics.opacityResponse);
    writeBrushDynamicsPropertyResponse(output, joinedKey({prefix, ".spacingResponse"}), dynamics.spacingResponse);
    writeBrushDynamicsPropertyResponse(output, joinedKey({prefix, ".scatterResponse"}), dynamics.scatterResponse);
    writeBrushDynamicsPropertyResponse(output, joinedKey({prefix, ".rotationResponse"}), dynamics.rotationResponse);
    writeBrushDynamicsPropertyResponse(output, joinedKey({prefix, ".textureDepthResponse"}), dynamics.textureDepthResponse);
    writeBrushDynamicsPropertyResponse(output, joinedKey({prefix, ".wetnessResponse"}), dynamics.wetnessResponse);
    writeBrushDynamicsPropertyResponse(output, joinedKey({prefix, ".dryOutResponse"}), dynamics.dryOutResponse);
    writeBrushDynamicsPropertyResponse(output, joinedKey({prefix, ".bristleSpreadResponse"}), dynamics.bristleSpreadResponse);
}

BrushDynamicsResponseCurve readBrushDynamicsResponseCurve(const ArchiveFields &values,
                                                          std::string_view prefix,
                                                          BrushDynamicsResponseCurve curve)
{
    curve.enabled = readBool(values, {prefix, ".enabled"}, curve.enabled);
    curve.min = readNumber<Types::Scalar>(values, {prefix, ".min"}, curve.min);
    curve.center = readNumber<Types::Scalar>(values, {prefix, ".center"}, curve.center);
    curve.max = readNumber<Types::Scalar>(values, {prefix, ".max"}, curve.max);
    curve.jitter = readNumber<Types::Scalar>(values, {prefix, ".jitter"}, curve.jitter);
    curve.easing = static_cast<BrushDynamicsEasing>(
            readNumber<int>(values, {prefix, ".easing"}, static_cast<int>(curve.easing)));
    return curve;
}

BrushDynamicsPropertyResponse readBrushDynamicsPropertyResponse(const ArchiveFields &values,
                                                                std::string_view prefix,
                                                                BrushDynamicsPropertyResponse response)
{
    response.enabled = readBool(values, {prefix, ".enabled"}, response.enabled);
    response.neutral = readNumber<Types::Scalar>(values, {prefix, ".neutral"}, response.neutral);
    response.combineMode = static_cast<BrushDynamicsCombineMode>(
            readNumber<int>(values, {prefix, ".combineMode"}, static_cast<int>(response.combineMode)));
    response.pressure = readBrushDynamicsResponseCurve(values, joinedKey({prefix, ".pressure"}), response.pressure);
    response.velocity = readBrushDynamicsResponseCurve(values, joinedKey({prefix, ".velocity"}), response.velocity);
    response.tilt = readBrushDynamicsResponseCurve(values, joinedKey({prefix, ".tilt"}), response.tilt);
    response.random = readBrushDynamicsResponseCurve(values, joinedKey({prefix, ".random"}), response.random);
    return response;
}

void readBrushDynamicsResponses(const ArchiveFields &values,
                                std::string_view prefix,
                                BrushDynamics &dynamics)
{
    dynamics.sizeResponse = readBrushDynamicsPropertyResponse(values, joinedKey({prefix, ".sizeResponse"}), dynamics.sizeResponse);
    dynamics.flowResponse = readBrushDynamicsPropertyResponse(values, joinedKey({prefix, ".flowResponse"}), dynamics.flowResponse);
    dynamics.opacityResponse = readBrushDynamicsPropertyResponse(values, joinedKey({prefix, ".opacityResponse"}), dynamics.opacityResponse);
    dynamics.spacingResponse = readBrushDynamicsPropertyResponse(values, joinedKey({prefix, ".spacingResponse"}), dynamics.spacingResponse);
    dynamics.scatterResponse = readBrushDynamicsPropertyResponse(values, joinedKey({prefix, ".scatterResponse"}), dynamics.scatterResponse);
    dynamics.rotationResponse = readBrushDynamicsPropertyResponse(values, joinedKey({prefix, ".rotationResponse"}), dynamics.rotationResponse);
    dynamics.textureDepthResponse = readBrushDynamicsPropertyResponse(values, joinedKey({prefix, ".textureDepthResponse"}), dynamics.textureDepthResponse);
    dynamics.wetnessResponse = readBrushDynamicsPropertyResponse(values, joinedKey({prefix, ".wetnessResponse"}), dynamics.wetnessResponse);
    dynamics.dryOutResponse = readBrushDynamicsPropertyResponse(values, joinedKey({prefix, ".dryOutResponse"}), dynamics.dryOutResponse);
    dynamics.bristleSpreadResponse = readBrushDynamicsPropertyResponse(values, joinedKey({prefix, ".bristleSpreadResponse"}), dynamics.bristleSpreadResponse);
}

BrushDynamics readBrushDynamics(const ArchiveFields &values,
                                std::string_view prefix)
{
    BrushDynamics dynamics;
    dynamics.pressureInputEnabled = readBool(values, {prefix, ".pressureInputEnabled"}, true);
    dynamics.velocityInputEnabled = readBool(values, {prefix, ".velocityInputEnabled"}, true);
    dynamics.tiltInputEnabled = readBool(values, {prefix, ".tiltInputEnabled"}, true);
    dynamics.randomInputEnabled = readBool(values, {prefix, ".randomInputEnabled"}, true);
    dynamics.pressureToSizeEnabled = readBool(values, {prefix, ".pressureToSizeEnabled"}, true);
    dynamics.pressureToOpacityEnabled = readBool(values, {prefix, ".pressureToOpacityEnabled"}, true);
    dynamics.pressureToFlowEnabled = readBool(values, {prefix, ".pressureToFlowEnabled"}, true);
    dynamics.velocityToSpacingEnabled = readBool(values, {prefix, ".velocityToSpacingEnabled"}, true);
    dynamics.velocityToOpacityEnabled = readBool(values, {prefix, ".velocityToOpacityEnabled"}, true);
    dynamics.velocityToDryOutEnabled = readBool(values, {prefix, ".velocityToDryOutEnabled"}, true);
    dynamics.tiltToEllipseEnabled = readBool(values, {prefix, ".tiltToEllipseEnabled"}, true);
    dynamics.rotationJitterEnabled = readBool(values, {prefix, ".rotationJitterEnabled"}, true);
    dynamics.grainJitterEnabled = readBool(values, {prefix, ".grainJitterEnabled"}, true);
    dynamics.pressureToSize = readNumber<Types::Scalar>(values, {prefix, ".pressureToSize"});
    dynamics.pressureToOpacity = readNumber<Types::Scalar>(values, {prefix, ".pressureToOpacity"});
    dynamics.pressureToFlow = readNumber<Types::Scalar>(values, {prefix, ".pressureToFlow"});
    dynamics.velocityToSpacing = readNumber<Types::Scalar>(values, {prefix, ".velocityToSpacing"});
    dynamics.velocityToOpacity = readNumber<Types::Scalar>(values, {prefix, ".velocityToOpacity"});
    dynamics.velocityToDryOut = readNumber<Types::Scalar>(values, {prefix, ".velocityToDryOut"});
    dynamics.tiltToRotation = readBool(values, {prefix, ".tiltToRotation"});
    dynamics.tiltToEllipse = readNumber<Types::Scalar>(values, {prefix, ".tiltToEllipse"});
    dynamics.tiltToTextureDirection = readBool(values, {prefix, ".tiltToTextureDirection"});
    dynamics.rotationJitter = readNumber<Types::Scalar>(values, {prefix, ".rotationJitter"});
    dynamics.grainJitter = readNumber<Types::Scalar>(values, {prefix, ".grainJitter"});
    readBrushDynamicsResponses(values, prefix, dynamics);
    return dynamics;
}

void writeBrushTextureAssetCache(ArchiveOutput &output,
                                 std::string_view prefix,
                                 const BrushTextureAssetCache &cache)
{
    output.line({prefix, ".enabled"}, boolText(cache.enabled));
    output.line({prefix, ".assetId"}, uuidText(cache.assetId));
    // Version 2 preserves the original; old readers still require single-line fields.
    auto legacyKey = cache.cacheKey;
    std::replace(legacyKey.begin(), legacyKey.end(), '\n', ' ');
    std::replace(legacyKey.begin(), legacyKey.end(), '\r', ' ');
    output.quoted({prefix, ".cacheKey"}, legacyKey);
    output.numbers({prefix, ".revision"}, cache.revision);
    output.numbers({prefix, ".width"}, cache.width);
    output.numbers({prefix, ".height"}, cache.height);
    output.binary({prefix, ".alpha"}, std::as_bytes(std::span(cache.alpha)));
}

void writeBrushTexture(ArchiveOutput &output,
                       std::string_view prefix,
                       const BrushTexture &texture)
{
    output.line({prefix, ".enabled"}, boolText(texture.enabled));
    output.numbers({prefix, ".space"}, static_cast<int>(texture.space));
    output.numbers({prefix, ".width"}, texture.width);
    output.numbers({prefix, ".height"}, texture.height);
    output.binary({prefix, ".alpha"}, std::as_bytes(std::span(texture.alpha)));
    output.numbers({prefix, ".grainStrength"}, texture.grainStrength);
    output.numbers({prefix, ".strength"}, texture.strength);
    output.numbers({prefix, ".scale"}, texture.scale);
    output.numbers({prefix, ".rotationRadians"}, texture.rotationRadians);
    output.numbers({prefix, ".offsetX"}, texture.offsetX);
    output.numbers({prefix, ".offsetY"}, texture.offsetY);
    output.numbers({prefix, ".scaleJitter"}, texture.scaleJitter);
    output.numbers({prefix, ".rotationJitter"}, texture.rotationJitter);
    writeBrushTextureAssetCache(output, joinedKey({prefix, ".assetCache"}), texture.assetCache);
}

BrushTextureAssetCache readBrushTextureAssetCache(const ArchiveFields &values,
                                                  std::string_view prefix)
{
    BrushTextureAssetCache cache;
    cache.enabled = readBool(values, {prefix, ".enabled"});
    cache.assetId = readUuid(values, {prefix, ".assetId"});
    cache.cacheKey = readString(values, {prefix, ".cacheKey"});
    cache.revision = readNumber<std::uint64_t>(values, {prefix, ".revision"});
    cache.width = readNumber<Types::Pixel>(values, {prefix, ".width"});
    cache.height = readNumber<Types::Pixel>(values, {prefix, ".height"});
    cache.alpha = readArchiveVector<Types::Byte>(values, {prefix, ".alpha"});
    return cache;
}

BrushTexture readBrushTexture(const ArchiveFields &values,
                              std::string_view prefix)
{
    BrushTexture texture;
    texture.enabled = readBool(values, {prefix, ".enabled"});
    texture.space = static_cast<BrushTextureSpace>(
            readNumber<int>(values, {prefix, ".space"}, static_cast<int>(texture.space)));
    texture.width = readNumber<Types::Pixel>(values, {prefix, ".width"});
    texture.height = readNumber<Types::Pixel>(values, {prefix, ".height"});
    texture.alpha = readArchiveVector<Types::Byte>(values, {prefix, ".alpha"});
    texture.grainStrength = readNumber<Types::Scalar>(values, {prefix, ".grainStrength"});
    texture.strength = readNumber<Types::Scalar>(values, {prefix, ".strength"}, 1.0);
    texture.scale = readNumber<Types::Scalar>(values, {prefix, ".scale"}, 1.0);
    texture.rotationRadians = readNumber<Types::Scalar>(values, {prefix, ".rotationRadians"});
    texture.offsetX = readNumber<Types::Scalar>(values, {prefix, ".offsetX"});
    texture.offsetY = readNumber<Types::Scalar>(values, {prefix, ".offsetY"});
    texture.scaleJitter = readNumber<Types::Scalar>(values, {prefix, ".scaleJitter"});
    texture.rotationJitter = readNumber<Types::Scalar>(values, {prefix, ".rotationJitter"});
    texture.assetCache = readBrushTextureAssetCache(values, joinedKey({prefix, ".assetCache"}));
    return texture;
}

void writeBrushMaterial(ArchiveOutput &output,
                        std::string_view prefix,
                        const BrushMaterial &material)
{
    writeBrushTexture(output, joinedKey({prefix, ".texture"}), material.texture);
    writeBrushTexture(output, joinedKey({prefix, ".paperGrain"}), material.paperGrain);
    output.line({prefix, ".dualBrush.enabled"}, boolText(material.dualBrush.enabled));
    output.numbers({prefix, ".dualBrush.compositeMode"}, static_cast<int>(material.dualBrush.compositeMode));
    output.numbers({prefix, ".dualBrush.scale"}, material.dualBrush.scale);
    output.numbers({prefix, ".dualBrush.spacingRatio"}, material.dualBrush.spacingRatio);
    output.numbers({prefix, ".dualBrush.opacity"}, material.dualBrush.opacity);
    output.numbers({prefix, ".dualBrush.rotationRadians"}, material.dualBrush.rotationRadians);
    output.numbers({prefix, ".dualBrush.offsetX"}, material.dualBrush.offsetX);
    output.numbers({prefix, ".dualBrush.offsetY"}, material.dualBrush.offsetY);
    output.numbers({prefix, ".dualBrush.scaleJitter"}, material.dualBrush.scaleJitter);
    output.numbers({prefix, ".dualBrush.rotationJitter"}, material.dualBrush.rotationJitter);
    output.binary({prefix, ".dualBrush.alpha"}, std::as_bytes(std::span(material.dualBrush.alpha)));
    output.numbers({prefix, ".dualBrush.width"}, material.dualBrush.width);
    output.numbers({prefix, ".dualBrush.height"}, material.dualBrush.height);
    output.line({prefix, ".scatter.enabled"}, boolText(material.scatter.enabled));
    output.numbers({prefix, ".scatter.radius"}, material.scatter.radius);
    output.numbers({prefix, ".scatter.count"}, material.scatter.count);
    output.line({prefix, ".simulation.enabled"}, boolText(material.simulation.enabled));
    output.numbers({prefix, ".simulation.model"}, static_cast<int>(material.simulation.model));
    output.numbers({prefix, ".simulation.wetness"}, material.simulation.wetness);
    output.numbers({prefix, ".simulation.smudgeStrength"}, material.simulation.smudgeStrength);
    output.numbers({prefix, ".simulation.mixStrength"}, material.simulation.mixStrength);
    output.numbers({prefix, ".simulation.pickup"}, material.simulation.pickup);
    output.numbers({prefix, ".simulation.deposit"}, material.simulation.deposit);
    output.line({prefix, ".bristle.enabled"}, boolText(material.bristle.enabled));
    output.numbers({prefix, ".bristle.shape"}, static_cast<int>(material.bristle.shape));
    output.numbers({prefix, ".bristle.count"}, material.bristle.count);
    output.numbers({prefix, ".bristle.length"}, material.bristle.length);
    output.numbers({prefix, ".bristle.stiffness"}, material.bristle.stiffness);
    output.numbers({prefix, ".bristle.spread"}, material.bristle.spread);
}

BrushMaterial readBrushMaterial(const ArchiveFields &values,
                                std::string_view prefix)
{
    BrushMaterial material;
    material.texture = readBrushTexture(values, joinedKey({prefix, ".texture"}));
    material.paperGrain = readBrushTexture(values, joinedKey({prefix, ".paperGrain"}));
    material.dualBrush.enabled = readBool(values, {prefix, ".dualBrush.enabled"});
    material.dualBrush.compositeMode = static_cast<DualBrushCompositeMode>(
            readNumber<int>(values, {prefix, ".dualBrush.compositeMode"}, static_cast<int>(material.dualBrush.compositeMode)));
    material.dualBrush.scale = readNumber<Types::Scalar>(values, {prefix, ".dualBrush.scale"}, 1.0);
    material.dualBrush.spacingRatio = readNumber<Types::Scalar>(values, {prefix, ".dualBrush.spacingRatio"}, 1.0);
    material.dualBrush.opacity = readNumber<Types::Scalar>(values, {prefix, ".dualBrush.opacity"}, 1.0);
    material.dualBrush.rotationRadians = readNumber<Types::Scalar>(values, {prefix, ".dualBrush.rotationRadians"});
    material.dualBrush.offsetX = readNumber<Types::Scalar>(values, {prefix, ".dualBrush.offsetX"});
    material.dualBrush.offsetY = readNumber<Types::Scalar>(values, {prefix, ".dualBrush.offsetY"});
    material.dualBrush.scaleJitter = readNumber<Types::Scalar>(values, {prefix, ".dualBrush.scaleJitter"});
    material.dualBrush.rotationJitter = readNumber<Types::Scalar>(values, {prefix, ".dualBrush.rotationJitter"});
    material.dualBrush.alpha = readArchiveVector<Types::Byte>(values, {prefix, ".dualBrush.alpha"});
    material.dualBrush.width = readNumber<Types::Pixel>(values, {prefix, ".dualBrush.width"});
    material.dualBrush.height = readNumber<Types::Pixel>(values, {prefix, ".dualBrush.height"});
    material.scatter.enabled = readBool(values, {prefix, ".scatter.enabled"});
    material.scatter.radius = readNumber<Types::Scalar>(values, {prefix, ".scatter.radius"});
    material.scatter.count = readNumber<std::uint32_t>(values, {prefix, ".scatter.count"}, 1);
    material.simulation.enabled = readBool(values, {prefix, ".simulation.enabled"});
    material.simulation.model = static_cast<BrushSimulationModel>(
            readNumber<int>(values, {prefix, ".simulation.model"}));
    material.simulation.wetness = readNumber<Types::Scalar>(values, {prefix, ".simulation.wetness"});
    material.simulation.smudgeStrength = readNumber<Types::Scalar>(values, {prefix, ".simulation.smudgeStrength"});
    material.simulation.mixStrength = readNumber<Types::Scalar>(values, {prefix, ".simulation.mixStrength"});
    material.simulation.pickup = readNumber<Types::Scalar>(values, {prefix, ".simulation.pickup"});
    material.simulation.deposit = readNumber<Types::Scalar>(values, {prefix, ".simulation.deposit"}, 1.0);
    material.bristle.enabled = readBool(values, {prefix, ".bristle.enabled"});
    material.bristle.shape = static_cast<BristleShape>(readNumber<int>(values, {prefix, ".bristle.shape"}));
    material.bristle.count = readNumber<std::uint32_t>(values, {prefix, ".bristle.count"});
    material.bristle.length = readNumber<Types::Scalar>(values, {prefix, ".bristle.length"});
    material.bristle.stiffness = readNumber<Types::Scalar>(values, {prefix, ".bristle.stiffness"}, 1.0);
    material.bristle.spread = readNumber<Types::Scalar>(values, {prefix, ".bristle.spread"}, 0.0);
    return material;
}

void writeBrushSnapshot(ArchiveOutput &output,
                        std::string_view prefix,
                        const BrushSnapshot &brush)
{
    output.binary({prefix, ".presetV2"}, serializeBrushPreset(restoreBrushPreset(brush)));
    output.line({prefix, ".brushId"}, uuidText(brush.brushId));
    auto legacyName = brush.name;
    std::replace(legacyName.begin(), legacyName.end(), '\n', ' ');
    std::replace(legacyName.begin(), legacyName.end(), '\r', ' ');
    output.quoted({prefix, ".name"}, legacyName);
    output.numbers({prefix, ".tip.width"}, brush.tip.width);
    output.numbers({prefix, ".tip.height"}, brush.tip.height);
    output.binary({prefix, ".tip.mask"}, std::as_bytes(std::span(brush.tip.mask)));
    output.numbers({prefix, ".size"}, brush.size);
    output.numbers({prefix, ".opacity"}, brush.opacity);
    output.numbers({prefix, ".hardness"}, brush.hardness);
    output.numbers({prefix, ".flow"}, brush.flow);
    output.numbers({prefix, ".density"}, brush.density);
    writeBrushDynamics(output, joinedKey({prefix, ".dynamics"}), brush.dynamics);
    writeBrushMaterial(output, joinedKey({prefix, ".material"}), brush.material);
}

BrushSnapshot readBrushSnapshot(const ArchiveFields &values,
                                std::string_view prefix)
{
    const ArchiveKey embeddedKey{prefix, ".presetV2"};
    if (const auto embedded = values.blob(embeddedKey)) {
        constexpr std::size_t maxPresetBytes = 40 * 1024 * 1024;
        const bool binary = values.usesBinaryFraming();
        if (embedded->size() > (binary ? maxPresetBytes : maxPresetBytes * 2)
                || (!binary && (embedded->size() % 2
                        || embedded->find_first_not_of("0123456789abcdefABCDEF") != std::string_view::npos))) {
            throw std::invalid_argument("Invalid embedded brush preset bytes for " + joinedKey(embeddedKey) + ".");
        }
        std::string payload;
        if (binary) {
            payload.assign(*embedded);
        } else {
            // 기존 프리셋의 16진수 바이트를 중간 벡터 없이 최종 문자열로 복원한다.
            payload.resize(embedded->size() / 2);
            for (std::size_t index = 0; index < payload.size(); ++index) {
                payload[index] = static_cast<char>(parseHexByte(*embedded, index * 2, embeddedKey));
            }
        }
        const auto decoded = readBrushPreset(payload);
        if (!decoded.preset) throw std::invalid_argument(decoded.errors.front());
        return snapshotBrushPreset(*decoded.preset);
    }
    BrushSnapshot brush;
    brush.brushId = readUuid(values, {prefix, ".brushId"});
    brush.name = readString(values, {prefix, ".name"});
    brush.tip.width = readNumber<int>(values, {prefix, ".tip.width"});
    brush.tip.height = readNumber<int>(values, {prefix, ".tip.height"});
    brush.tip.mask = readArchiveVector<std::byte>(values, {prefix, ".tip.mask"});
    brush.size = readNumber<float>(values, {prefix, ".size"});
    brush.opacity = readNumber<float>(values, {prefix, ".opacity"});
    brush.hardness = readNumber<float>(values, {prefix, ".hardness"});
    brush.flow = readNumber<float>(values, {prefix, ".flow"});
    brush.density = readNumber<float>(values, {prefix, ".density"});
    brush.dynamics = readBrushDynamics(values, joinedKey({prefix, ".dynamics"}));
    brush.material = readBrushMaterial(values, joinedKey({prefix, ".material"}));
    return brush;
}

void writeCommand(ArchiveOutput &output,
                  std::string_view prefix,
                  const Command &command)
{
    output.numbers({prefix, ".sequence"}, command.sequence);
    output.quoted({prefix, ".label"}, command.label);
    output.line({prefix, ".targetId"}, uuidText(command.targetId));
    output.numbers({prefix, ".kind"}, static_cast<int>(command.kind));
    output.numbers({prefix, ".scope"}, static_cast<int>(command.scope));
    output.line({prefix, ".transactionId"}, uuidText(command.transactionId));
    output.numbers({prefix, ".timestamp"}, command.timestamp);
    output.quoted({prefix, ".coalescingKey"}, command.coalescingKey);
    output.line({prefix, ".reversible"}, boolText(command.reversible));
    output.line({prefix, ".committed"}, boolText(command.committed));
    writeRect(output, {prefix, ".dirtyBounds"}, command.dirtyBounds);
    output.numbers({prefix, ".patches.count"}, command.patches.size());
    for (std::size_t index = 0; index < command.patches.size(); ++index) {
        const std::string patchPrefix = indexedKey({prefix, ".patches"}, index);
        const CommandPatch &patch = command.patches[index];
        output.line({patchPrefix, ".targetId"}, uuidText(patch.targetId));
        output.numbers({patchPrefix, ".scope"}, static_cast<int>(patch.scope));
        output.numbers({patchPrefix, ".before.storage"}, static_cast<int>(patch.beforeState.storage));
        output.quoted({patchPrefix, ".before.mimeType"}, patch.beforeState.mimeType);
        output.line({patchPrefix, ".before.assetId"}, uuidText(patch.beforeState.assetId));
        output.binary({patchPrefix, ".before.bytes"}, std::as_bytes(std::span(patch.beforeState.bytes)));
        output.numbers({patchPrefix, ".after.storage"}, static_cast<int>(patch.afterState.storage));
        output.quoted({patchPrefix, ".after.mimeType"}, patch.afterState.mimeType);
        output.line({patchPrefix, ".after.assetId"}, uuidText(patch.afterState.assetId));
        output.binary({patchPrefix, ".after.bytes"}, std::as_bytes(std::span(patch.afterState.bytes)));
        writeRect(output, {patchPrefix, ".dirtyBounds"}, patch.dirtyBounds);
    }
}

// 새 이력 슬롯을 채운다. 패치 수를 공통 예산으로 검증한 뒤 실제 데이터를 읽는다.
void readCommand(const ArchiveFields &values,
                 std::string_view prefix,
                 std::size_t &remainingPatches,
                 Command &command)
{
    const std::string patchesPrefix = joinedKey({prefix, ".patches"});
    const std::size_t patchCount = readCollectionCount(values, patchesPrefix, remainingPatches, ".targetId");
    command.sequence = readNumber<std::uint64_t>(values, {prefix, ".sequence"});
    command.label = readString(values, {prefix, ".label"});
    command.targetId = readUuid(values, {prefix, ".targetId"});
    command.kind = static_cast<CommandKind>(readNumber<int>(values, {prefix, ".kind"}));
    command.scope = static_cast<CommandScope>(readNumber<int>(values, {prefix, ".scope"}));
    command.transactionId = readUuid(values, {prefix, ".transactionId"});
    command.timestamp = readNumber<Types::Scalar>(values, {prefix, ".timestamp"});
    command.coalescingKey = readString(values, {prefix, ".coalescingKey"});
    command.reversible = readBool(values, {prefix, ".reversible"}, true);
    command.committed = readBool(values, {prefix, ".committed"}, true);
    command.dirtyBounds = readRect<DocumentRect>(values, {prefix, ".dirtyBounds"});
    command.patches.reserve(patchCount);
    for (std::size_t index = 0; index < patchCount; ++index) {
        const std::string patchPrefix = indexedKey({patchesPrefix}, index);
        CommandPatch &patch = command.patches.emplace_back();
        patch.targetId = readUuid(values, {patchPrefix, ".targetId"});
        patch.scope = static_cast<CommandScope>(readNumber<int>(values, {patchPrefix, ".scope"}));
        patch.beforeState.storage = static_cast<CommandPayloadStorage>(
                readNumber<int>(values, {patchPrefix, ".before.storage"}));
        patch.beforeState.mimeType = readString(values, {patchPrefix, ".before.mimeType"});
        patch.beforeState.assetId = readUuid(values, {patchPrefix, ".before.assetId"});
        patch.beforeState.bytes = readArchiveVector<std::byte>(values, {patchPrefix, ".before.bytes"});
        patch.afterState.storage = static_cast<CommandPayloadStorage>(
                readNumber<int>(values, {patchPrefix, ".after.storage"}));
        patch.afterState.mimeType = readString(values, {patchPrefix, ".after.mimeType"});
        patch.afterState.assetId = readUuid(values, {patchPrefix, ".after.assetId"});
        patch.afterState.bytes = readArchiveVector<std::byte>(values, {patchPrefix, ".after.bytes"});
        patch.dirtyBounds = readRect<DocumentRect>(values, {patchPrefix, ".dirtyBounds"});
    }
}

void writeHistory(ArchiveOutput &output,
                  std::string_view prefix,
                  const HistoryStack &history)
{
    output.numbers({prefix, ".cursor"}, history.cursor);
    output.numbers({prefix, ".nextSequence"}, history.nextSequence);
    output.numbers({prefix, ".maxUndoCommands"}, history.maxUndoCommands);
    output.numbers({prefix, ".undo.count"}, history.undoCommands.size());
    for (std::size_t index = 0; index < history.undoCommands.size(); ++index) {
        writeCommand(output, indexedKey({prefix, ".undo"}, index), history.undoCommands[index]);
    }
    output.numbers({prefix, ".redo.count"}, history.redoCommands.size());
    for (std::size_t index = 0; index < history.redoCommands.size(); ++index) {
        writeCommand(output, indexedKey({prefix, ".redo"}, index), history.redoCommands[index]);
    }
}

// Undo와 Redo가 명령 및 패치 예산을 공유하며 명령은 각 벡터에서 직접 구성한다.
HistoryStack readHistory(const ArchiveFields &values,
                         std::string_view prefix,
                         DocumentArchiveReadLimits &remaining)
{
    const std::string undoPrefix = joinedKey({prefix, ".undo"});
    const std::string redoPrefix = joinedKey({prefix, ".redo"});
    const std::size_t undoCount = readCollectionCount(values, undoPrefix, remaining.maxHistoryCommands, ".sequence");
    const std::size_t redoCount = readCollectionCount(values, redoPrefix, remaining.maxHistoryCommands, ".sequence");
    HistoryStack history;
    history.cursor = readArchiveSize(values, {prefix, ".cursor"});
    history.nextSequence = readNumber<std::uint64_t>(values, {prefix, ".nextSequence"}, 1);
    history.maxUndoCommands = readArchiveSize(values, {prefix, ".maxUndoCommands"});
    std::uint64_t maxSequence = 0;
    const auto readCommands = [&](std::vector<Command> &commands, std::string_view commandPrefix, std::size_t count) {
        commands.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            Command &command = commands.emplace_back();
            readCommand(values, indexedKey({commandPrefix}, index), remaining.maxHistoryPatches, command);
            maxSequence = std::max(maxSequence, command.sequence);
        }
    };
    readCommands(history.undoCommands, undoPrefix, undoCount);
    readCommands(history.redoCommands, redoPrefix, redoCount);
    if (history.nextSequence <= maxSequence) {
        history.nextSequence = maxSequence + 1;
    }
    return history;
}

void writeColorSpace(ArchiveOutput &output,
                     std::string_view prefix,
                     const ColorSpace &colorSpace)
{
    output.quoted({prefix, ".name"}, colorSpace.name);
    output.numbers({prefix, ".primaries"}, static_cast<int>(colorSpace.primaries));
    output.numbers({prefix, ".transferFunction"}, static_cast<int>(colorSpace.transferFunction));
    output.numbers({prefix, ".componentEncoding"}, static_cast<int>(colorSpace.componentEncoding));
    output.binary({prefix, ".iccProfile"}, std::as_bytes(std::span(colorSpace.iccProfile)));
    output.line({prefix, ".linear"}, boolText(colorSpace.linear));
    output.line({prefix, ".hdr"}, boolText(colorSpace.hdr));
    output.numbers({prefix, ".minComponentValue"}, colorSpace.minComponentValue);
    output.numbers({prefix, ".maxComponentValue"}, colorSpace.maxComponentValue);
    output.numbers({prefix, ".referenceWhiteNits"}, colorSpace.referenceWhiteNits);
    writePoint(output, {prefix, ".redPrimary"}, colorSpace.redPrimary);
    writePoint(output, {prefix, ".greenPrimary"}, colorSpace.greenPrimary);
    writePoint(output, {prefix, ".bluePrimary"}, colorSpace.bluePrimary);
    writePoint(output, {prefix, ".whitePoint"}, colorSpace.whitePoint);
}

ColorSpace readColorSpace(const ArchiveFields &values,
                          std::string_view prefix)
{
    ColorSpace colorSpace;
    colorSpace.name = readString(values, {prefix, ".name"}, "sRGB");
    colorSpace.primaries = static_cast<ColorPrimaries>(readNumber<int>(values, {prefix, ".primaries"}));
    colorSpace.transferFunction = static_cast<ColorTransferFunction>(readNumber<int>(values, {prefix, ".transferFunction"}));
    colorSpace.componentEncoding = static_cast<ColorComponentEncoding>(readNumber<int>(values, {prefix, ".componentEncoding"}));
    colorSpace.iccProfile = readArchiveVector<std::byte>(values, {prefix, ".iccProfile"});
    colorSpace.linear = readBool(values, {prefix, ".linear"});
    colorSpace.hdr = readBool(values, {prefix, ".hdr"});
    colorSpace.minComponentValue = readNumber<Types::Scalar>(values, {prefix, ".minComponentValue"});
    colorSpace.maxComponentValue = readNumber<Types::Scalar>(values, {prefix, ".maxComponentValue"}, 1.0);
    colorSpace.referenceWhiteNits = readNumber<Types::Scalar>(values, {prefix, ".referenceWhiteNits"}, 80.0);
    colorSpace.redPrimary = readPoint<ColorChromaticity>(values, {prefix, ".redPrimary"});
    colorSpace.greenPrimary = readPoint<ColorChromaticity>(values, {prefix, ".greenPrimary"});
    colorSpace.bluePrimary = readPoint<ColorChromaticity>(values, {prefix, ".bluePrimary"});
    colorSpace.whitePoint = readPoint<ColorChromaticity>(values, {prefix, ".whitePoint"});
    return colorSpace;
}

void writeAsset(ArchiveOutput &output,
                std::string_view prefix,
                const DocumentAsset &asset)
{
    output.line({prefix, ".id"}, uuidText(asset.id));
    output.quoted({prefix, ".name"}, asset.name);
    output.quoted({prefix, ".mimeType"}, asset.mimeType);
    output.binary({prefix, ".bytes"}, std::as_bytes(std::span(asset.bytes)));
}

DocumentAsset readAsset(const ArchiveFields &values,
                        std::string_view prefix)
{
    DocumentAsset asset;
    asset.id = readUuid(values, {prefix, ".id"});
    asset.name = readString(values, {prefix, ".name"});
    asset.mimeType = readString(values, {prefix, ".mimeType"});
    asset.bytes = readArchiveVector<std::byte>(values, {prefix, ".bytes"});
    return asset;
}

// 측정, 메모리 출력과 스트림 출력이 동일한 필드 순서와 데이터 표현을 공유한다.
void writeDocumentArchive(ArchiveOutput &output, const DocumentArchive &archive)
{
    output.signature();
    output.quoted({"formatMagic"}, archive.formatMagic);
    writeDocumentMetadata(output, "document.metadata", archive.document.metadata);

    writeSurface(output, "document.surface", archive.document.surface);
    output.numbers({"document.layers.count"}, archive.document.layers.layers.size());
    for (std::size_t layerIndex = 0; layerIndex < archive.document.layers.layers.size(); ++layerIndex) {
        writeLayer(output,
                   indexedKey({"document.layers"}, layerIndex),
                   archive.document.layers.layers[layerIndex]);
    }
    output.numbers({"document.layers.activeLayerIndex"}, archive.document.layers.activeLayerIndex);

    output.numbers({"brushSources.count"}, archive.brushSources.size());
    for (std::size_t index = 0; index < archive.brushSources.size(); ++index) {
        writeBrushSnapshot(output, indexedKey({"brushSources"}, index), archive.brushSources[index]);
    }

    output.numbers({"colorSpaces.count"}, archive.colorSpaces.size());
    for (std::size_t index = 0; index < archive.colorSpaces.size(); ++index) {
        writeColorSpace(output, indexedKey({"colorSpaces"}, index), archive.colorSpaces[index]);
    }

    output.numbers({"assets.count"}, archive.assets.size());
    for (std::size_t index = 0; index < archive.assets.size(); ++index) {
        writeAsset(output, indexedKey({"assets"}, index), archive.assets[index]);
    }

    writeHistory(output, "history", archive.history);
    // 이전 줄 기반 리더도 청크 내부의 바이트보다 이 버전 레코드를 마지막으로 읽는다.
    output.numbers({"formatVersion"}, documentArchiveFormatVersion);
}

// 공통 예산으로 문서를 복원하며 실패하면 부분적으로 생성된 객체도 해제한다.
DocumentArchive readDocumentArchive(std::string_view payload, DocumentArchiveReadLimits remaining)
{
    const ArchiveFields values(payload);

    DocumentArchive archive;
    archive.formatMagic = readString(values, {"formatMagic"}, "iiPaintDocument");
    const std::uint32_t sourceFormatVersion = readNumber<std::uint32_t>(values, {"formatVersion"}, 1);
    archive.formatVersion = documentArchiveFormatVersion;
    if (sourceFormatVersion > documentArchiveFormatVersion) {
        throw std::invalid_argument("The document was created by a newer unsupported format version.");
    }
    if (values.usesBinaryFraming() != (sourceFormatVersion == documentArchiveFormatVersion)) {
        throw std::invalid_argument("Document archive version does not match its binary framing.");
    }
    archive.document.metadata = readDocumentMetadata(values, "document.metadata");

    std::string_view surfacePrefix = "document.surface";
    std::string_view layersPrefix = "document.layers";
    if (sourceFormatVersion < kFirstFlatArchiveVersion) {
        const std::size_t legacySurfaceCount = readArchiveSize(values, {"document.canvases.count"});
        if (legacySurfaceCount != 1) {
            throw std::invalid_argument("Only single-surface legacy bitmap documents can be migrated without data loss.");
        }
        surfacePrefix = "document.canvases.0.surface";
        layersPrefix = "document.canvases.0.layers";
        if (!values.contains({surfacePrefix, ".width"})) {
            throw std::invalid_argument("Missing archive field document.canvases.0.surface.width.");
        }
        readLegacySurfaceMetadata(values, "document.canvases.0.metadata", archive.document.metadata);
    }

    archive.document.surface = readSurface(values, surfacePrefix, remaining.maxDirtyRects);
    archive.document.layers.layers = readLayers(values, layersPrefix, remaining, 1);
    archive.document.layers.activeLayerIndex = readArchiveSize(values, {layersPrefix, ".activeLayerIndex"});

    const std::size_t brushCount = readCollectionCount(values, "brushSources", remaining.maxBrushSources,
                                                        ".brushId", ".presetV2");
    archive.brushSources.reserve(brushCount);
    for (std::size_t index = 0; index < brushCount; ++index) {
        archive.brushSources.push_back(readBrushSnapshot(values, indexedKey({"brushSources"}, index)));
    }

    const std::size_t colorSpaceCount = readCollectionCount(values, "colorSpaces", remaining.maxColorSpaces, ".name");
    archive.colorSpaces.reserve(colorSpaceCount);
    for (std::size_t index = 0; index < colorSpaceCount; ++index) {
        archive.colorSpaces.push_back(readColorSpace(values, indexedKey({"colorSpaces"}, index)));
    }

    const std::size_t assetCount = readCollectionCount(values, "assets", remaining.maxAssets, ".id");
    archive.assets.reserve(assetCount);
    for (std::size_t index = 0; index < assetCount; ++index) {
        archive.assets.push_back(readAsset(values, indexedKey({"assets"}, index)));
    }

    archive.history = readHistory(values, "history", remaining);
    return archive;
}

} // namespace

// lvalue 진입점은 기존 호출자의 문서를 복사한다.
DocumentArchive makeDocumentArchive(const PaintDocument &document)
{
    DocumentArchive archive;
    archive.document = document;
    return archive;
}

// rvalue 진입점은 문서가 이미 소유한 픽셀 벡터를 그대로 이전한다.
DocumentArchive makeDocumentArchive(PaintDocument &&document)
{
    DocumentArchive archive;
    archive.document = std::move(document);
    return archive;
}

// 최종 크기를 먼저 예약하여 큰 청크를 쓰는 동안 문자열 버퍼가 교체되지 않게 한다.
std::string serializeDocumentArchive(const DocumentArchive &archive)
{
    if (!archive.compatible) return {};
    const std::locale locale;
    ArchiveOutput sizing(locale);
    writeDocumentArchive(sizing, archive);

    std::string payload;
    payload.reserve(sizing.measuredSize());
    ArchiveOutput output(locale, &payload);
    writeDocumentArchive(output, archive);
    return payload;
}

// 호출자 스트림에 직접 기록하고 짧은 쓰기나 스트림 오류를 전달한다.
bool serializeDocumentArchiveTo(const DocumentArchive &archive, std::ostream &stream)
{
    if (!archive.compatible || !stream) return false;
    try {
        ArchiveOutput output(std::locale{}, nullptr, &stream);
        writeDocumentArchive(output, archive);
        return true;
    } catch (const std::ios_base::failure &) {
        return false;
    }
}

// 기존 문자열 API도 복사 없는 뷰 경로를 사용한다.
DocumentArchive deserializeDocumentArchive(const std::string &payload)
{
    return deserializeDocumentArchiveView(payload);
}

DocumentArchive deserializeDocumentArchiveWithLimits(const std::string &payload,
                                                     DocumentArchiveReadLimits limits)
{
    return deserializeDocumentArchiveView(payload, limits);
}

// 호출자 한도를 적용하고 실패는 호환 불가 상태로 매핑한다.
DocumentArchive deserializeDocumentArchiveView(std::string_view payload, DocumentArchiveReadLimits limits)
{
    constexpr DocumentArchiveReadLimits hardLimits;
    limits.maxLayers = std::min(limits.maxLayers, hardLimits.maxLayers);
    limits.maxLayerDepth = std::min(limits.maxLayerDepth, hardLimits.maxLayerDepth);
    limits.maxDirtyRects = std::min(limits.maxDirtyRects, hardLimits.maxDirtyRects);
    limits.maxHistoryCommands = std::min(limits.maxHistoryCommands, hardLimits.maxHistoryCommands);
    limits.maxHistoryPatches = std::min(limits.maxHistoryPatches, hardLimits.maxHistoryPatches);
    limits.maxBrushSources = std::min(limits.maxBrushSources, hardLimits.maxBrushSources);
    limits.maxColorSpaces = std::min(limits.maxColorSpaces, hardLimits.maxColorSpaces);
    limits.maxAssets = std::min(limits.maxAssets, hardLimits.maxAssets);

    // Prepare the failure result before parsing so an allocation failure needs no new diagnostic storage.
    DocumentArchive rejected;
    rejected.compatible = false;
    rejected.compatibilityError = "Out of memory.";
    try {
        try {
            return readDocumentArchive(payload, limits);
        } catch (const std::invalid_argument &error) {
            rejected.compatibilityError = error.what();
        } catch (const std::length_error &) {
            rejected.compatibilityError = "Document archive exceeds container capacity.";
        }
    } catch (const std::bad_alloc &) {
        // If allocating a more specific diagnostic failed, the prebuilt failure is still valid.
    }
    return rejected;
}
