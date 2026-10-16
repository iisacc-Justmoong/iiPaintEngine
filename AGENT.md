# iiPaintEngine 개발 계약

iiPaintEngine은 C++23 비트맵 파일 페인팅 라이브러리이다. 공개 API는 표준 라이브러리만 사용하며, 내부 이미지 코덱은 MIT 라이선스 stb를 고정 커밋으로 포함하여 C++23 번역 단위에서 컴파일한다. Qt, QML, LVRS, iiFileProvider, 다른 언어의 제품 소스 및 상업용 프레임워크 의존성을 추가하지 않는다.

## 비트맵과 입력

입력은 `PointerEvent → 현재 StrokePoint → RasterDabStream → BrushDab → RasterSample → StrokeCompositeBuffer → RasterLayer` 순서로 즉시 픽셀화한다. InputStrokeBuilder는 활성 여부만, RasterDabStream은 직전 점 하나와 거리·시간·방향·난수·스칼라 상태만 가진다. 좌표 목록, 전체 궤적, 벡터·텍스트 레이어, 미래 위치, 입력 replay는 생성하지 않는다. 외부 텍스트·SVG·도형은 엔진 진입 전에 픽셀로 변환한다.

BitmapPaintSession은 BitmapFile, 선택 브러시, pending/preview 픽셀, dirty rect 픽셀 이력을 소유한다. 활성 획은 시작 설정을 사용한다. release는 preview와 같은 픽셀을 확정하고 cancel은 pending 픽셀을 폐기한다. undo/redo는 픽셀 patch를 교환한다. 미리보기는 새 dab의 sample pixel만 갱신한다. 인과적 smoothing은 현재 점과 직전 filtered 점 하나만 사용한다. 마모와 획 색상은 스칼라 상태이다.

앱은 화면, 입력 수집, 좌표 변환, 시계와 이벤트 정책을 담당한다. 시간 분사는 앱이 advanceTime에 단조 증가하는 초를 전달하여 처리한다. 엔진은 타이머나 입력 좌표 queue를 소유하지 않는다. 파일 교체·clear·undo/redo·취소는 미완성 입력을 정리한다. BitmapFile은 실제 파일 경로, 감지 형식, ARGB32 픽셀, 수정 상태를 소유한다.

## 모듈과 의존성

Application은 BitmapFile/Input/Document/Layer/Brush/Stroke/Render/History 및 기타 작업 모듈을 조합한다. 하위 모듈은 Application을 참조하지 않는다. Core는 표준 타입만 사용한다. Brush는 Stroke를 참조하지 않는다. Input은 현재 StrokePoint 하나를 방출한다. Document의 내용은 Layer/DrawingSurface 픽셀이다. Render cache는 픽셀 tile과 stamp atlas만 가진다. 순환 의존성과 상향 참조를 금한다.

프리셋 버전 3의 확장 옵션과 버전 1/2 읽기를 유지한다. 문서 버전 3은 픽셀·메타데이터·마스크·브러시·asset·raster history를 저장하고 호환 가능한 버전 2 단일 표면을 이관한다. presetV2라는 역사적 필드명 안의 formatVersion이 실제 프리셋 버전을 결정한다. 무효·미래 버전은 거부한다. 옵션 계약은 docs/BRUSH_OPTIONS.md, API 이관 계약은 docs/CXX23_MIGRATION.md에 따른다.

## 변경과 검증

모든 소스 변경에는 문서와 테스트 갱신이 따른다. 빌드 디렉터리는 build/이다. 변경 뒤 전체 빌드와 CTest를 수행한다. 예제와 테스트도 C++23으로 빌드하며 다른 언어의 실행 스크립트를 제품에 포함하지 않는다. CMake는 빌드 메타데이터이다. 미래 요구를 예측한 추상화·입력 replay 모델을 추가하지 않는다.

NativeDependencyContract와 DependencyBoundary는 언어·프레임워크·계층 경계를 고정한다. NativeBitmapContract는 형식 탐지·왕복·실패 보존을, BitmapPaintSessionContract와 PreviewPixelContract는 즉시 미리보기·확정·취소·이력·시간 분사를 검증한다. 기존 brush/input/document/layer 계약을 유지하고, InstallUpgradeContract는 삭제된 헤더를 제거한 설치본을 별도 C++23 소비자가 링크하여 실행하는지 검증한다.

공개 API나 구조체 배치 변경 시 소비자를 재빌드한다. 소스 빌드·설치·실행 증거를 구별하고 실제 펜 및 다른 플랫폼 검증 여부를 명시한다. 기존 작업과 사용자 데이터를 보존한다.
