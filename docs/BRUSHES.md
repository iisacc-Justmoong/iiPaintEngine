# 고급 비트맵 브러시

`BrushPreset`은 팁, 모양, 간격, 색상, 입력 반응, 재질을 소유하는 편집 가능한 값 객체이다. `resolveBrushPreset()`은 설정을 검증하고 `BrushState` 실행 스냅샷을 만든다. 실패한 결과에는 `brush`가 없으며 `errors`에 원인이 기록된다. Brush 모듈은 Qt와 Stroke 모듈을 참조하지 않는다.

```cpp
#include <iiPaintEngine>

BrushPreset preset = builtInBrushPresets().front();
preset.size = 24;
preset.shape.angleMode = BrushAngleMode::StylusRotation;
preset.shape.roundness = 0.35;

BrushDynamicsBinding pressure;
pressure.source = BrushDynamicsSource::Pressure;
pressure.target = BrushDynamicsTarget::Flow;
pressure.curve.points = {{0, 0.05}, {0.3, 0.2}, {0.75, 0.8}, {1, 1}};
preset.dynamics.bindings.push_back(pressure);

auto resolved = resolveBrushPreset(preset, 0xFF336699U, 1234);
if (!resolved.brush) { /* report resolved.errors */ return; }
RasterDabStream stream;
StrokePoint point{{100, 80}, 0.6, 0.0};
point.rotationRadians = 0.4;
auto dabs = appendRasterDabs(stream, point, *resolved.brush);
auto pixels = projectBrushDabs(dabs, resolved.brush->rasterizer, {}, resolved.brush->material);
// 픽셀을 RasterLayer / BitmapFile에 즉시 적용한다. 붓 자국을 작품 데이터로 보관하지 않는다.
```

## 설정과 단위

| 영역 | 지원 설정 |
|---|---|
| 모양 | 원·사각·마름모의 절차적 팁, 알파 마스크 팁, 두께 비율, X/Y 반전 |
| 방향 | 기존 자동 모드, 고정, 진행 방향, 기울기 방위, 펜 축 회전 + 고정 각도 |
| 다중 팁 | 최대 256개 마스크의 순환·시드 난수·압력 구간 선택; 각 팁의 긴 변을 지정 크기에 맞춤 |
| 획 | 절대 간격 또는 크기 대비 간격, 압력 크기를 따르는 간격, flow, opacity cap, hardness, density, 시작 거리 taper |
| 산포 | 스탬프 개수와 개수 변동, 양축·진행 방향·수직 방향, 사각·균일 원판 분포, 절대 거리 또는 브러시 크기 기준 반경 |
| 색상 | 전경/보조색 혼합, HSV 변화, 스탬프마다 또는 획마다 고정되는 색상 난수 |
| 재질 | 기존 팁·진행 방향·문서·종이 좌표 텍스처, 이중 마스크, WetPaint/Smudge/Mixer, bristle 설정 |
| 에어브러시 | 정지 상태에서도 초당 지정 횟수로 분사; 시간 기반 배치가 거리 기반 배치를 대체 |

좌표·크기·거리·속도는 문서 픽셀, 초, 문서 픽셀/초 단위이다. 각도는 라디안, 압력은 `[0,1]`, 기울기 X/Y와 tangential pressure는 `[-1,1]`이다. `BrushStrokeSettings::spacing > 0`이면 절대 간격이 `spacingRatio`보다 우선한다. 크기 상한은 4096이다. `shape.roundness`는 `[0.01,1]`이다.

`opacity`는 한 획의 누적 불투명도 상한이며 `flow`는 각 스탬프의 잉크량이다. 절차적 팁의 hardness는 중심의 불투명 영역과 가장자리 감쇠 폭을 바꾸며, 이미지 마스크의 hardness는 기존 알파 응답을 조절한다. 팁은 알파 마스크이다. 다색 이미지 스탬프는 지원하지 않는다.

크기와 동적 응답을 모두 곱한 최종 산포 반경은 문서 픽셀 1,000,000 이내로 제한한다. 개별 설정이 유효하더라도 곱한 결과가 정수 픽셀 좌표 범위를 넘지 않도록 하기 위함이다.

## 입력을 속성에 연결

기존 `sizeResponse`, `flowResponse` 등의 동작을 먼저 평가한 뒤 `dynamics.bindings`를 배열 순서대로 적용한다. 각 연결은 입력 범위 정규화, 반응 곡선, `Multiply`·`Add`·`Replace` 결합 방식을 가진다. 각도·색조처럼 기본값이 0인 속성을 직접 제어할 때는 `Add` 또는 `Replace`를 사용한다.

입력은 Pressure, Velocity, Tilt, TiltX, TiltY, Direction, Rotation, TangentialPressure, Distance, Time, Random, StrokeRandom, Custom이다. `Custom`은 `StrokePoint::custom[0..7]`에 전달하는 앱 정의 스칼라이다. 센서가 지원하지 않는 펜 회전·tangential pressure는 0으로 전달된다.

출력은 Size, Flow, Opacity, Hardness, Spacing, Scatter, Rotation, Roundness, TextureDepth, Wetness, DryOut, BristleSpread, ScatterCount, Hue, Saturation, Value, ColorMix이다. Hue는 한 바퀴를 1로 표현한다. 색상 출력은 `color.enabled`가 켜졌을 때 적용된다. Flow 등 scale 출력은 기반 설정에 곱해진다. ScatterCount는 기반 개수에 곱한 뒤 반올림하며 0–256으로 제한한다.

`inputMinimum`과 `inputMaximum`은 원래 입력의 단위로 지정한다. 예를 들어 `Velocity`의 0–1200, `Time`의 0–2, `Rotation`의 0–6.283을 각각 곡선의 0–1로 정규화할 수 있다. 범위 밖 입력은 끝값에 고정한다. Time은 획 시작 후 경과 시간, Distance는 시작 후 누적 거리이다.

곡선은 최대 64개의 `(input, output)` 매듭을 받으며 input은 `[0,1]`에서 엄격하게 증가해야 한다. 각 구간에 Linear/EaseIn/EaseOut/EaseInOut을 적용한다. 매듭이 비어 있으면 기존 min/center/max 세 점을 사용한다. 입력 궤적을 보존하는 곡선이 아니라 스칼라 파라미터의 전달 함수이다.

최대 64개 연결을 사용한다. `pressureInputEnabled`, `velocityInputEnabled`, `tiltInputEnabled`, `randomInputEnabled`는 해당 연결에도 적용된다. 시드와 동일한 입력·설정은 동일한 결과를 낸다. 한 획에서 센서 이벤트의 순서나 시각을 변경하면 속도·시간 반응이 달라질 수 있다.

## 시간 분사와 화면 연결

core 호출자는 단조 증가하는 초 단위 time을 전달한다. 정지 분사는 같은 위치와 센서 상태를 새 시각으로 다시 전달한다. 한 이벤트에서 최대 65,536개 스탬프 또는 배치까지 허용한다. 비유한 입력, 역행 시간, 과도한 이벤트는 예외로 거부하고 기존 stream 상태를 보존한다. 생성된 팁의 기본 지름은 4096으로 제한하며 극단적인 크기·고밀도 설정은 픽셀 투영 비용이 크다.

`BitmapFileItem::setBrushPreset()`은 C++ 객체를, QML의 `setBrushPresetData(payload)`는 직렬화된 프리셋을 적용한다. 실패하면 false와 `lastBrushError`를 반환하며 기존 설정을 유지한다. `brushPresetData`는 기본 size/flow/opacity/hardness/spacing 속성에서 수정한 값도 포함한다. `resetBrushPreset()`은 기존 기본 브러시로 돌아간다. 색상은 `brushColor`로 지정한다.

활성 획은 press 시점의 브러시 설정 스냅샷을 사용한다. 도중에 선택한 프리셋은 다음 획에 적용된다. 화면 어댑터는 에어브러시에 한해 단조 시계와 16ms 타이머로 같은 위치를 다시 분사한다. release·cancel·파일 교체·뷰 변경·undo 때 타이머와 미완성 픽셀을 정리한다. 타이머는 좌표를 모으거나 미래 위치를 예측하지 않는다.

`builtInBrushPresets()`는 Pressure Ink, Grain Pencil, Soft Airbrush, Flat Marker, Color Spray, Wet Flat Mixer의 독립적으로 편집 가능한 예제를 반환한다. 목록의 객체를 수정해 다른 브러시를 만들 수 있다.

## 저장과 호환성

프리셋 출력 버전은 2이다. `readBrushPreset()`은 버전 1과 2를 읽고 오류 목록을 반환한다. `deserializeBrushPreset()`은 잘못된 입력에 `std::invalid_argument`를 던진다. 미래 버전, 잘못된 enum·숫자·마스크·곡선, 중복 키, 과도한 배열/페이로드를 거부한다. 마스크 전체는 16 MiB, 직렬화 입력은 40 MiB로 제한한다. 문자열의 개행·탭·역슬래시를 보존한다.

`snapshotBrushPreset()`/`restoreBrushPreset()`는 모든 설정을 복사한다. 문서 버전 3의 `brushSources`에는 완전한 버전 2 프리셋을 `presetV2`로 넣고 기존 필드도 함께 기록한다. 새 엔진은 `presetV2`를 우선하고 잘못된 프리셋이면 문서를 incompatible로 반환한다. 기존 문서는 기존 필드로 읽힌다. 이전 엔진은 픽셀과 기존 브러시 필드는 읽을 수 있지만 새 설정을 재저장할 수 없으므로 새 설정이 있는 문서의 편집에는 새 엔진을 사용해야 한다.

설계 범위는 [Adobe의 브러시 동적 속성](https://helpx.adobe.com/ie/photoshop/using/adding-dynamic-elements-brushes.html)과 [CLIP STUDIO의 다중 팁 설정](https://tips.clip-studio.com/en-us/articles/678)을 참고했다. 두 제품의 완전한 재현이나 ABR/SUT 호환성을 뜻하지 않는다. 기존 습식 혼색은 픽셀 샘플링 모델이며 유체 시뮬레이션이 아니다. 원본 궤적 저장, 종료점 taper, 전체 경로 smoothing, 리본 벡터는 bitmap-only 계약에 포함되지 않는다.

## 실행 가능한 견본

`Example/BrushGallery.cpp`는 각 입력점을 즉시 픽셀화하며 여섯 프리셋을 한 PNG로 출력한다. 소스 패키지 0.2.0은 C++23으로 빌드하며 공개 헤더의 C++17 소비자 호환성은 별도로 검사한다. 구조체에 필드가 추가되었으므로 SDK를 교체할 때 소비자도 재빌드해야 한다.

```sh
QT_QPA_PLATFORM=offscreen build/iiPaintEngineBrushGallery build/brush-gallery.png
```
