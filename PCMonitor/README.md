# ESP8266 电脑监控

`PCMonitor` 是 Windows 桌面程序，读取 CPU/GPU 温度、CPU/GPU 使用率和物理内存占用率，通过 USB 串口发送给 `PCStats` 固件。开发板 OLED 显示温度和使用率。

## 使用

需要 Windows、.NET Windows Desktop 9、LibreHardwareMonitorLib，以及 PawnIO CPU 访问驱动。CPU 温度默认读取 CPU Package，GPU 温度默认读取 GPU Core；这些读数可能与核心最高温度或 GPU 热点温度不同。程序只读取传感器，不设置风扇、功耗或频率。

1. 使用 `publish-monitor.ps1` 发布桌面程序。
2. 使用项目根目录中的构建脚本将 `PCStats` 上传到 ESP8266 开发板。
3. 启动 `PCMonitor`，选择开发板对应的串口并开始显示。

```powershell
.\publish-monitor.ps1
.\build.ps1 -Sketch PCStats -Upload -Port COM3
```

停止显示会释放串口；退出会结束程序。传感器数据超过 5 秒无更新时，屏幕清除温度并显示 `PC OFFLINE`。串口断开后程序会继续尝试连接所选端口；端口改变时，请停止显示并重新选择端口。

## 串口协议

115200 波特，8N1，ASCII，每行以换行符结尾：

```text
PC1,序号,CPU温度乘10,GPU温度乘10,CPU使用率,GPU使用率,物理内存占用率
ACK1,序号,CPU温度乘10,GPU温度乘10,CPU使用率,GPU使用率,物理内存占用率
```

温度不可用时编码为 `-999`，百分比不可用时编码为 `-1`。电脑端只将完全匹配当前数据的 `ACK1` 视为成功。固件会验证整行字段、范围和长度，再更新画面并应答。

## 第三方开源代码

- LibreHardwareMonitorLib 0.9.6：MPL-2.0；[源码与许可](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/tree/v0.9.6)。
- System.IO.Ports：Microsoft MIT License；[源码与许可](https://github.com/dotnet/runtime)。
- PawnIO 2.2.0：使用[官方安装器](https://github.com/namazso/PawnIO.Setup/releases/tag/2.2.0)，单独安装系统驱动。
- 固件使用的 Adafruit SSD1306、Adafruit GFX 和 Adafruit BusIO 来源见仓库根目录 [README](../README.md)。

详细许可说明见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。
