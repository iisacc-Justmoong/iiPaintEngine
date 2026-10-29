# Filter pipeline costs

The ARGB box blur maintains exact 64-bit column sums and slides a horizontal
window across them. Its work is O(width * height), independent of blur radius,
instead of repeating a square neighborhood scan for each selected pixel. It
keeps the existing source snapshot and adds O(width) scratch memory (32 bytes per
column). A sparse selection still requires scanning the image.

Edges use only in-bounds samples. Division happens once after summing integer
channels, without intermediate rounding or a separable approximation. Selection
predicates and rounding of finite radii are preserved. Oversized windows are
clipped before integer conversion; radii below one use a one-pixel radius.
Empty surfaces are no-ops.

`FilterWindowParityTest` checks every pixel against an independent scalar
reference across empty/single-row/single-column images, random ARGB data, radius
rounding, oversized windows, rectangles, inverted selections and alpha masks.
Existing editing/pipeline tests cover disabled nodes and chained filters.

## 투영된 브러시 샘플의 초기 예약

초기 샘플 벡터 예약은 입력 dab에 실제 투영을 적용한 범위로 추정한다.
작게 투영되는 큰 이미지 팁이 원본 이미지 전체 크기로 메모리를 미리 예약하는
것을 피하며, 초기 예약 힌트는 최대 1,048,576개 샘플로 제한한다. 안전하게
범위를 추정할 수 없으면 미리 예약하지 않고 기존 그리기 경로에 맡긴다.

이 값은 초기 예약만 제한한다. 반환 벡터의 최종 길이, 이후 재할당, 큰 투영의
계산량 또는 프로세스 전체 메모리의 상한은 아니다. 추정에는 각 dab의 범위를
다시 계산하는 비용이 있으며 실제 성능은 입력과 측정 범위를 밝혀 평가한다.
`ProjectedReserveContractTest`는 축소된 이미지 팁과 투영 오버로드, 보이지 않는
입력, 빈 입력 및 먼 좌표의 예약량을 확인한다. 기존 브러시·투영 검사는 출력
픽셀의 동등성을 별도로 확인한다.
