# 선택과 래스터 편집의 픽셀 의미

iiPaintEngine 0.5.2에서 `SelectionState`와 `RasterEditTool`의 동작을 고정한다. 공개 구조체 배치와 파일 형식은 바뀌지 않는다. 이전 버전에서 무시하던 값을 실제로 반영하므로, 해당 잘못된 출력에 의존하던 소비자는 결과를 다시 검증해야 한다.

## 선택 평가

- 비활성 선택은 전체 커버리지 1이다. 이 경우 반전·feather 설정은 적용하지 않는다.
- 사각 선택은 문서 좌표의 반열린 경계 `[x,x+width) × [y,y+height)`이다. 픽셀 커버리지는 정수 좌표에서 평가한다. 무한·NaN·양수가 아닌 사각형은 생성 시 비활성이다.
- 마스크는 `mask.bounds`의 원점과 크기 및 행 우선 byte alpha로 평가한다. `selection.bounds`가 마스크의 실제 커버리지를 대체하지 않는다. 경계 밖 또는 부족한 저장 공간의 표본은 0이다.
- `featherRadius > 0`이면 `ceil(radius)` 픽셀 반경의 정규화된 사각 box 커널을 적용한다. 선택 밖은 0으로 채우며 커널의 전체 면적으로 나눈다. 반경 1이면 3×3 표본의 평균이다. Gaussian이나 거리장 기반 feather를 뜻하지 않는다.
- 비유한·0 이하 반경은 feather를 끈다. 표현 가능한 최대 반경은 `Types::Pixel`의 최대값이다. 사각 선택은 격자 교집합을 상수 시간에 계산한다. 마스크 단일 조회는 커널과 실제 저장 공간의 교집합을 읽으므로, 큰 마스크를 반복 평가하는 소비자는 별도 사전 래스터화·타일 캐시의 성능 예산을 정해야 한다.
- 반전은 feather가 끝난 커버리지에 **한 번만** `1-alpha`로 적용한다.
- `selectionContains`는 feather 없는 사각형에는 연속 좌표 경계를 사용한다. 마스크와 feather 선택은 `floor(point)`가 가리키는 픽셀의 커버리지가 양수인지 검사한다. 마스크 구멍과 부드러운 외곽도 반영한다.

예를 들어 `[1,6) × [1,6)` 사각형에 반경 1을 적용하면 `(1,1)`은 4/9, `(1,3)`은 2/3, `(0,3)`은 1/3, `(3,3)`은 1이다. 반전 뒤 `(1,1)`은 5/9이다.

## 채우기·그라디언트·지우개

색은 straight ARGB32이며 RGB 채널의 기존 인코딩을 유지한다. Fill과 Gradient는 기존 `compositeRasterPixel`의 SourceOver를 재사용한다. Tool → Render 의존성을 허용하되 Render는 Tool이나 상위 앱을 참조하지 않는다.

유효 불투명도는 `clamp(operation.opacity,0,1) × selectionAlphaAt(...)`이다. 색의 alpha는 이 값과 별도로 곱해진다. 반투명 선택을 단순한 선택 여부로 축소하지 않는다. 비유한 불투명도·비활성 연산·0 불투명도는 원본 byte를 보존한다. 완전히 투명한 fill/gradient 표본도 원본을 유지한다.

- Linear는 시작→끝 방향의 투영 거리로 색을 보간한다.
- Radial은 시작점을 중심, 시작→끝 길이를 반경으로 사용한다. 반경 바깥은 끝 색이다.
- 길이 0이면 시작 색을 사용한다. 비유한 좌표나 표현할 수 없는 길이는 연산 전체를 무작업으로 처리한다.
- 기존 채널별 ARGB 보간을 유지한다. 선형광 보간이나 16비트 작업 문서 지원을 추가하는 변경은 아니다.
- Eraser는 선택 커버리지까지 곱한 DestinationOut에 해당하는 alpha 감소를 적용하며 RGB byte를 유지한다.

불투명 검정에 불투명 빨강을 opacity=0.5로 채우면 `0xFF800000`이다. 선택 alpha=128과 opacity=0.5를 함께 쓰면 `0xFF400000`이다. 불투명 파랑 위의 `0x80FF0000`은 `0xFF80007F`가 된다. 지우개가 alpha=128 선택을 통과하면 원래 alpha=255는 127이 된다.

이 API는 flood-fill 탐색이나 편집 이력 세션을 만들지 않는다. iiSharedCanvas의 `floodMask`·`fillRaster`·`featherMask`는 별도 API이며, 이 수정만으로 그 경로의 픽셀·가장자리 정책까지 동일하다고 주장하지 않는다.

## 검증

`SelectionRasterEditContractTest.cpp`는 위의 독립 기대값과 마스크 구멍·반전·경계 overflow·반투명 합성·방사형 중심/중간/외곽·불투명도 0·지우개를 검사한다. 기존 `EditingToolPipelineContract`를 함께 유지한다. `InstallUpgradeContract`는 설치한 공개 헤더와 라이브러리로 같은 공개 동작 검사를 별도 실행 파일에서 빌드·실행한다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

호스트 빌드·설치 소비자 통과는 해당 호스트의 증거이다. 앱 채택, 실제 펜, 다른 플랫폼 실행, 대형 마스크의 성능 검증은 별도 단계이다.
