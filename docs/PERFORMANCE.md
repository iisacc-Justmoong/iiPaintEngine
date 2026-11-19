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

기본 `projectBrushDabs`의 이 값은 초기 예약에 적용된다. 라이브 세션은
아래의 이벤트 예산으로 샘플 수와 픽셀 순회를 함께 제한한다. 추정에는 각 dab의
범위를 다시 계산하는 비용이 있으며 실제 성능은 입력과 측정 범위를 밝혀 평가한다.
`ProjectedReserveContractTest`는 축소된 이미지 팁과 투영 오버로드, 보이지 않는
입력, 빈 입력 및 먼 좌표의 예약량을 확인한다. 기존 브러시·투영 검사는 출력
픽셀의 동등성을 별도로 확인한다.

## 라이브 세션의 이벤트 예산

`BitmapPaintSession`과 `MaskPaintSession`은 생성자에서 `RasterEventLimits`를 받는다.
기본값은 이벤트당 `maxDabsPerEvent` 4,096과 `maxProjectedPixels` 1,048,576이다.
호출자가 지정하는 값에는 각각 65,536과 16,777,216의 상한이 적용된다. 비트맵을
다시 열거나 마스크를 교체해도 생성 시 지정한 한도가 적용된다.

최소 간격은 0.01px이다. dab 한도는 배치 시도 수와 최종 출력 수에 각각 적용된다.
산포로 출력이 없는 배치도 시도 수에 포함되며, 대칭 복제는 최종 출력 수에 포함된다.
픽셀 예산은 캔버스와 교차하는 각 dab의 후보 픽셀 방문 수의 합이다. 여러 dab이
같은 픽셀을 방문하면 각각 합산한다. 전체 이벤트를 검증한 뒤 샘플을 할당하거나
마스크를 갱신한다. 압력과 간격으로 정해진 보간은 한도 안에서 입력 그대로 적용된다.

Press·Move·Release와 비트맵의 `advanceTime`에서 이벤트 예산을 초과하면 업데이트의
`code`가 `EngineErrorCode::BudgetExceeded`로 반환된다. 거부된 이벤트는 포인터 및
브러시 스트림 상태, 미리보기, 원본 픽셀, Undo·Redo 이력을 보존한다. 다음 입력은
마지막으로 수락된 좌표 및 시각을 기준으로 처리된다. 업데이트 타입과 세션의
C++ 객체 배치가 바뀌었으므로 이 소스로 빌드한 패키지의 소비자도 재빌드해야 한다.

## 비트맵 스트로크 버퍼 재사용

비트맵을 열거나 만들 때 `m_pending`과 `m_preview`를 확보하고 파일을 닫거나 교체할
때까지 유지한다. 커밋 후에는 이번 스트로크의 변경 사각형에 해당하는 누적값을
지운다. 취소하거나 새 Press가 진행 중인 획을 교체할 때는 같은 영역의 미리보기를
확정 픽셀로 복원하고 취소 결과에 그 영역을 반환한다. Undo·Redo는 이력 패치만 동기화한다.
리셋 비용은 변경 사각형의 면적에 비례한다. 전체 지우기는 전체 캔버스를 변경하며,
Undo·Redo는 기록된 사각형만 처리한다.

`PaintSessionBudgetContractTest`는 10억 px 입력, 최소 간격, 투영·산포·대칭·시간 예산과
거부 후 스트로크 재개를 검사한다. 기본 2048×2048 캔버스의 100회 스트로크에서
전체 버퍼를 매번 초기화하는 기준 경로와 미리보기 픽셀이 비트 단위로 일치하는지
비교한다. `8192` 인자로 8192×8192에서 같은 검사를 수행할 수 있다. 할당 카운터가
라이브러리의 C++ 할당을 감지하는지 먼저 확인한 뒤, 비정상 입력의 요청 바이트와
스트로크·취소·이력 처리 중 캔버스 크기 할당 0회를 검사한다.

## 비트맵 Undo·Redo의 메모리 예산

`BitmapPaintSession`의 Undo·Redo는 기본 64 MiB와 32개 항목을 공유한다. 호출자는
세 번째 생성자 인자 `BitmapPaintHistoryLimits`로 `maximumBytes`와 `maximumEntries`를
지정할 수 있다. 두 기본값은 `RasterHistoryLimits`에서 가져온다. 문서 아카이브의
`HistoryStack::maxUndoCommands`는 문서 명령의 개수 제한을 담당한다.

비트맵 편집은 확정 전후의 ARGB32 픽셀을 XOR한 델타를 한 번 저장한다. 같은 델타를
적용해 Undo와 Redo를 처리하므로 이력 이동에 추가 픽셀 버퍼가 필요하지 않다.
연속으로 같은 델타 값은 RLE로 인코딩한다. 압축 이득이 없으면 밀집 XOR 값으로
저장한다. `clear()`는 원본 픽셀과 채울 색으로 직접 인코딩하며, 픽셀이 같은 색으로
유지되면 이력을 추가하지 않는다.

새 편집을 확정할 때 Redo 항목을 폐기하고, 이력이 두 한도에 맞을 때까지 가장 오래된
항목부터 제거한다. `historyStatistics()`는 커서 양쪽의 항목 수와 항목 메타데이터 및
페이로드를 합산한 `retainedBytes`를 반환한다. 큐의 예약 공간과 할당자 메타데이터는
별도 오버헤드다. 편집을 확정하는 동안에는 기존 이력과 예산 크기 이내의 새 후보
한 항목을 함께 보관할 수 있다.

단일 편집이 예산보다 크면 원본 픽셀과 확정된 이력을 보존한다. `clear()`는 진행 중인
미리보기도 보존하고 `false`와 `lastError()`를 반환한다. 스트로크 종료 시에는
`BudgetExceeded`를 반환하고 스트로크를 끝내며, 복원한 미리보기 영역을 `dirtyBounds`로
알린다. 이력 항목이나 바이트 한도가 0이면 픽셀을 바꾸는 편집에 같은 규칙을 적용한다.
세션의 C++ 객체 배치가 바뀌었으므로 패키지 소비자도 재빌드해야 한다.

`PaintSessionBudgetContractTest`는 바이트와 항목 수 제한, 오래된 이력 제거, 이력 분기,
예산 초과 후 미리보기와 이력 복원, 밀집 델타의 픽셀 일치 여부를 확인한다.
1024×768 캔버스에 작은 획을 그린 사례에서 `clear()`의 신규 이력 바이트와 C++
할당량이 전체 ARGB32 백업보다 작고, Undo·Redo에 추가 할당이 없는지도 검사한다.

## 문서 아카이브 직렬화 성능

`DocumentSerializer`는 입력 키와 값을 `std::string_view`로 읽어 해시 인덱스에 등록한다.
반복 조회는 접두사와 접미사를 재사용 버퍼에서 이어 붙인다. 인덱스는 입력 바이트와
조회한 키 길이의 합에 대해 평균 선형이며, 같은 키가 반복되면 마지막 값을 사용한다.
인덱스 노드는 블록 단위로 확보하고 읽기 종료 시 일괄 회수한다. 해시 테이블을
확장할 때는 교체한 버킷 배열을 반환한다.

일반 숫자와 좌표에는 `std::to_chars`와 `std::from_chars`를 사용한다. 기록할 때 접두사,
필드명, 숫자를 출력 버퍼에 직접 추가한다. 부동소수점의 일반 표기와 정밀도
`std::numeric_limits<long double>::digits10 + 1`은 버전 3의 기존 출력과 같다.
사용자 지정 로케일, 형식 추출이 필요했던 이전 숫자 표기, 부동소수점 `charconv`를
제공하지 않는 배포 타깃은 재사용 스트림으로 처리한다. Apple 표준 라이브러리는
macOS 13.3 이상을 대상으로 쓸 때와 26.0 이상을 대상으로 읽을 때 부동소수점 `charconv`를 제공한다.

`DocumentSerializerPerformanceTest`는 저장된 버전 3 골든을 버전 4로 이관하고
직렬화 결과를 비교한다. 부호 있는 0, 경계값, CRLF, 레코드 순서를 바꾼 입력,
중복 키의 마지막 값, 로케일 및 인용문 처리도 검사한다.

기존 버전 3 벤치는 중첩 레이어 4,000개의 아카이브에서 100,039개 필드와 5,372,229바이트를
처리한다. 파일 I/O와 데이터 준비 시간은 측정 범위에서 제외하고, 예열 후 9회씩
파싱과 직렬화의 중앙값을 구한다. Apple M1 Max의 macOS 27.2.1 Release에서 기존
파싱은 42.50ms, 직렬화는 14.46ms였다. 수정 후 3회 실행에서 각각 15.59~16.46ms와
5.78~5.99ms가 측정됐다.

`IIPAINTENGINE_ARCHIVE_PARSE_MAX_MS`에 양의 정수 밀리초를 지정하면 파싱 중앙값을
제한한다. 기준 장비에서 32ms로 설정하면 기존 구현은 실패하고 새 구현은 통과한다.
기본값은 장비별 차이를 고려해 측정값 출력과 정확성 검사로 운영한다.

```sh
IIPAINTENGINE_ARCHIVE_PARSE_MAX_MS=32 ctest --test-dir build -C Release \
  -R '^iiPaintEngineDocumentSerializerPerformance$' --verbose --output-on-failure
```

## 문서 아카이브 버전 4의 메모리 사용량

`DocumentArchiveMemoryTest`는 각각 4096×2048인 표면과 레이어, 알파 마스크와 썸네일을
포함한 문서를 비트 단위로 왕복한다. 호출 전의 원본 문서와 입력 페이로드는 측정에서
제외한다. 각 호출에서 생긴 C++ 할당 중 가장 많이 동시에 살아 있는 시점의 바이트를
센다. 메모리 직렬화의 출력 문자열과 읽기가 만든 벡터는 각각 해당 호출의 피크에 포함된다.
할당 추적기가 라이브러리의 출력과 픽셀 할당을 관측하는지 확인한 뒤 두 피크 모두
아카이브 크기의 1.5배 이내인지 검사한다.
Windows DLL의 할당을 추적하지 못하는 환경에서는 바이트 왕복과 스트림 오류를 검사한다.

같은 macOS Release 테스트에서 버전 3 페이로드는 150,997,128바이트였으며 쓰기 피크는
402,658,160바이트, 읽기 피크는 75,502,688바이트였다. 버전 4는 페이로드
75,499,702바이트, 쓰기 피크 75,499,776바이트, 읽기 피크 75,502,688바이트였다.
버퍼를 보관하지 않는 검증용 스트림의 쓰기 피크는 72바이트였고, 짧은 쓰기도 검출했다.

문자열 API는 최종 출력 크기를 계산하는 메타데이터 패스를 한 번 더 수행한다. 스트림 API는
한 패스로 기록한다. 버전 4의 100,039개 필드 벤치는 5,304,257바이트를 처리했으며,
9회 중앙값은 파싱 18.93ms와 문자열 직렬화 9.91ms였다.

## 문서 이력 페이로드 소유권

문서를 읽을 때 `readHistory()`는 검증한 수만큼 Undo·Redo 벡터를 예약하고 각 명령을
최종 위치에서 직접 생성한다. `readCommand()`도 패치 수를 확인한 뒤 명령의 패치
벡터에서 각 항목을 직접 생성한다. 읽어 온 바이트 벡터의 소유권을 해당 패치에 이전하므로
디코딩 이후 이력에 넣는 과정에서 페이로드 크기에 비례하는 추가 복사가 없다.

`DocumentArchiveMemoryTest`는 Undo와 Redo에 두 패치씩 넣어, 크기가 서로 다른
256 KiB 이상의 전후 상태 페이로드 8개를 검사한다. 역직렬화 중 각 크기로 할당을 한 번만
요청하고, 완성된 이력이 최초 할당의 주소를 유지하며, 다시 직렬화한 바이트가 원본과
같은지 확인한다.
