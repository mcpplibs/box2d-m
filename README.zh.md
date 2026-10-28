# box2d

> 以 C++23 模块使用 Box2D v3——游戏用 2D 物理引擎。

[English](README.md) · **简体中文** · [Upstream](https://github.com/erincatto/box2d) · [Releases](https://github.com/mcpplibs/box2d-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [Index](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![Box2D](https://img.shields.io/badge/Box2D-MIT-blue.svg)](upstream/box2d/LICENSE)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/box2d-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/box2d-m/actions/workflows/ci.yml)

Box2D 3.2.0 模拟二维刚体：刚体、形状、关节、接触与传感器事件、射线与形状投射、
连续碰撞和内置的任务调度器，以 MIT 许可的 C17 编写。上游 C API 保持不变：
`b2CreateWorld`、`b2World_Step`、`b2CreatePolygonShape`、`b2MakeRot` 都是
`<box2d/box2d.h>` 的上游全局名字。库本身是 `compat.box2d` 3.2.0，按上游为确定性
所做的方式构建（`-ffp-contract=off`、上游默认的 SIMD 路径）。Box2D 跨平台确定，
每次 CI 都会检查：上游自己的确定性场景和一个被撞倒的 20 层金字塔，在 Linux x86_64
（GCC、LLVM）、Linux arm64（GCC、LLVM）、macOS、Windows 和 wasm32 上，用 1 到 8 个
worker、两种 SIMD 宽度，得到逐位相同的结果。

## 模块

- `import mcpplibs.mwrap.box2d;`：`<box2d/box2d.h>` 的完整 API（世界、刚体、形状、链、关节、事件、查询、碰撞与几何、内联数学函数、向量运算符），头文件的静态常量（`b2_nullBodyId`、`b2Rot_identity` 等）位于 `mcpplibs::mwrap::box2d`，并重新导出下面的模块
- `import mcpplibs.mwrap.box2d.macros;`：`B2_PI`、`B2_MAX_POLYGON_VERTICES`、`B2_HASH_INIT`、`B2_LINEAR_SLOP()`、`B2_ID_EQUALS` 等，位于 `mcpplibs::mwrap::box2d::macros`

> 更多模块 API -> [用法与 API](docs/usage.md)

## 快速开始

```sh
mcpp new tower --template mcpplibs.mwrap.box2d@3.2.0
cd tower
mcpp run
```

默认模板以固定的 60 Hz 步长用一个球撞倒一座箱子塔，打印箱子下落的位置，最后输出
终态的哈希：在每个平台上都相同。

```toml
[dependencies]
"mcpplibs.mwrap.box2d" = "3.2.0"
```

```cpp
import std;
import mcpplibs.mwrap.box2d;

int main() {
    b2WorldDef worldDef = b2DefaultWorldDef();  // gravity (0, -10)
    b2WorldId world = b2CreateWorld(&worldDef);

    b2BodyDef groundDef = b2DefaultBodyDef();
    groundDef.position = b2Vec2{0.0f, -1.0f};
    b2BodyId ground = b2CreateBody(world, &groundDef);
    b2Polygon groundBox = b2MakeBox(20.0f, 1.0f);
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    b2CreatePolygonShape(ground, &shapeDef, &groundBox);

    b2BodyDef boxDef = b2DefaultBodyDef();
    boxDef.type = b2_dynamicBody;
    boxDef.position = b2Vec2{0.0f, 4.0f};
    b2BodyId box = b2CreateBody(world, &boxDef);
    b2Polygon square = b2MakeBox(0.5f, 0.5f);
    b2CreatePolygonShape(box, &shapeDef, &square);

    for (int i = 0; i < 90; ++i) b2World_Step(world, 1.0f / 60.0f, 4);
    b2Vec2 p = b2Body_GetPosition(box);
    std::println("the box rests at ({:.2f}, {:.2f})", p.x, p.y);  // (0.00, 0.50)
    b2DestroyWorld(world);
}
```


## 确定性

Box2D 3 在它支持的每个平台上，对相同的输入给出相同的结果，前提是浮点按上游的方式
构建：不把 `a*b+c` 收缩为 FMA。`compat.box2d` 就是这样构建库的。你自己参与模拟的
浮点运算（自己算的位置、`b2MakeRot` 等会编译进你代码里的内联数学函数），在有 FMA 的
地方（arm64 总有；x86_64 在 AVX2/FMA 下）也需要同样的选项：
`[build] cxxflags = ["-ffp-contract=off"]`，本仓库的测试和示例就是这样做的。
[tools/run_determinism.py](tools/run_determinism.py) 和 [examples/box2d](examples/box2d)
（`box2d-determinism`）可在任何机器上报告这些哈希。

## 宏

`B2_PI`、`B2_MAX_POLYGON_VERTICES` 等常量保持其宏的类型。`B2_HUGE`、`B2_LINEAR_SLOP`
等读取 `b2_lengthUnitsPerMeter` 的值是函数（`B2_LINEAR_SLOP()`），因为
`b2SetLengthUnitsPerMeter` 会在运行时改变它们。`B2_IS_NULL`、`B2_IS_NON_NULL`、
`B2_ID_EQUALS` 是函数体就是宏本身的 constexpr 函数。声明说明符（`B2_API`、
`B2_INLINE` 等）、初始化器 `B2_NULL_ID` / `B2_ZERO_INIT` 以及断言 `B2_ASSERT` /
`B2_VALIDATE` 保留为宏，在 `<mwrap/box2d/macros.hpp>` 中，于 import 之前包含。
详见 [docs/usage.md](docs/usage.md)。

## 示例

- [examples/box2d](examples/box2d)：`box2d-determinism`，在指定的 worker 数和 SIMD 路径下运行确定性场景，
  打印休眠步数和哈希，用来比较不同机器与编译器。

## 构建与测试

Linux（x86_64 GCC 与 LLVM、arm64 GCC 与 LLVM）、macOS 和 Windows 的 CI 构建模块，
运行测试、导出审计（[tools/check_exports.py](tools/check_exports.py)）和确定性检查；
Web 任务以 wasm32-emscripten 在 node 下运行测试和确定性检查。审计检查生成的源文件是否
最新、每个声明是否都已导出、每个宏是否已映射或列明原因。另有一个任务把默认模板作为
已安装的包生成项目并运行。

## 许可

包装代码为 [Apache-2.0](LICENSE)，由 mcpp-community 提供（[NOTICE](NOTICE)）。
Box2D 为 MIT（[upstream/box2d/LICENSE](upstream/box2d/LICENSE)）；它的许可证和 README
会部署在每个依赖本包的程序旁边。
