iiPaintEngine Development Contract

1. 목적

iiPaintEngine은 단순 Vincent 전용 렌더링 모듈이 아니라, 장기적으로 Vincent Pro를 포함한 고급 그래픽 애플리케이션을 구축하기 위한 독립 드로잉 엔진 라이브러리이다.

본 엔진은 다음 목표를 가진다.

* 고품질 드로잉 경험
* 낮은 입력 지연
* 확장 가능한 문서 구조
* GPU 친화적 렌더링 구조
* 비파괴 편집 가능성
* 장기 유지보수성
* Qt/QML 애플리케이션 통합
* 플랫폼 독립적 코어 설계

iiPaintEngine의 구조는 “작은 앱을 빠르게 만드는 구조”가 아니라 “오랫동안 확장 가능한 구조”를 우선한다.

2. 핵심 설계 원칙

2.1 엔진은 Vincent Basic이 아니라 Vincent Pro를 기준으로 설계한다

기본판의 단순함 때문에 엔진 구조를 축소하지 않는다.

기본판은 엔진 기능의 일부만 사용하는 제품이며, 엔진 자체는 장기적으로 다음 기능을 수용 가능해야 한다.

* 라이브 스트로크
* 비파괴 레이어
* GPU 렌더링
* 고급 브러시 다이내믹스
* 벡터 레이어
* 텍스트 레이어
* 애니메이션
* 리플레이
* 협업
* 타임라인
* 문서 스냅샷

3. 계층 구조 계약

3.1 의존 방향

다음 의존 방향을 절대 위반하지 않는다.

QtAdapter
→ Canvas/Input
→ Document
→ Layer/Stroke/Brush/Render/History
→ Core

하위 계층은 상위 계층을 알 수 없다.

예시:

* Stroke는 QML을 몰라야 한다
* Brush는 Document를 몰라야 한다
* Rasterizer는 UI를 몰라야 한다
* Core는 Qt Quick를 몰라야 한다

4. QObject 사용 규칙

4.1 QObject는 UI 경계에서만 사용한다

QObject는 다음 경우에만 사용한다.

* QML 노출
* signal/slot
* property binding
* Qt object tree
* UI 이벤트 전달

4.2 엔진 코어는 순수 C++ 타입을 사용한다

다음 객체들은 QObject를 상속하지 않는다.

* StrokePoint
* Stroke
* BrushSnapshot
* PaintPoint
* PaintRect
* LayerData
* Raster data
* Geometry types

이들은 가능한 POD-like 구조를 유지한다.

5. 브러시 시스템 계약

5.1 브러시는 “도형”이 아니라 “동적 시스템”이다

브러시는 다음 요소를 가진다.

* shape
* dynamics
* spacing
* opacity
* flow
* hardness
* pressure mapping
* velocity mapping
* tilt mapping

5.2 손맛은 Basic과 Pro 사이에서 동일해야 한다

다음 파이프라인은 Basic과 Pro가 공유한다.

StrokeInput
→ Stabilizer
→ StrokeCurve
→ Rasterizer

손맛은 제품 등급에 따라 차별화하지 않는다.

6. 입력 처리 계약

6.1 입력 원본은 보존한다

원본 입력 데이터는 가능한 손실 없이 저장한다.

struct StrokePoint {
float x;
float y;
float pressure;
float tiltX;
float tiltY;
double time;
};

속도, 곡률 등의 값은 파생값으로 계산한다.

6.2 입력 보정은 비파괴적으로 수행한다

Stabilizer는 원본 입력을 제거하지 않는다.

원본 스트로크 재생 가능성을 유지한다.

7. 좌표계 계약

모든 좌표는 명확한 공간을 가져야 한다.

View Space
Canvas Space
Document Space
Device Pixel Space

좌표계를 암묵적으로 변환하지 않는다.

8. 렌더링 계약

8.1 엔진은 렌더러 구현과 분리된다

엔진은 다음 렌더러를 교체 가능해야 한다.

* CPU Raster
* OpenGL
* Vulkan
* Metal

8.2 Dirty Region 기반 갱신을 우선한다

전체 캔버스 재렌더링을 기본 동작으로 삼지 않는다.

9. 레이어 시스템 계약

레이어는 공통 메타데이터를 가진다.

Layer
├── id
├── name
├── visible
├── opacity
└── blendMode

실제 데이터는 파생 레이어가 보유한다.

RasterLayer
StrokeLayer
VectorLayer
TextLayer

10. History 시스템 계약

Undo/Redo는 엔진 레벨 기능이다.

앱 레벨에서 직접 구현하지 않는다.

History는 다음을 지원 가능해야 한다.

* command-based undo
* snapshot-based undo
* hybrid undo

11. 문서 시스템 계약

PaintDocument는 단순 bitmap 컨테이너가 아니다.

문서는 다음을 포함한다.

LayerStack
HistoryStack
Metadata
CanvasInfo
ColorProfile
Assets

12. 성능 계약

12.1 입력 지연 최소화

입력 경로에서:

* 동적 할당 최소화
* 불필요한 복사 금지
* QObject 사용 최소화

12.2 구조적 성능 우선

미세 최적화보다 구조적 병목 제거를 우선한다.

13. 파일 구조 계약

새 객체는 반드시 명확한 책임 영역에 배치한다.

예시:

* Brush 관련 → Brush/
* Stroke 처리 → Stroke/
* 문서 저장 → Document/
* 렌더링 → Render/

“기타 유틸리티” 폴더를 만들지 않는다.

14. API 설계 계약

14.1 공개 API는 안정성을 우선한다

내부 구현보다 API 변경 비용을 더 중요하게 고려한다.

14.2 엔진은 앱 정책을 알지 않는다

엔진은:

* 무료판 제한
* 라이선스 정책
* UI 제한

등을 알지 않는다.

이는 Vincent 애플리케이션 계층에서 처리한다.

15. 구현 우선순위

초기 구현 우선순위는 다음과 같다.

Core
→ Stroke pipeline
→ RasterLayer
→ PaintDocument
→ Render
→ History
→ QtAdapter
→ Advanced systems

고급 기능보다 “한 획이 자연스럽게 그려지는 경험”을 우선한다.

16. 최종 원칙

iiPaintEngine은 단순 그래픽 라이브러리가 아니다.

이 엔진의 목적은 사용자의 입력 흐름을 가능한 낮은 마찰로 시각적 사고로 변환하는 것이다.

따라서 가장 중요한 것은 기능 수가 아니라:

* 입력 품질
* 반응성
* 상태 일관성
* 장기 확장성
* 구조적 안정성

이다.