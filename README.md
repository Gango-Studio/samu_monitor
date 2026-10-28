# samu_monitor

> 为树莓派Raspberry Pi Pico开发的模仿了1970年代末早期风格的内存管理程序以及内置简化的8位虚拟CPU
> (但由于pico硬件限制 本程序无法像当年的内存管理程序一样通电直接启动，需要Pico ROM 内的BootLoader启动跳转至位于Flash后运行)  
## 功能特性
- **USB CDC 串口**：通过USB串口交互
- **同时支持 UART**：GP0/GP1，115200
- **交互式内存操作**：读单字节、成段转储、批量写入
- **可执行程序**：内置的8位虚拟CPU从任意地址开始执行，带步数上限防止死循环
- **逐字符回显**：支持退格

## 硬件要求

| 项目 | 要求 |
|---|---|
| 主控 | RP2040（Pico 1 / Pico W / 各类 RP2040 兼容板） |
| Flash | 2 MB（固件约 78 KB） |
| 连接 | USB 数据线（Type-C 或 micro-USB，视板子而定） |
| 调试 | 可选，串口输出需要 USB-TTL 或直接用 USB CDC |


## 编译

### 依赖

| 工具 | 版本 | 说明 |
|---|---|---|
| Pico SDK | 2.x（验证于 2.3.1） | 需要初始化 `lib/tinyusb` 子模块，USB 串口依赖它 |
| Arm GNU Toolchain | 13.2+（验证于 15.2.Rel1） | 必须是 `arm-none-eabi`，**不是** `aarch64-none-elf` |
| CMake | 3.13+ | |
| Ninja | 任意 | 可选，也可换 Make |

SDK 里缺少 tinyusb 时，USB 串口会被链接器丢掉（固件能跑但没有 COM 口），先补上：

```bash
cd $PICO_SDK_PATH && git submodule update --init lib/tinyusb
```

### 构建

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_TOOLCHAIN_PATH=/path/to/arm-none-eabi

cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPICO_BOARD=pico
cmake --build build --parallel
```

产物：

| 文件 | 用途 |
|---|---|
| `build/samu_monitor.uf2` | **烧录用** |
| `build/samu_monitor.elf` | 调试用 |
| `build/samu_monitor.bin` / `.hex` | 其它烧写方式 |

Windows 下建议包一层批处理，并临时目录指到项目内（某些环境 gcc 写系统 TEMP 会失败）：

```bat
set "TMP=%~dp0tmp"
set "TEMP=%~dp0tmp"
set "PICO_SDK_PATH=C:\path\to\pico-sdk"
set "PICO_TOOLCHAIN_PATH=C:\path\to\arm-none-eabi"
set "PATH=%PICO_TOOLCHAIN_PATH%\bin;%PATH%"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPICO_BOARD=pico
cmake --build build --parallel
```

## 烧录

1. **拔掉 USB**
2. **按住 BOOTSEL 键**，再插上 USB，保持 2 秒后松开
3. 电脑出现名为 `RPI-RP2` 的 U 盘（约 127 MB）
4. 把 `samu_monitor.uf2` 拖进它的根目录
5. 板子自动重启，U 盘消失即完成

## 使用

刷完后**正常插 USB**，系统会出现一个串口：

- Windows：设备管理器 → 端口（COM 和 LPT）
- Linux：`/dev/ttyACM0`
- macOS：`/dev/cu.usbmodem*`

用任意串口终端连接（PuTTY / MobaXterm / VS Code Serial Monitor / screen / minicom）：

```bash
screen /dev/ttyACM0 115200
```

连上后先看到开机横幅（只在上电时打印一次）：

```
=====Samu Monitor=====
```

Windows平台也可以直接使用samu.cmd串口终端脚本

```
.\samu.cmd
```

### 命令

| 命令 | 说明 | 示例 |
|---|---|---|
| `ADDR` | 读该地址 1 个字节 | `100` → `0x0100: 0x00` |
| `START.END` | 转储一段内存 | `100.17F` |
| `ADDR B1 B2 ...` | 从该地址写入（单次最多 256 字节） | `100 04 48 08` |
| `ADDRR` | 从该地址开始执行 | `100R` |

- 地址和字节都是**十六进制**，`0x` 前缀可写可不写
- 单次执行最多 1000 步，防止死循环卡住终端

### 内置虚拟机指令集

| 操作码 | 助记符 | 长度 | 语义 |
|---|---|---|---|
| `00` | NOP | 1 | 空操作 |
| `01` | HALT | 1 | 停机 |
| `02 HH LL` | LDA addr | 3 | A = MEM[HHLL] |
| `03 HH LL` | STA addr | 3 | MEM[HHLL] = A |
| `04 NN` | LDA #imm | 2 | A = NN |
| `05 NN` | ADD #imm | 2 | A = (A + NN) & 0xFF |
| `06 HH LL` | JMP addr | 3 | 无条件跳转 |
| `07 HH LL` | JZ addr | 3 | A 为 0 时跳转 |
| `08` | OUT | 1 | 输出 A 对应的字符 |

寄存器与状态：

- `A`：8 位累加器
- `PC`：16 位程序计数器
- `Z`：零标志，由 `LDA` 和 `ADD` 更新
- 内存：65536 字节，地址 `0x0000` ~ `0xFFFF`
