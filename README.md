# ESP8266 / ESP-12F 开发项目

## 当前运行：NAT 中继与网络状态屏

当前固件为 `NatRelay/NatRelay.ino`，连接用户配置的 2.4 GHz Wi-Fi 并提供 **ESP8266-NAT** 热点，建立 `192.168.4.0/24` 子网。主网直接访问子网的包和主动入站 TCP 连接被过滤；子网主动联网的 NAT 响应可以返回。OLED 每秒显示网络状态，配置保存后重新供电即可运行。

桌面的 **ESP8266-NAT 热点密码** 可查看热点密码。使用说明与验证范围见 [NatRelay/README.md](NatRelay/README.md)，测试证据在 `evidence/nat-relay`。

正常运行时短按并松开 **FLASH**，可轮换网络信息、热点密码、性能三页。性能页显示内存余量、CPU 频率和当前/峰值上行、下行速率；切页期间中继持续运行。RESET 保留复位用途。

三页均采用 64×128 竖向布局，已去掉 `FLASH: next` 提示。

NAT 固件使用 160 MHz CPU 频率，构建脚本已固定该选项；其他示例保留原有 80 MHz 默认值。

## 电脑温度监控（已保留）

监控固件为 `PCStats/PCStats.ino`，电脑端软件为 `PCMonitor`，桌面快捷方式是 **ESP8266 电脑监控**。它通过 USB 串口每秒发送 CPU/GPU 温度、CPU/GPU 使用率和物理内存占用率。切换 NAT 时已退出托盘程序；恢复时先刷回 PCStats，再打开监控软件。

完整使用说明见 [PCMonitor/README.md](PCMonitor/README.md)。实机完成 10 次真实数据采集及匹配应答、9 项固件协议检查；用户已确认 OLED 正常显示并持续变化。当前验证记录位于 `evidence/pc-monitor\verification.json`。

## 图片示例（已保留）

图片示例是 `ImageDisplay/ImageDisplay.ino`。它将用户提供的图片按比例缩放为 95×64，居中放到 128×64 OLED 上。头发使用纯白色块，面部为黑色，保留眼睛等轮廓；不叠加文字。

- 原图：`assets/oled-source.png`。
- 实际 128×64 位图预览：`assets/oled-preview.png`；放大预览：`assets/oled-preview-4x.png`。
- 固件使用的 1024 字节位图：`ImageDisplay/oled-image.h`，存放在 Flash 中。
- 编译上传：`.\build.ps1 -Sketch ImageDisplay -Upload -Port COM3`。
- 恢复 Hello World：`.\build.ps1 -Sketch HelloWorld -Upload -Port COM3`。

更换图片时，使用已安装的 Pillow 编码工具生成头文件，再编译上传：

```powershell
& 'python' .\tools\prepare_oled_image.py .\assets\oled-source.png .\ImageDisplay\oled-image.h .\assets\oled-preview.png
.\build.ps1 -Sketch ImageDisplay -Upload -Port COM3
```

默认采用灰度阈值 200，使原图的头发显示为纯色；可用 `--threshold` 调整分界，或用 `--mode dither` 改为抖动灰度效果。

实机验证：图片程序编译、刷入和写入校验通过；6 秒串口捕获收到 7 条图片状态及 OLED 应答记录。用户已目视确认图片显示、头像可辨认。编码后的 1024 字节与预览图逐字节一致。记录位于 `evidence/image-display\verification.json`。

## 已安装的开发环境

| 项目 | 版本 / 位置 |
| --- | --- |
| Arduino IDE | 2.3.10，`<Arduino IDE 安装目录>`，桌面有 Arduino IDE 快捷方式 |
| Arduino CLI | 1.5.1，`arduino-cli` |
| ESP8266 Arduino Core | 3.1.2 |
| OLED 显示库 | Adafruit SSD1306 2.5.17、Adafruit GFX 1.12.6、Adafruit BusIO 1.17.4 |
| CH340 串口驱动 | WCH 3.9.2024.9，Windows 已识别为 COM3 |
| 环境配置 | `<Arduino CLI 配置文件>` |
| 编译输出 | `build\HelloWorld` |
| 包下载和编译缓存 | `<Arduino 包缓存目录>` |

USB 实测芯片为 ESP8266EX，晶振 26 MHz，Flash 4 MB。串口号在换 USB 口后可能改变。

## 示例行为

程序探测板载 0.96 英寸 SSD1306 OLED 的 I²C 接线和地址，显示两行大字 `Hello`、`World`，底部每秒更新运行时间。同时通过串口输出 `Hello World` 和屏幕通信信息。串口参数：115200，8N1。

启动时依次探测 SDA/SCL GPIO 组合 `14/12`、`4/5`、`12/14`，地址 `0x3C` 和 `0x3D`。串口记录实际找到的接线；若没有设备应答，会打印 `OLED not detected on tested I2C pins`。显示库使用已探测的接线，不会重新切换到默认 I²C 引脚。

本开发板已实测：**SDA=GPIO14、SCL=GPIO12、I²C 地址=0x3C，128×64 SSD1306 OLED**。

## 图形界面开发

1. 打开桌面的 Arduino IDE 快捷方式；它会打开 `HelloWorld\HelloWorld.ino`。
2. 在开发板菜单选择 **Generic ESP8266 Module**，端口选择 **COM3**（若端口变化，以设备管理器为准）。
3. CPU Frequency 选 **80 MHz**；Flash Mode 选 **DOUT (compatible)**；Flash Size 选 **4MB (FS:1MB OTA:~1019KB)**；Reset Method 选 **dtr (aka nodemcu)**；Upload Speed 选 **115200**。
4. 点击上传。上传完成后打开串口监视器，波特率选 **115200**。

IDE 和命令行可共用同一套核心、工具和包缓存；IDE 的小型配置文件位于 `%USERPROFILE%\.arduinoIDE\arduino-cli.yaml`。

## 命令行开发

在本项目目录打开 PowerShell：

```powershell
# 编译
.\build.ps1

# 编译并上传到指定开发板
.\build.ps1 -Upload -Port COM3

# 读取 15 秒串口输出，结束后自动释放串口
.\monitor.ps1 -Port COM3
```

上传之前关闭占用该端口的串口监视器。脚本使用明确的工具路径，无需修改全局 PATH。

## 官方来源

- [Arduino CLI 安装文档](https://docs.arduino.cc/arduino-cli/installation/)
- [Arduino IDE 安装文档](https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE)
- [ESP8266 Core 安装文档](https://arduino-esp8266.readthedocs.io/en/latest/installing.html)
- [WCH 官方 CH341 驱动发布包](https://github.com/WCH-IC/download/releases/tag/CH341)
- [Adafruit SSD1306 官方显示库](https://github.com/adafruit/Adafruit_SSD1306)

下载的 Arduino CLI、IDE 和驱动包均核对了发布资产 SHA-256；安装的驱动目录签名为 Microsoft Windows Hardware Compatibility Publisher。

## 本次实机验证（2026-10-13）

- 屏幕版编译成功，固件大小 284160 字节。
- 在 COM3 完成上传，工具确认 `Hash of data verified.`，并自动复位开发板。
- 使用本项目的 `monitor.ps1` 读取实际串口输出，8 秒内收到 8 条 `Hello World` 和 8 条屏幕刷新记录；地址及引脚与实测一致。
- 用户目视确认屏幕显示 `Hello`、`World`，底部运行时间持续变化。
- 验证记录、编译日志、上传日志和串口输出位于 `evidence`。
- 刷入前已完整读取原有 4 MB Flash，备份保存在本机备份目录，校验值见验证记录 `verification.json`。
- 最初的串口版验证记录保留为 `verification-serial-only.json`；`verification.json` 对应 Hello World 屏幕版。图片程序的验证记录在 `evidence\image-display` 子目录，当前电脑状态屏记录在 `evidence\pc-monitor` 子目录。
