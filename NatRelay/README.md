# ESP8266 NAT 中继

此 sketch 将 ESP8266 连接到用户配置的 2.4 GHz WPA2 Wi-Fi，并创建独立热点 `ESP8266-NAT`。热点使用 `192.168.4.0/24` 子网，网关为 `192.168.4.1`，客户端通过 DHCP 获取地址和上游 DNS。

## 功能

- 为热点客户端提供 IPv4 TCP、UDP 和 ICMP Echo 的 NAT 转发。
- 过滤主网主动发起的入站连接；已建立 NAT 会话的回复可以返回。
- OLED 提供网络信息、热点密码、性能和流量电子宠物四页。开机显示宠物；正常运行时短按并松开 FLASH，依次切换网络 → 密码 → 性能 → 宠物；NAT 转发在切页时继续运行。
- 性能页显示 CPU 频率、堆内存、热点流量速率、启动以来的峰值和内存碎片率。
- 配置保存在开发板 Flash。设备重新供电后会自动连接上游并启动热点。

板载 OLED 需要 SSD1306 128×64，SDA 接 GPIO14、SCL 接 GPIO12，I²C 地址为 0x3C。FLASH 按键接 GPIO0。构建配置使用 160 MHz CPU 频率。

## 构建与使用

需要 ESP8266 Arduino Core 3.1.2、Adafruit SSD1306、Adafruit GFX 和 Adafruit BusIO。必须通过仓库中的构建脚本编译，因为它为 lwIP Ethernet 收包入口添加 `--wrap=ethernet_input_LWIP2` 过滤钩子：

```powershell
.\build.ps1 -Sketch NatRelay
.\build.ps1 -Sketch NatRelay -Upload -Port COM3
```

首次配置前，在 `tools/nat_control.py` 中将 `UPSTREAM_SSID` 和 `UPSTREAM_PROFILE` 设置为自己的上游 Wi-Fi 名称，以及 Windows 已保存的 WLAN 配置名称。安装好 pyserial 后，在项目根目录运行：

```powershell
python -B .\tools\nat_control.py provision --seconds 20
```

配置工具从 Windows WLAN 配置读取上游密码，生成随机热点密码，再通过 USB 串口将配置写入开发板。主机侧配置保存在项目目录下的 `NatRelay/credentials.dpapi`，受当前 Windows 用户的 DPAPI 保护；`.gitignore` 会排除该文件。开发板 Flash 中的配置不加密。密码不会作为命令行参数发送，也不会由固件写入串口诊断输出。

其他常用命令：

```powershell
python -B .\tools\nat_control.py status --seconds 10
python -B .\tools\nat_control.py test --seconds 5
python -B .\tools\nat_control.py test-ui --seconds 5
python -B .\tools\nat_control.py test-pet --seconds 5
python -B .\tools\nat_control.py pet --seconds 2
python -B .\tools\nat_control.py show-password
```

如果当前 Python 环境未安装 pyserial，可将环境变量 `ESP8266_SERIAL_LIBRARY` 指向 ESP8266 工具包中的 pyserial 模块目录。`connect-ap`、`connect-upstream`、`radio-on` 和 `cleanup` 是可选的本机网络验证命令，会更改当前 Wi-Fi 连接；`cleanup` 会删除测试配置并恢复验证前的软件无线电状态。

## OLED 页面

屏幕旋转 90°，使用 64×128 竖向画布。

1. **网络信息**：上游 Wi-Fi、WAN/AP 地址、客户端数量、RSSI、NAT 状态、拦截包数和运行时间。
2. **热点密码**：热点名称和密码。密码只显示在 OLED 页面中。
3. **性能**：CPU 频率、剩余堆内存、当前和峰值上行/下行速率、内存碎片率。
4. **电子宠物**：像素猫动画、上下行速率和连接热点的设备数量（`Friends`）。右上实心圆表示 NAT 已启用，空心圆表示尚未启用。

宠物根据热点上下行速率之和活动：低于 1 KiB/s 时打盹（`SLEEP`），达到 1 KiB/s 时走动（`WALK`），达到 32 KiB/s 时奔跑（`RUN`）；上游断开或 NAT 暂未就绪时显示疑惑表情（`OFFLINE`），缺少配置时为 `SETUP`。短暂流量会保持走动 2.5 秒、奔跑 1.5 秒，避免频繁抖动。上述阈值是动画规则，不代表网络的最大带宽。

动画使用现有 OLED 缓冲区直接绘图，休息时每秒刷新 2 次，活动时最多 4 次；仅宠物页使用此刷新频率。不申请额外图片缓冲区，也不写入 Flash。CPU 维持 160 MHz。板载指示灯的 GPIO 和有效电平尚未确认，宠物状态先通过屏幕显示。

`test-pet` 在开发板上检查流量阈值、短流量保持、断线重连、计时器回绕和速率相加溢出。`pet` 通过 USB 切到宠物页；`pet-frame` 切到宠物页并导出 1024 字节 SSD1306 缓冲区（`PET1` 十六进制记录），可用于渲染核对，不导出密码页。串口状态新增 `pageCount`、`oled`、`petState` 和 `petFrame`。

`UP` 是热点收到客户端数据的速率，`DN` 是热点成功提交发送的数据速率。计数包括热点侧 Ethernet 帧及协议头，不保存或输出报文内容；不包括无线 MAC 开销、重传和空口占用。峰值只统计本次启动以来的采样值，不代表链路理论带宽。

## 网络行为

子网客户端可以主动访问上游网络；主网不能主动建立到子网客户端的连接。该功能不是二层桥接，不转发广播、IPv6 或服务发现。受 lwIP NAPT 限制，IPv4 分片和不支持的转发协议会被丢弃。上游断开时会暂停转发，并保留热点等待自动重连。

## 其他固件

电脑监控程序使用 `PCStats` 固件；刷入步骤见 [PCMonitor/README.md](../PCMonitor/README.md)。切换到电脑监控固件：

```powershell
.\build.ps1 -Sketch PCStats -Upload -Port COM3
```
