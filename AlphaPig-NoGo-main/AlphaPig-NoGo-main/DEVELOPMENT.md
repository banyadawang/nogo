# AlphaPig-NoGo 开发文档

> 版本：v2.0（GUI + GPU 训练 + scanAvailable 优化 + 验证框架）
> 更新日期：2026-10-21

---

## 1. 项目概述

AlphaPig-NoGo 是一个基于 **神经网络 + MCTS** 的不围棋（NoGo）AI，采用 AlphaZero 风格的自对弈训练架构：

- **C++ 引擎**负责 MCTS 搜索、NoGo 规则判定与神经网络前向推理（Intel oneMKL 加速）；
- **Python 脚本**负责训练（Keras/TensorFlow 的 CPU 版本，PyTorch 的 GPU 版本）；
- **SFML GUI** 提供人机对战界面，并显示 MCTS 搜索调试信息；
- 附带一套**验证/实验框架**（SelfPlayValidate + PowerShell 自动化 + Python 报告生成），用于对 MCTS 优化（如合法着法扫描、树复用）做量化评估。

### 技术栈

| 组件 | 技术 |
|------|------|
| 引擎语言 | C++（C++20） |
| 推理加速 | Intel oneMKL（`cblas_sgemm`）/ Botzone 版为手写 AVX-256 SIMD |
| 并行 | OpenMP |
| 训练 | Keras + TensorFlow（CPU）/ PyTorch（GPU，推荐） |
| GUI | SFML 2.6 |
| 构建 | Visual Studio 2022 + Intel C++ Compiler 2025（SelfPlay）/ CMake + MSVC（GUI） |
| 脚本 | PowerShell、Python 3 |

### 版本历史

| 版本 | 提交 | 内容 |
|------|------|------|
| v1.x | `23237a3` Initial commit | 基础引擎 + Keras 训练 + Botzone 接口 |
| v2.0 | `03492a0` | GUI、GPU 训练（PyTorch）、scanAvailable BFS 优化、验证框架 |

---

## 2. 目录结构

```
AlphaPig-NoGo/                          # 项目根目录
├── SelfPlay/                           # C++ 引擎（MCTS + Network + NoGo 规则）
│   ├── Alphapig.cpp                    # AlphaPig 类：MCTS 搜索、棋盘规则、合法性判定
│   ├── Network.cpp                     # 神经网络推理（MKL，含各层定义与权重加载）
│   ├── SelfPlay.cpp                    # 自对弈训练数据生成入口（10000 局/轮，OpenMP）
│   ├── SelfPlayValidate.cpp            # 验证框架入口（读配置文件、跑 N 局并记录性能）
│   ├── SelfPlay.sln / .vcxproj         # Visual Studio 工程（Intel C++ Compiler 2025）
│   └── CMakeLists_validate.txt         # SelfPlayValidate 的备用 CMake 定义
├── gui/                                # SFML 可视化 GUI
│   ├── main.cpp                        # 主窗口（900×700，人机对战主循环）
│   ├── BoardRenderer.h/.cpp            # 9×9 棋盘、棋子、热力图、提示渲染
│   ├── GameController.h/.cpp           # 对局流程控制（当前玩家、胜负判定、重置）
│   ├── EngineBridge.h/.cpp             # 引擎封装（GUI 访问引擎的唯一入口）
│   ├── Engine.h / SearchInfo.h         # 前置声明 / 搜索调试信息结构体
│   ├── test_tree_reuse.cpp             # 树复用逻辑单元测试（6 组测试用例）
│   ├── CMakeLists.txt                  # 构建 AlphaPigGUI + SelfPlayValidate
│   ├── assets/                         # 字体资源（font.ttf）
│   └── build/                          # CMake 构建产物（含 Release 可执行文件）
├── work/                               # 训练工作目录（核心，详见 §7）
│   ├── SelfPlay.exe                    # 自对弈可执行文件
│   ├── Train.py                        # Keras CPU 训练脚本
│   ├── Train_torch.py                  # PyTorch GPU 训练脚本（当前主力）
│   ├── Trainloop.ps1                   # CPU 训练循环（SelfPlay → Train.py）
│   ├── Trainloop_gpu.ps1               # GPU 训练循环（SelfPlay → Train_torch.py）
│   ├── dataset/                        # 训练数据（SelfPlay 生成）
│   ├── model.keras / model_torch.pt    # 训练检查点
│   └── network.weights*                # 导出给 C++ 引擎的权重（多份历史备份）
├── tools/                              # 开发与实验工具
│   ├── Config.h                        # key=value 配置读取器
│   ├── PerformanceLogger.h             # 逐着/逐局性能 CSV 记录器
│   ├── compare_results.py              # 两组实验 CSV 的对比报告生成
│   └── run_experiment.ps1              # 一键实验脚本（baseline vs tree_reuse）
├── configs/                            # 验证框架的测试/训练配置（key=value 文本）
├── benchmark/                          # scanAvailable 正确性 + 性能基准
├── search/                             # FastScanAvailable.h（BFS 合法着法扫描实现）
├── results/                            # 实验输出目录（按时间戳归档）
├── Botzone.cpp                         # Botzone 竞赛接口（单文件自包含版）
├── Train.py / Trainloop.ps1            # 旧版训练脚本副本（现行工作流以 work/ 为准）
├── model.keras / model_torch.pt / network.weights   # 旧版模型文件副本
└── README.md                           # 项目简介（使用向）
```

---

## 3. 系统架构与数据流

### 3.1 训练闭环

```
                    ┌──────────────────────────────┐
                    │   SelfPlay.exe（C++/OpenMP）  │
                    │   10000 局自对弈              │
                    │   每步 MCTS 搜索 200 次模拟    │
                    └──────────────┬───────────────┘
                                   │ (state, π, z) 样本
                                   ▼
                    ┌──────────────────────────────┐
                    │   dataset/<timestamp>.txt    │
                    └──────────────┬───────────────┘
                                   │ 随机采样 256k 条
                                   ▼
                 ┌─────────────────────────────────┐
                 │  Train.py (Keras) /              │
                 │  Train_torch.py (PyTorch, GPU)   │
                 │  4 epoch × batch 256，SGD        │
                 └──────────────┬──────────────────┘
                                │ 导出二进制权重
                                ▼
                 ┌─────────────────────────────────┐
                 │  network.weights（约 1.6 MB）    │
                 │  → 被 SelfPlay / GUI / Validate  │
                 │    / Botzone 的 C++ 引擎加载     │
                 └─────────────────────────────────┘
```

`Trainloop.ps1`（CPU）与 `Trainloop_gpu.ps1`（GPU）将上述流程组织为无限循环：先自对弈生成数据，再训练更新网络。

### 3.2 关键产物与放置位置

| 文件 | 生产者 | 消费者 | 放置位置 |
|------|--------|--------|----------|
| `model_torch.pt` | `Train_torch.py` | `Train_torch.py`（续训加载） | `work\` |
| `model.keras` | `Train.py` | `Train.py`（续训加载） | `work\` |
| `network.weights` | 两个训练脚本 | 所有 C++ 可执行文件 | 见下方说明 |
| `dataset\*.txt` | `SelfPlay.exe` | 两个训练脚本 | `work\dataset\` |
| `results\*.csv` | `SelfPlayValidate.exe` | `compare_results.py` | 可配置 |

> **`network.weights` 放置规则**：所有 C++ 程序按**运行时工作目录**加载它（默认文件名 `network.weights`），因此：
> - `work\network.weights` —— `SelfPlay.exe` 自对弈用；
> - `gui\build\Release\network.weights` —— `AlphaPigGUI.exe` / `SelfPlayValidate.exe` 用；
> - `gui\AlphaPigGUI.exe` 支持命令行参数指定任意权重路径；`SelfPlayValidate.exe` 通过配置项 `model.weight_file` 指定。
> - 每次训练完成后，需将 `work\` 中新导出的权重复制到上述目录。

---

## 4. 不围棋（NoGo）规则与棋盘表示

- 棋盘为 9×9，一维数组 `board[81]` 存储：`0` = 空，`1` = 己方（AI），`-1` = 对方（OP）。
- **不围棋规则**：落子时**禁止自杀**（落子后己方棋块无气），且**禁止提子**（落子导致相邻对方棋块无气，即不允许吃子）。二者任一违反即为非法着法。
- 终局：轮到一方走棋但**无任何合法着法**时判负，对方获胜（无需数子/点目）。
- 合法性判定核心为 `AlphaPig::judgeAvailable(board, p, col)`（[Alphapig.cpp](SelfPlay/Alphapig.cpp)）：先模拟落子，递归检查己方棋块与相邻对方棋块是否都有气，随后恢复棋盘。

---

## 5. MCTS 引擎（SelfPlay/Alphapig.cpp）

### 5.1 数据结构

`AlphaPig::TreeNode` 每个节点保存：

- 完整棋盘副本 `board[81]` 与当前行棋方 `color`；
- **双合法着法列表**：`available_next`（当前行棋方合法着法）、`available_last`（对方合法着法）——通过维护两份列表避免每次全盘扫描；
- 访问统计 `value` / `total`，网络输出 `net_policy[81]` / `net_value`；
- 父指针 `father` 与子节点数组 `children[81]`、上一手位置 `last_step`。

### 5.2 搜索流程

一次 `trainChoose(last_step)`（走一步棋）的流程：

1. `initRoot`：以当前局面建立根节点（每步**重建整棵树**），计算双方合法着法；
2. 进行 **200 次模拟**，每次模拟走完 `select → 扩展 → 回传` 三阶段：
   - **选择**：沿 PUCT 公式 `Q(s,a) + c·P(s,a)·√N(s) / (1+N(s,a))` 选择子节点（`c = 2.5`）；节点首次到达时直接以网络价值回传，再次到达时才扩展新子节点；
   - **评估**：叶子节点用网络预测（见 §6）；若当前行棋方无合法着法则为终局（该方负）；
   - **回传**：沿路径自下而上交替取反累加 `value` 与 `total`；
3. 将根节点各子节点的访问次数归一化为策略向量 `probs`；
4. 选择落子（`bestStep`）：**前 20 手内**（根节点子节点数 < 20）按访问次数比例**温度采样**（τ=1，增加开局多样性），之后选择访问次数最多的着法；
5. 搜索信息写入 `debug_info`（模拟次数、根价值、最优着法、各点策略/访问数），供 GUI 与验证框架读取。

### 5.3 合法着法扫描优化（scanAvailable）

naive 做法是每扩展一个节点就全盘 81 点 × 双方各做一次 `judgeAvailable`。由于 NoGo 中一步落子只可能影响**该点相邻位置**的合法性，优化版分两阶段（[Alphapig.cpp](SelfPlay/Alphapig.cpp) 中的 `scanAvailable`，与 [search/FastScanAvailable.h](search/FastScanAvailable.h) 同算法）：

1. **Phase 1**：仅检查 `last_step` 上下左右 4 个邻点——空点直接判定；棋块则 BFS 数气，气数为 1 时该气点即为候选点；
2. **Phase 2**：用候选点结果**过滤父节点的两份合法列表**（父节点列表中远离 `last_step` 的点合法性不变，直接继承）。

复杂度由 O(全盘扫描) 降为 O(邻域 + 列表过滤)，`benchmark/scan_benchmark.cpp` 验证其与全扫描**结果完全一致**并量化加速比。

### 5.4 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 模拟次数 | 200 / 步 | `trainChoose` 中固定 |
| PUCT 常数 `c` | 2.5 | `bestChild` 中固定 |
| 温度采样 | τ=1，前 20 手 | 之后按最大访问次数 |
| 输入增强 | 随机 8 对称变换 | 每次 `predict` 随机翻转 + 旋转，输出反变换取平均 |

---

## 6. 神经网络

### 6.1 架构（C++ 端与两个训练脚本严格一致）

| 部分 | 结构 |
|------|------|
| 输入 | 6 通道 × 9×9（`channels_first`） |
| 主干 | Conv(6→64, 3×3, BN, ReLU) → 5× 残差块 → Conv(64→64, 3×3, BN, ReLU) |
| 残差块 | Conv(3×3, BN, ReLU) → Conv(3×3, BN) → 残差相加 → ReLU |
| 价值头 | Conv(1×1, BN, ReLU) → Flatten → FC(81→64, ReLU) → FC(64→1, tanh) |
| 策略头 | Conv(1×1) → Flatten → Softmax(81) |

输入 6 通道含义（均以**当前行棋方视角**编码）：

| 通道 | 内容 |
|------|------|
| 0 | 我方棋子（值为 1 的位置） |
| 1 | 对方棋子 |
| 2 | 我方合法落子位置 |
| 3 | 对方合法落子位置 |
| 4 | 我方是否先手（整盘 0/1） |
| 5 | 对方最后一手位置 |

价值输出经 tanh 后取反（网络以当前视角输出，回传时再按路径交替取反）。

### 6.2 C++ 推理实现（SelfPlay/Network.cpp）

- 手写前向传播，卷积用 **im2col + `cblas_sgemm`**（Intel MKL）实现，1×1 卷积直接 GEMM；
- 层类型：`ConvolutionalLayer`（含 BN 与 ReLU）、`ConnectedLayer`（全连接）、`SoftmaxLayer`、`ShortcutLayer`（残差连接）；
- BN 在加载时把方差预转为标准差（`sqrt(var) + 1e-6`），推理时按 `(x - mean) / std * scale` 计算；
- 网络结构在构造函数中**硬编码**（与训练脚本一一对应），改动架构需同时改三处（Keras、PyTorch、C++）。

### 6.3 权重文件格式（network.weights）

二进制小端格式，总大小约 **1.67 MB**：

1. **头部 20 字节**：5 个 int32 占位（全 0），C++ 加载时 `seekg(20)` 跳过；
2. 每层按加载顺序依次写入（与 [Train.py](Train.py) 的 `saveModel`、[Train_torch.py](work/Train_torch.py) 的 `export_weights` 对应）：

```
每个 Conv+BN 层：  BN beta(filters) → BN gamma(filters) → BN mean(filters)
                  → BN var(filters) → Conv 权重(out×in×3×3)
每个 Dense 层：    bias(outputs) → 权重(outputs×inputs)
```

权重数组均按 Keras 格式（`(kh,kw,in,out)`）转置为 C++ 期望的 `(out,in,kh,kw)` 顺序存储。

### 6.4 Botzone 版推理（Botzone.cpp）

Botzone.cpp 为竞赛平台定制的**单文件自包含版本**，与引擎版的差异：

- 用 `jsoncpp` 通过 stdin/stdout 交换 JSON（Botzone 协议），权重默认从 `data/nogo.weights` 加载；
- 不依赖 MKL，改为**手写 AVX-256 SIMD GEMM**（`avx256_noncblas_sgemm`），层结构与权重格式与引擎版相同；
- `AlphaPig` 类为独立副本，接口略有差异（`choose()`、`steps`、`mcts_value`）。

---

## 7. 训练系统（work/）

### 7.1 自对弈数据生成（SelfPlay.exe）

- OpenMP 并行执行 **10000 局**，每局每线程独立创建 `Network`（即每局重新加载一次权重）；
- 每局两个 `AlphaPig` 实例共享同一网络，棋色互为镜像，**双方每走一步都记录一条训练样本**；
- 样本格式（每行 6×81 + 81 + 1 = 568 个浮点数）：
  `6 通道输入(486) + MCTS 策略概率(81) + 胜负价值(±1)`；
- 终局价值回填：胜方所有样本 `z=+1`，负方 `z=-1`；
- 全部样本合并写入 `dataset/<timestamp>.txt`。

### 7.2 训练脚本

两个脚本流程相同：加载检查点（不存在则新建并导出初始权重）→ 读取 `dataset/` → **随机采样 256,000 条** → 逐条做随机 8 对称增强 → 训练 → 保存检查点并导出 `network.weights`。

| 项目 | Train.py（Keras, CPU） | Train_torch.py（PyTorch, GPU） |
|------|------------------------|--------------------------------|
| 检查点 | `model.keras` | `model_torch.pt` |
| 轮数 | 4 epoch × batch 256 | 4 epoch × batch 256 |
| 优化器 | SGD lr=0.01, momentum=0.9 | SGD lr=0.01, momentum=0.9, weight_decay=1e-4 |
| 损失 | 0.5·MSE(价值) + 0.5·CCE(策略) | 0.5·MSE(价值) + 0.5·CCE(策略) |
| 设备 | CPU（TensorFlow） | CUDA（自动回退 CPU） |

### 7.3 训练循环脚本

```powershell
# CPU 训练循环（SelfPlay → Keras 训练，无限循环）
.\Trainloop.ps1

# GPU 训练循环（SelfPlay → PyTorch 训练，无限循环，推荐）
.\Trainloop_gpu.ps1
```

注意：`dataset/` 为空时训练脚本会**跳过训练直接退出**（不报错），因此必须先运行一次 `SelfPlay.exe` 生成数据。

---

## 8. GUI 人机对战（gui/）

### 8.1 模块职责

| 模块 | 职责 |
|------|------|
| `main.cpp` | 窗口与事件循环、信息面板绘制（玩家/搜索信息/策略 Top10/价值历史/调试） |
| `BoardRenderer` | 棋盘网格、星位、棋子、上一手标记、悬停提示、访问次数热力图 |
| `GameController` | 对局状态机：轮流落子、合法性检查、胜负判定（无合法着法者负）、重置 |
| `EngineBridge` | RAII 持有 `Network` + `AlphaPig`，棋盘状态的**唯一事实来源**，封装规则与搜索 |
| `SearchInfo` | 一次搜索的调试信息（playouts、耗时、根价值、最优着法、各点策略/访问数） |

### 8.2 线程模型

- 玩家（黑棋）点击落子后，AI 的 MCTS 搜索在**独立后台线程**执行（`computeAIMove` 只读棋盘，不修改状态），避免 UI 冻结；
- 搜索完成后结果经 `std::atomic<int>` 传回，**在主线程**应用落子，避免竞态。

### 8.3 操作说明

| 操作 | 功能 |
|------|------|
| 鼠标左键 | 落子（仅黑棋回合有效，非法着法有提示） |
| `R` | 新游戏 |
| `H` | 切换热力图显示（子节点访问次数） |
| `D` | 调试模式（打印棋盘矩阵、合法着法数、上一手位置） |
| `Esc` | 退出 |

---

## 9. 验证与实验框架

### 9.1 SelfPlayValidate.exe

验证框架入口（[SelfPlayValidate.cpp](SelfPlay/SelfPlayValidate.cpp)）：读取配置文件 → OpenMP 并行自对弈 N 局 → 通过 `PerformanceLogger` 记录每步性能 → 输出 CSV 及逐局/总体统计。

```powershell
cd gui\build\Release
.\SelfPlayValidate.exe ..\..\..\configs\baseline.txt
```

### 9.2 配置文件格式（configs/）

key=value 文本（支持 `#` 注释），由 [tools/Config.h](tools/Config.h) 解析：

| 键 | 含义 |
|----|------|
| `mcts.tree_reuse` | 是否启用树复用（见 §9.5） |
| `mcts.playouts` / `mcts.c_puct` | 搜索参数（当前实现中为参考值） |
| `test.num_games` / `test.seed` | 对局数与随机种子 |
| `test.output` | CSV 输出路径（相对运行目录） |
| `model.weight_file` | 权重文件路径 |

现成配置：`baseline.txt`（100 局，复用关）、`tree_reuse.txt`（100 局，复用开）、`quick_baseline/quick_reuse`（10 局快速冒烟）、`full_baseline.txt`（500 局）。

### 9.3 性能日志 CSV（PerformanceLogger）

每行记录一次走子：`game_id, move, time_ms, playouts, root_visits, reused, reused_visits, new_nodes, root_value, best_move, total_moves, winner`；文件尾部附逐局摘要与总体摘要（平均耗时、平均模拟数、复用率等，以 `#` 注释行表示）。

### 9.4 一键实验与对比报告

```powershell
powershell .\tools\run_experiment.ps1 -Games 500
```

自动完成：创建 `results/<时间戳>/` → 生成 baseline / tree_reuse 两组临时配置 → 依次运行 `SelfPlayValidate.exe` → 调用 `compare_results.py` 生成 `comparison.md` 对比报告。

### 9.5 树复用（Tree Reuse）现状

- 配置文件与日志字段已为树复用预留（`mcts.tree_reuse`、`reused_visits`、`new_nodes` 等），但当前 `SelfPlayValidate` 实现中记录仍为固定值（未真正启用复用），`trainChoose` 每步重建整棵树；
- `gui/test_tree_reuse.cpp` 提供 6 组复用逻辑单元测试（子节点提升为根、统计保持、禁用复用、对局重置、完整周期等），作为后续实现复用时的回归基线；
- `tools/run_experiment.ps1` 即为树复用上线前后做 A/B 对比的实验模板。

### 9.6 其他基准与测试

- `benchmark/scan_benchmark.cpp`：scanAvailable **正确性**（10000 个随机局面与全扫描对照）+ **性能**（各 10 万次调用计时，输出加速比），失败返回码 1；
- `gui/test_tree_reuse.cpp`：编译方式见文件头注释（MSVC `cl` + oneMKL 静态库链接）。

---

## 10. 构建指南

### 10.1 依赖

| 依赖 | 说明 |
|------|------|
| Visual Studio 2022 | C++20 工具链 |
| Intel oneAPI（含 MKL） | 引擎推理加速；SelfPlay 需 Intel C++ Compiler 2025（icx） |
| SFML 2.6.2 | GUI 库，安装在 `C:/SFML-2.6.2` |
| Python 3 | 训练：`tensorflow`+`keras`（CPU）或 `torch`+`numpy`（GPU，推荐） |
| CMake ≥ 3.20 | GUI 构建 |

### 10.2 构建命令

**GUI + SelfPlayValidate（CMake，MSVC）**：

```powershell
cd gui\build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

产物输出到 `gui\build\Release\`，SFML DLL 由 POST_BUILD 自动复制；`/utf-8` 编译选项已配置。

**SelfPlay.exe（Visual Studio / Intel 编译器）**：

用 VS2022 打开 `SelfPlay\SelfPlay.sln` 构建即可（工程已配置 Intel C++ Compiler 2025 + oneMKL Sequential + OpenMP），产物复制到 `work\` 使用。

**其他工具**（在 Intel oneAPI 或 MSVC 环境下手动编译）：

```powershell
# 树复用单元测试
cl /EHsc /std:c++20 /I.. /I../SelfPlay test_tree_reuse.cpp /link mkl_intel_lp64.lib mkl_sequential.lib mkl_core.lib

# scanAvailable 基准测试（benchmark/ 目录下，方式同上，含 ../SelfPlay 头文件路径）
```

---

## 11. 运行指南

### 11.1 首次准备

1. 确认权重文件就位（见 §3.2 表格）：`work\network.weights` 与 `gui\build\Release\network.weights`；
2. 确认 `work\dataset\` 存在（不存在则由 `SelfPlay.exe` 首次运行创建）。

### 11.2 训练（GPU 推荐）

```powershell
cd work
.\Trainloop_gpu.ps1    # SelfPlay.exe → Train_torch.py 无限循环
# 或单次执行：
.\SelfPlay.exe          # 生成 dataset/<timestamp>.txt
python .\Train_torch.py # 训练并导出 network.weights
```

训练完成后将新权重同步到 GUI/验证目录：

```powershell
Copy-Item work\network.weights gui\build\Release\network.weights
```

### 11.3 人机对战

```powershell
cd gui\build\Release
.\AlphaPigGUI.exe [权重文件路径]   # 参数可选，默认当前目录 network.weights
```

### 11.4 验证与实验

```powershell
cd gui\build\Release
.\SelfPlayValidate.exe ..\..\..\configs\baseline.txt        # 单组验证
powershell ..\..\..\tools\run_experiment.ps1 -Games 500     # 完整 A/B 实验（从项目根运行）
python ..\..\..\tools\compare_results.py a.csv b.csv        # 手动生成对比报告
```

---

## 12. 注意事项与已知问题

1. **源码编码混杂**：`SelfPlay\Alphapig.cpp`、`Network.cpp` 为 GBK 编码（中文注释），`SelfPlay.cpp`、`SelfPlayValidate.cpp`、`gui\` 为 UTF-8。GUI 的 CMake 已加 `/utf-8`；自行用其他方式编译时需注意中文注释的编码处理，建议统一转为 UTF-8。
2. **相对路径依赖**：所有程序按工作目录解析 `network.weights`、`dataset/`、`results/`、字体等相对路径，务必按 §11 指定目录运行；GUI 可通过命令行参数显式指定权重路径。
3. **训练前置条件**：`dataset/` 为空时训练脚本静默跳过（`No training data found. Skipping training.`），并非报错。
4. **权重同步**：GUI 与验证程序用的是各自目录下的权重副本，训练后不复制会继续使用旧网络。
5. **网络结构硬编码**：C++ 加载器、Keras 脚本、PyTorch 脚本三处需同步修改（§6.3）。
6. **每局重新加载权重**：`SelfPlay.exe` 每局构建一次 `Network`（约 1.67 MB 文件 × 10000 局），属可优化点。
7. **树复用未启用**：验证框架中 `mcts.tree_reuse` 仅为预留配置（§9.5）。
8. **仓库冗余**：项目根目录残留旧版 `Train.py`/`Trainloop.ps1`/模型文件的副本，现行工作流一律以 `work\` 为准；`work\` 内亦有 `network.weights.keras`/`network.weights.torch` 等历史备份，注意区分。

---

## 13. 参考链接

- [README.md](README.md) —— 面向使用的快速开始
- 版本历史：`git log`（v1 初始提交 `23237a3`，v2.0 提交 `03492a0`）
