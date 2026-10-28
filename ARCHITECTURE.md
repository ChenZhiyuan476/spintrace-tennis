# SpinTrace Architecture

当前结构采用“一个稳定的 `Game` 门面，多个按职责拆分的实现文件”。游戏保持 120 Hz 固定更新，球拍碰撞、飞行和弹跳等纯物理逻辑独立于输入、规则与表现层。

## Runtime flow

```text
main.cpp
  ├─ 收集并换算 SDL 输入坐标
  ├─ 以 120 Hz 调用 Game::update
  └─ 每个显示帧调用 Game::render

Game
  ├─ game_input.cpp       SDL输入适配、挥拍轨迹、玩家击球
  ├─ game.cpp             比赛编排、AI、规则与计分
  ├─ game_menu.cpp        暂停菜单和设置修改
  └─ game_render.cpp      球场、HUD、轨迹历史和暂停界面
                                │
                                └─ render_primitives.cpp
                                   SDL像素绘制基础工具

Independent gameplay data/systems
  ├─ match_state.hpp          可序列化比赛快照
  ├─ player_input.hpp         平台无关移动与挥拍命令
  ├─ ball_physics.hpp/.cpp    球拍碰撞、来球继承、发射、空气受力、旋转、预测和弹跳
  ├─ gameplay_config.hpp/.cpp 玩家成长、接触区、握拍、外观和场地主题
  ├─ progression_system.*     教学关卡、技巧解锁和成长奖励
  └─ scoring_system.*         分、局、盘与比赛胜负推进
```

## Extension boundaries

- 音效：新增 `audio_system.hpp/.cpp`，由击球、触网、落地和得分事件触发，不应写进绘制代码。
- 教学与关卡：`ProgressionSystem` 和 `LessonStage` 作为未来扩展保留，但当前启动和比赛流程不加载关卡档案。
- 玩家风格：`PlayerProfile` 保存握拍、挥拍动作、能力、击球区和小球外观；默认值保证自由模式手感不变。
- 场地风格：`CourtTheme` 保存球场、外场、观众席和观众配色，不进入比赛模拟。
- 联机：先把每帧输入整理为 `InputCommand`，把比赛数据整理为可序列化的 `MatchSnapshot`，网络模块只传递命令和快照。
- 新视觉风格：扩展 `render_primitives` 或增加新的 renderer；比赛模拟不应依赖具体绘制方法。
- 新输入设备：键鼠、手柄和触屏先统一转换为同一种挥拍/移动命令，再交给模拟层。

## Dependency rule

表现层可以读取 `MatchState`，但物理系统不调用表现层。`BallPhysicsSystem` 不依赖SDL、玩家、计分或菜单。平台入口负责SDL生命周期，并把键鼠转换为 `PlayerInputCommand`/`StrokeInputCommand`。未来音效和网络应消费比赛事件与快照，而不是直接操作绘制代码。

`tests/core_tests.cpp` 只链接物理、配置、进度和记分模块，不链接SDL。这保证核心系统可以在服务器、回放工具或其他平台独立运行，并检查相同初始状态与输入得到相同物理结果；球拍碰撞测试还会验证来球借力、反向旋转继承和三维速度上限。
