# iiPaintEngine 개발 계약

## 1. 제품 정의

iiPaintEngine은 C++/Qt/QML 애플리케이션에 포함하는 순수 비트맵 파일 페인팅 엔진이다. 열린 파일, 레이어, 저장 형식, 실행 중 미리보기, undo/redo의 기준 데이터는 모두 픽셀이다.

이 계약에서 비트맵 전용이라는 표현은 다음을 뜻한다.

- 파일에 기록되는 모든 결과는 `BitmapFile`의 `RasterLayer` 또는 `DrawingSurface` 픽셀이다.
- 입력 궤적은 다시 편집하거나 재생할 수 있는 경로 객체로 저장하지 않는다.
- 그리기 도중에도 전체 좌표열, 곡선, 경로, 명령형 스트로크 모델을 만들지 않는다.
- 텍스트, SVG, 도형 같은 외부 콘텐츠는 iiPaintEngine 경계에 들어오기 전에 비트맵으로 래스터화한다.
- 벡터·텍스트·스트로크 전용 레이어를 추가하지 않는다.

호환성, 미리보기, undo, 직렬화, 향후 확장을 이유로 이 규칙을 우회해서는 안 된다.

## 2. 유일한 그리기 파이프라인

입력은 다음 순서로 즉시 픽셀화한다.

```text
PointerEvent
→ InputStrokeBuilder가 현재 StrokePoint 하나를 방출
→ RasterDabStream이 직전 점과 현재 점 사이의 간격만 계산
→ BrushDab 비트맵 스탬프
→ RasterSample 픽셀
→ StrokeCompositeBuffer 픽셀 누적
→ RasterLayer / DrawingSurface
```

`InputStrokeBuilder`는 활성 여부만 가진다. `RasterDabStream`은 직전 점 하나, 이동 거리, 다음 dab 거리, 난수 시퀀스만 가진다. 두 타입 모두 점 목록을 소유해서는 안 된다.

`BrushDab`은 벡터 도형이 아니라 한 번 투영하고 폐기하는 비트맵 브러시 스탬프이다. `BitmapFileItem`은 이벤트마다 생성된 dab을 즉시 `RasterSample`로 투영하고
`BitmapFile`의 픽셀 변경 API에 누적한다. release 전까지 유지할 수 있는 그림 데이터는 픽셀 버퍼뿐이다.

미래 좌표나 전체 길이가 필요한 후처리는 허용하지 않는다. 따라서 종료점 기준 taper, 전체 궤적 smoothing, 곡선 보간, 원본 입력 replay를 구현하지 않는다. 시작점부터 누적 거리만으로 계산 가능한
warm-up 효과는 허용한다.

## 3. 문서와 레이어

소유 구조는 다음과 같다.

```text
PaintDocument
→ LayerStack
→ Layer
→ DrawingSurface
→ ARGB 픽셀
```

`BitmapFile`은 파일 경로, 실제 바이트에서 감지한 형식, 수정 상태, 직접 쓰는 ARGB 픽셀을 소유한다. `BitmapFileItem`은 화면 표시와 입력 좌표 변환만 담당하고 픽셀을 소유하지 않는다.
`PaintDocument::surface`는 최종 합성 표면이며 `Layer::surface`는 각 페인트 레이어의 픽셀 표면이다. `Layer`의 children, mask, opacity, blend mode는
비트맵 합성 속성이다.
레이어 메타데이터가 콘텐츠 종류를 설명하더라도 실제 콘텐츠는 항상 픽셀이어야 한다.

문서 직렬화 형식은 레이어 픽셀, 레이어 메타데이터, 마스크, 브러시 preset, 색공간, asset, raster history만 저장한다. 포인터 좌표, 곡선, dab 열, 재생 가능한 그리기 명령은 저장하지
않는다.

undo/redo는 전체 비트맵 파일 스냅샷 또는 dirty rect 픽셀 patch로 구현한다. 그리기를 다시 실행해 이전 상태를 복원하지 않는다.

## 4. 미리보기와 입력 처리

live preview도 별도 표현이 아니라 pending 픽셀 버퍼를 화면에 합성한 결과이다. release 시 같은 픽셀 버퍼를 committed raster layer로 합성한다. 지우개는 pending
alpha mask를 destination-out 방식으로 적용한다.

전체 입력을 worker queue에 복사하거나 frame tick까지 모으지 않는다. 각 press/move/release 이벤트는 동기적으로 현재 점을 픽셀화한다. 무거운 계산을 분리해야 할 때도 좌표열을 넘기지
말고 이미 생성된 비트맵 tile 또는 pixel patch만 넘긴다.

viewport 변경, 파일 교체, clear, undo/redo 중 활성 입력이 있으면 pending 픽셀을 폐기하고 최소 상태를 초기화한다. view item resize는 열린 파일의 픽셀 크기를 바꾸지
않는다.

## 5. 계층과 의존성

의존 방향은 다음을 지킨다.

```text
QtAdapter
→ BitmapFile / Input / Document / Transform
→ Layer / Brush / Render / History
→ Core
```

- Core는 Qt와 상위 모듈을 알지 않는다.
- Brush와 Render는 Document나 QML을 알지 않는다.
- Input은 pointer event를 현재 `StrokePoint` 하나로 바꾸는 일만 한다.
- BitmapFile은 경로·감지 형식·ARGB 픽셀·수정 상태와 codec 입출력을 소유한다.
- Qt의 QObject와 QQuickItem은 QtAdapter 경계에서만 사용한다.
- 하위 모듈이 상위 모듈을 참조하거나 순환 의존성을 만들지 않는다.

## 6. 변경 규칙

- 기능 변경은 테스트와 문서를 함께 갱신한다.
- 빌드 디렉터리는 항상 `build/`를 사용한다.
- 소스 변경 뒤 전체 빌드와 `ctest --test-dir build --output-on-failure`를 실행한다.
- 구조 변경 전후에 금지 타입·파일 이름을 정적 검색한다.
- 외부 라이브러리는 유지보수 상태, 라이선스, 의존성 규모를 평가한 뒤 도입한다.
- 미래 기능을 예상한 경로 추상화나 재생 모델을 추가하지 않는다.
- 레이어와 저장 형식의 기본값은 언제나 bitmap-only이다.

## 7. 필수 계약 테스트

`iiPaintEngineBitmapOnlyArchitectureContract`는 다음을 영구히 고정한다.

- `PaintDocument`에 retained stroke 저장소가 없다.
- `InputStrokeBuilder`와 결과 타입에 좌표 collection이 없다.
- 벡터·경로·원본 stroke 모델의 소스 파일이 존재하지 않는다.
- press/move/release가 각 이벤트에서 즉시 raster pixel buffer로 누적된다.

`iiPaintEngineBitmapFileArchitectureContract`는 구형 화면 표면 API가 없고 `BitmapFile`이 경로·형식·픽셀을 직접 소유하는지 검사한다.
`iiPaintEngineBitmapFileCompatibilityContract`는 설치된 래스터 코덱만 노출하고 SVG·PDF를 디코더에 전달하지 않으며 content sniffing과 주요 비트맵 형식 왕복을
검사한다.
`iiPaintEngineInstallUpgradeContract`는 업그레이드 설치가 package 소유 공개 헤더 트리를 교체해 삭제된 API 헤더를 남기지 않는지 검사하고, 별도 CMake
소비자가 설치 package를 `find_package`해 link·load할 수 있는지 검증한다.

`iiPaintEngineDocumentSerializerContract`는 저장 payload에 raw input이나 vector curve가 없고 format version 3이 픽셀 문서를 왕복하며
version 2 단일 표면 문서를 안전하게 이관하는지 검사한다. 손실 없이 이관할 수 없는 레거시 다중 표면과 미래 버전은 fail-closed한다. `iiPaintEnginePipelineHeartbeat`,
`iiPaintEnginePointerStrokeFlow`, `iiPaintEngineBitmapFileLivePreviewRealtimeContract`는 각각 core, input, QML bitmap file
경로가 같은 비트맵 파이프라인을 사용하는지 검사한다.
