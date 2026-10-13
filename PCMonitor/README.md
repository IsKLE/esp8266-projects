# ESP8266 电脑监控

电脑端程序读取本机传感器，通过 USB 串口发送数据给 `PCStats` 固件。OLED 显示 CPU/GPU 温度，底部显示 CPU/GPU 使用率和物理内存占用。

## 使用

1. 双击桌面的 **ESP8266 电脑监控**，接受 Windows 管理员授权。CPU 温度传感器访问需要此权限。
2. 开发板连接 USB，选择对应端口。本机当前为 **COM3**。
3. 程序启动后自动开始更新；“停止显示”释放串口，“退出”结束程序。
4. 关闭窗口或点击“缩到托盘”后继续工作。双击托盘图标可以重新打开，托盘菜单可以退出。

OLED 上 `C` 是 CPU 使用率，`G` 是 GPU 使用率，`R` 是系统物理内存占用率。温度使用摄氏度：CPU 默认读取 **CPU Package**，GPU 默认读取 **GPU Core**。这两项与核心最高温度、GPU 热点温度可能不同。

缺失的传感器显示 `--`。超过 5 秒没有有效电脑数据时，屏幕清除温度并显示 `PC OFFLINE`。USB 重新插入后，程序会继续尝试连接所选串口；端口编号变化时先停止显示，再刷新、选择新端口。

## 文件位置

| 项目 | 位置 |
| --- | --- |
| 可运行程序 | `<项目目录>\PCMonitor\app\ESP8266-PCMonitor.exe` |
| 程序最新数据 / 错误日志 | `%LOCALAPPDATA%\ESP8266\PCMonitor\latest.json` / `last-error.json` |
| 编译缓存 | `build\PCMonitor\obj` 和 `bin` |
| NuGet 包缓存 | `%NUGET_PACKAGES%` |
| 原版 PawnIO 安装包 | `<PawnIO 安装目录>\PawnIO_setup.exe` |
| 实机验证记录 | `evidence/pc-monitor` |

程序基于本机已安装的 **.NET Windows Desktop 9**。CPU/GPU 传感器库为 **LibreHardwareMonitorLib 0.9.6**；CPU 访问使用 **PawnIO 2.2.0**。PawnIO 是独立系统驱动，由原版安装器安装到系统要求的位置。程序仅读取传感器，不设置风扇、功耗或频率。

## 开发与恢复

在项目根目录执行：

```powershell
# 编译电脑端程序
.\publish-monitor.ps1

# 刷入电脑状态屏固件
.\build.ps1 -Sketch PCStats -Upload -Port COM3

# 恢复之前的图片或 Hello World
.\build.ps1 -Sketch ImageDisplay -Upload -Port COM3
.\build.ps1 -Sketch HelloWorld -Upload -Port COM3
```

上传之前退出电脑监控程序，释放串口。之前的图片、Hello World 源码和固件备份均保留。

## 串口协议

115200，8N1，ASCII，每行以换行符结尾：

```text
PC1,序号,CPU温度乘10,GPU温度乘10,CPU使用率,GPU使用率,物理内存占用率
ACK1,序号,CPU温度乘10,GPU温度乘10,CPU使用率,GPU使用率,物理内存占用率
```

温度不可用编码为 `-999`，百分比不可用编码为 `-1`。电脑端只将完全匹配本次数据的 `ACK1` 当作显示成功。固件整批验证字段、范围和行长，再更新画面并应答。

## 官方来源

- [LibreHardwareMonitor 源码与许可](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/tree/v0.9.6)
- [PawnIO 原版安装器](https://github.com/namazso/PawnIO.Setup/releases/tag/2.2.0)
- [Adafruit SSD1306 显示库](https://github.com/adafruit/Adafruit_SSD1306)

第三方许可说明见 `THIRD-PARTY-NOTICES.md`。

## 本机验证（2026-10-13）

- 电脑端发布构建、固件编译及串口写入校验通过。
- 10 次真实传感器采集均获得开发板完全匹配的数据应答，CPU/GPU 温度均可读取。
- 9 项实机协议检查通过，覆盖异常字段、范围、超长行、恢复、缺失传感器、分段数据及离线超时。
- Windows Forms 界面已渲染检查；用户已确认 OLED 显示正常且数值持续变化。
- 当前程序在托盘运行，真实数据与开发板应答记录保存在 `latest.json`。
