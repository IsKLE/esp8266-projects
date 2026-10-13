# ESP8266 项目集

本仓库包含 ESP8266 Arduino 示例、网络中继固件、电脑传感器监控程序和相关辅助工具。

## 项目内容

| 目录 | 内容 |
| --- | --- |
| `HelloWorld` | 探测 SSD1306 OLED 的 I²C 接线并显示问候文字和运行时间。 |
| `ImageDisplay` | 在 128×64 OLED 上显示由图片转换得到的单色位图。 |
| `NatRelay` | 建立 Wi-Fi 热点，为客户端提供 IPv4 NAT、入站过滤和 OLED 状态页。 |
| `PCStats` | 接收电脑端传感器数据并在 OLED 上显示。 |
| `PCMonitor` | Windows 桌面程序，读取 CPU/GPU 传感器并通过 USB 串口发送给 `PCStats`。 |
| `tools` | 图片编码、串口监控、NAT 配置和协议辅助工具。 |

## 使用

需要安装 Arduino CLI 或 Arduino IDE、ESP8266 Arduino Core，以及 Adafruit SSD1306、Adafruit GFX 和 Adafruit BusIO 库。命令行构建还需要 PowerShell。每个 sketch 的名称与其目录名相同。

在项目根目录打开 PowerShell，编译或上传示例：

```powershell
.\build.ps1 -Sketch HelloWorld
.\build.ps1 -Sketch HelloWorld -Upload -Port COM3
```

将 `HelloWorld` 替换为 `ImageDisplay`、`NatRelay` 或 `PCStats` 可选择其他固件。串口号按当前连接的开发板调整。

电脑传感器监控的使用方法见 [PCMonitor/README.md](PCMonitor/README.md)。NAT 热点配置、使用和网络行为见 [NatRelay/README.md](NatRelay/README.md)。

转换 OLED 图片并上传：

```powershell
python .\tools\prepare_oled_image.py .\assets\oled-source.png .\ImageDisplay\oled-image.h .\assets\oled-preview.png
.\build.ps1 -Sketch ImageDisplay -Upload -Port COM3
```

图片转换工具需要 Python 和 Pillow。NAT 配置工具需要 Windows、Python 和 pyserial；首次使用前，请按 NAT 使用说明设置上游 Wi-Fi 配置。

## 第三方开源代码

- ESP8266 Arduino Core 3.1.2：[源码与许可](https://github.com/esp8266/Arduino/tree/3.1.2)
- Adafruit SSD1306 2.5.17：[源码与许可](https://github.com/adafruit/Adafruit_SSD1306/tree/2.5.17)
- Adafruit GFX 1.12.6：[源码与许可](https://github.com/adafruit/Adafruit-GFX-Library/tree/1.12.6)
- Adafruit BusIO 1.17.4：[源码与许可](https://github.com/adafruit/Adafruit_BusIO/tree/1.17.4)
- LibreHardwareMonitorLib、System.IO.Ports 和 PawnIO 的来源与许可见 [PCMonitor/THIRD-PARTY-NOTICES.md](PCMonitor/THIRD-PARTY-NOTICES.md)。
