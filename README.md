# iiPaintEngine

`iiPaintEngine`은 Qt 기반 페인트 엔진 라이브러리이다. Canvas 계열 객체는 `QObject`를 공개 상속하여 Qt object tree와 meta-object system에 연결된다. Canvas가 아닌 Brush/Stroke 계열 객체는 값 복사와 aggregate 초기화를 지원하는 단순 `struct`이다.

## 현재 객체

- Canvas QObject 클래스: `DrawingSurface`, `Layer`
- Brush 단순 struct: `BrushSnapshot`, `BrushDynamics`, `BrushShape`, `BrushResolve`
- Stroke 단순 struct: `Rasterizer`, `Stabilizer`, `Stroke`, `StrokePoint`

Canvas 객체는 `explicit ClassName(QObject *parent = nullptr)` 생성자를 제공한다. 단순 struct 객체는 생성자를 제공하지 않고 공개 필드만 유지한다.

## 검증

빌드 디렉터리는 `build/`만 사용한다.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
