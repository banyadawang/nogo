# AlphaPig

基于神经网络 + MCTS 的不围棋（NoGo）AI，AlphaZero 风格架构。

## 项目结构

```
AlphaPig/
├── SelfPlay/            # C++ 引擎 (MCTS + Network + NoGo规则)
│   ├── Alphapig.cpp     # AlphaPig 类：MCTS搜索、棋盘规则
│   ├── Network.cpp      # 神经网络推理 (Intel MKL / AVX-256)
│   ├── SelfPlay.cpp     # 自对弈训练入口
│   └── SelfPlay.sln     # Visual Studio 项目
├── gui/                 # SFML 可视化 GUI
│   ├── main.cpp         # 主窗口 (900×700，人机对战)
│   ├── BoardRenderer.*  # 9×9棋盘 + 棋子 + 热力图渲染
│   ├── GameController.* # 游戏状态管理
│   ├── EngineBridge.*   # 引擎接口封装
│   └── SearchInfo.h     # MCTS 搜索调试信息
├── tools/               # 开发和测试工具
│   ├── Config.h         # 配置读取器
│   ├── PerformanceLogger.h  # 性能日志记录
│   ├── compare_results.py   # 实验结果比较
│   └── run_experiment.ps1   # 自动实验脚本
├── configs/             # 测试/训练配置
├── benchmark/           # 性能测试
├── search/              # 搜索算法优化
├── Train.py             # Keras CPU 训练脚本
├── Trainloop.ps1        # CPU 训练循环
└── Botzone.cpp          # Botzone 竞赛接口
```

## 运行环境

- [Visual Studio 2022](https://visualstudio.microsoft.com/zh-hans/)
- [Intel oneAPI Toolkits](https://www.intel.com/content/www/us/en/developer/tools/oneapi/toolkits.html) (MKL)
- [Python 3](https://www.python.org/)
- [PyTorch](https://pytorch.org/) (GPU 训练，推荐)
- [SFML 2.6](https://www.sfml-dev.org/) (GUI)

## 快速开始

### 人机对战 (GUI)

```powershell
cd gui\build\Release
.\AlphaPigGUI.exe
```

- 鼠标点击落子（黑棋）
- AI 自动回应（白棋）
- `R` 新游戏 | `H` 热力图 | `D` 调试 | `Esc` 退出

### 训练

```powershell
cd work
.\Trainloop_gpu.ps1    # GPU训练 (推荐)
# 或
.\Trainloop.ps1        # CPU训练
```

### 编译

```powershell
& "C:\Program Files (x86)\Intel\oneAPI\setvars.bat"
cd gui\build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## 神经网络架构

- 输入: 6 通道 × 9×9
- 5 个残差块 (Conv 64×3×3 → BN → ReLU)
- 价值头: Conv1×1 → FC(81→64) → FC(64→1, tanh)
- 策略头: Conv1×1 → Softmax(81)
- 优化器: SGD (lr=0.01, momentum=0.9, weight_decay=1e-4)
- 训练: GPU (PyTorch) 或 CPU (Keras/TensorFlow)

## MCTS 参数

| 参数 | 值 |
|------|-----|
| 模拟次数 | 200/步 |
| PUCT 常数 | 2.5 |
| 棋盘大小 | 9×9 |

## 版本

v2.0 — GUI + GPU 训练 + 性能验证框架
