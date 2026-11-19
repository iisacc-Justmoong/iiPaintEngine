#include <iiBrushTools/Gesture.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <vector>

using namespace ii::brush;

namespace {
struct AllocationProbe;
thread_local AllocationProbe* activeProbe = nullptr;

// 호출 중 살아 있는 할당만 센다. 추적용 버퍼는 스택에 둔다.
struct AllocationProbe {
    AllocationProbe() { activeProbe = this; }
    ~AllocationProbe() { activeProbe = nullptr; }
    AllocationProbe(const AllocationProbe&) = delete;
    AllocationProbe& operator=(const AllocationProbe&) = delete;

    void add(void* pointer, std::size_t bytes) noexcept
    {
        if (count == allocations.size()) {
            exhausted = true;
            return;
        }
        allocations[count++] = {pointer, bytes};
        live += bytes;
        peak = std::max(peak, live);
    }

    void remove(void* pointer) noexcept
    {
        for (std::size_t i = 0; i < count; ++i) {
            if (allocations[i].pointer != pointer) continue;
            live -= allocations[i].bytes;
            allocations[i] = allocations[--count];
            return;
        }
    }

    struct Entry { void* pointer; std::size_t bytes; };
    std::array<Entry, 32> allocations{};
    std::size_t count = 0, live = 0, peak = 0;
    bool exhausted = false;
};
} // namespace

void* operator new(std::size_t size)
{
    if (auto* pointer = std::malloc(std::max<std::size_t>(size, 1))) {
        if (activeProbe) activeProbe->add(pointer, size);
        return pointer;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept
{
    if (activeProbe) activeProbe->remove(pointer);
    std::free(pointer);
}
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

namespace {
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

StrokePoint sample(DocumentPoint position, std::size_t index)
{
    return StrokePoint{.position = position, .pressure = .4 + .1 * (index % 5),
                       .time = static_cast<double>(index) / 64};
}

// 기준 구현은 모든 선분을 직접 적분한다. 모멘트를 현재 점 기준으로 계산해 큰 좌표의 상쇄를 줄인다.
DocumentPoint integratedPosition(const PreparedGesture& gesture, std::size_t i, double radius)
{
    const auto original = gesture.points[i].position;
    const auto s = gesture.distances[i];
    const auto h = std::min({radius, s, gesture.length - s});
    if (h <= 1e-9) return original;

    long double length = 0, x = 0, y = 0;
    for (std::size_t j = 1; j < gesture.points.size(); ++j) {
        const auto start = gesture.distances[j - 1];
        const auto end = gesture.distances[j];
        const auto left = std::max(start, s - h);
        const auto right = std::min(end, s + h);
        if (right <= left) continue;

        const auto a = gesture.points[j - 1].position;
        const auto b = gesture.points[j].position;
        const auto weight = static_cast<long double>(right - left);
        const auto t = (static_cast<long double>(left - start) + (right - start)) / (2 * (end - start));
        length += weight;
        x += weight * ((static_cast<long double>(a.x) - original.x) + (static_cast<long double>(b.x) - a.x) * t);
        y += weight * ((static_cast<long double>(a.y) - original.y) + (static_cast<long double>(b.y) - a.y) * t);
    }
    if (length <= 0) return original;

    const DocumentPoint center{static_cast<double>(original.x + x / length),
                               static_cast<double>(original.y + y / length)};
    const auto dx = center.x - original.x, dy = center.y - original.y;
    const auto distance = std::hypot(dx, dy);
    if (distance > h) return {original.x + dx * h / distance, original.y + dy * h / distance};
    return center;
}

// 같은 입력을 안정화만 적용한 뒤 선분 적분과 좌표·누적 거리를 비교한다.
void checkGeometry(std::span<const StrokePoint> inputs, double stabilization, double radius,
                   std::span<const DocumentPoint> legacy = {})
{
    Controls controls;
    check(controls.set(Parameter::Stabilization, stabilization) && controls.set(Parameter::PostCorrection, 0),
          "reference control setup failed");
    const auto original = prepareGesture(inputs, controls);
    check(original.has_value() && controls.set(Parameter::PostCorrection, radius), "reference preparation failed");
    const auto actual = prepareGesture(inputs, controls);
    check(actual && actual->points.size() == inputs.size() && actual->distances.size() == inputs.size(),
          "correction did not retain the point count");

    DocumentPoint previous{};
    double length = 0, toleranceSum = 0, maxError = 0;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        const auto expected = integratedPosition(*original, i, radius);
        const auto actualPoint = actual->points[i].position;
        const auto tolerance = 1e-10 + 8 * std::numeric_limits<double>::epsilon()
            * std::max({1., std::abs(expected.x), std::abs(expected.y)});
        const auto error = std::hypot(expected.x - actualPoint.x, expected.y - actualPoint.y);
        if (i != 0) {
            length += std::hypot(expected.x - previous.x, expected.y - previous.y);
            toleranceSum += 2 * tolerance;
        }
        previous = expected;
        maxError = std::max(maxError, error);
        check(error <= tolerance && std::abs(actual->distances[i] - length) <= toleranceSum,
              "correction differs from direct segment integration");
        if (!legacy.empty()) {
            check(std::hypot(legacy[i].x - actualPoint.x, legacy[i].y - actualPoint.y) <= 1e-10,
                  "correction differs from the original tree golden");
        }
    }
    check(actual->length == actual->distances.back(), "correction length is not the final distance");
    std::cout << "max_reference_error=" << maxError << '\n';
}

// 비균일 간격, 정지점, 큰 누적 거리 뒤의 작은 보정 구간을 포함한다.
void geometry()
{
    const std::array corners{
        DocumentPoint{0, 0}, DocumentPoint{3, 0}, DocumentPoint{3, 0}, DocumentPoint{3, 4},
        DocumentPoint{7, 4}, DocumentPoint{6, 8}, DocumentPoint{9, 10}, DocumentPoint{9, 10},
        DocumentPoint{12, 7}, DocumentPoint{12, 7}, DocumentPoint{15, 12},
    };
    // 리프까지 저장하던 4n 노드 구현에서 얻은 동일 획의 보정 좌표이다.
    const std::array<DocumentPoint, corners.size()> legacy{{
        {0, 0}, {2.3125, .6875}, {2.3125, .6875}, {3.6875, 3.3125},
        {6.1457567577875212, 4.6669729688499153}, {6.7387778195697461, 7.7143834160549298},
        {8.9141013347084836, 9.132507703029404}, {8.9141013347084836, 9.132507703029404},
        {11.867579919790673, 8.0756622984931248}, {11.867579919790673, 8.0756622984931248}, {15, 12},
    }};
    std::vector<StrokePoint> inputs;
    for (auto p : corners) inputs.push_back(sample(p, inputs.size()));
    checkGeometry(inputs, 0, 2.75, legacy);
    checkGeometry(inputs, 62, 20);

    inputs.clear();
    constexpr double base = 999'999'600.;
    for (std::size_t i = 0; i < 2'048; ++i) {
        inputs.push_back(sample({i % 2 == 0 ? 0. : base, -base}, inputs.size()));
    }
    for (std::size_t i = 0; i < 129; ++i) {
        const auto step = static_cast<double>(i / 3);
        inputs.push_back(sample({base + step / 64, -base + static_cast<double>((i / 3) % 7) / 32}, inputs.size()));
    }
    checkGeometry(inputs, 0, .03125);
}

// 호출 전의 입력을 제외하고 반환 데이터와 동시에 살아 있던 보정 버퍼의 상한을 확인한다.
void memoryBudget()
{
    const Limits limits;
    const auto count = limits.maxInputPoints;
    std::vector<StrokePoint> inputs;
    inputs.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        inputs.push_back(sample({static_cast<double>(i) / 8, static_cast<double>(i % 33) / 16}, i));
    }
    Controls controls;
    check(controls.set(Parameter::Stabilization, 0) && controls.set(Parameter::PostCorrection, 20),
          "memory control setup failed");

    std::size_t peak = 0, retained = 0;
    bool complete = false;
    {
        AllocationProbe probe;
        const auto result = prepareGesture(inputs, controls, limits);
        complete = result && result->points.size() == count && result->distances.size() == count && !probe.exhausted;
        peak = probe.peak;
        retained = probe.live;
    }
    const auto outputBytes = count * (sizeof(StrokePoint) + sizeof(double));
    constexpr auto scratchBytesPerPoint = 2 * sizeof(DocumentPoint) + sizeof(double);
    check(complete && retained == outputBytes && peak >= retained, "allocation probe missed gesture buffers");
    const auto scratch = peak - retained;
    std::cout << "points=" << count << " retained_bytes=" << retained << " peak_bytes=" << peak
              << " scratch_bytes=" << scratch << '\n';
    check(scratch <= count * scratchBytesPerPoint, "gesture correction exceeded the scratch memory budget");
}
} // namespace

int main()
{
    try {
        geometry();
        memoryBudget();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
