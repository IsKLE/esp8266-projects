# ESP8266 NAT 中继

适配本机的 ESP-12F / NodeMCU OLED Type-C 开发板。OLED 为 SSD1306 128×64，SDA=GPIO14、SCL=GPIO12，地址 0x3C。

## 当前配置

- 上游：用户配置的 2.4 GHz WPA2 Wi-Fi。
- 新热点：`ESP8266-NAT`，独立随机密码。桌面的“ESP8266-NAT 热点密码”入口可查看密码。
- 子网：`192.168.4.0/24`，网关 `192.168.4.1`。DHCP 自动分配客户端地址和上游 DNS。
- 上游地址由路由器 DHCP 动态分配，具体地址会随网络环境变化。
- 最多 4 个热点客户端，256 个 NAPT 表项。支持 IPv4 TCP、UDP、ICMP Echo。
- CPU 频率：160 MHz；`build.ps1` 对 NatRelay 指定 `xtal=160`，重新构建也会保留该频率。
- 配置保存在开发板 Flash，重新供电即可自动连接和中继；电脑和串口软件无需持续运行。

## FLASH 按键与三页显示

界面旋转 90°，使用 **64×128 的竖向画布**。三页重新按窄屏排版，移除 `FLASH: next` 提示。长地址和 SSID 分行显示；当前 16 位热点密码按每行 4 个字符放大显示，按从上到下的顺序拼接输入。

正常开机后，短按并松开 **FLASH**，依次轮换以下页面；NAT 和入站过滤持续运行。

1. **网络信息（1/3）**：上游 Wi-Fi、WAN/AP 地址、客户端数量、RSSI、NAT/入站过滤状态、拦截包计数和运行时间。
2. **热点密码（2/3）**：新热点的 SSID 和密码，不显示上游 Wi-Fi 密码。密码只显示在 OLED，不通过串口诊断输出。
3. **性能（3/3）**：CPU 频率、剩余堆内存、当前上行/下行速率、开机以来的上行/下行峰值、内存碎片率和运行时间。

按键接 GPIO0，启用内部上拉，按下为低电平，40 ms 消抖，松开后触发一次切页；持续按住不会连续翻页。RESET 仍用于硬件复位。不要按住 FLASH 再上电或复位，否则可能进入固件下载模式。重启后返回网络页，峰值速率清零，Wi-Fi 凭据保留。

屏幕约每秒更新。`NAT ON` 表示 Wi-Fi 上游及 NAT 已启用，不等同于互联网可达性探测。上游断线时关闭转发，保留热点等待自动重连。

### 性能数字的含义

- `CPU 160MHz` 表示 CPU 频率 160 MHz，不是实时 CPU 占用百分比。
- `Free ...K` 为当前剩余堆内存，1 K=1024 字节；`Frag` 为分配器报告的碎片率。
- `UP` 为热点收到客户端数据的速率，`DN` 为热点成功提交发送的数据速率，约每秒采样一次。计数包含热点侧 Ethernet 帧、协议头及 DHCP/ARP 等控制流量，不记录报文内容；不包含无线 MAC 开销、无线重传或空口占用百分比。
- `MAX UP`、`MAX DN` 是本次启动以来采样得到的峰值，不是理论链路速率或测速得到的极限带宽。速率单位 KB/s，1 KB=1000 字节；超过 1000 KB/s 时自动使用 MB/s，保持数值完整显示。

## 访问规则

子网设备可以主动访问互联网和主网；已建立 NAT 会话的回复可返回。主网不能主动建立到子网设备的连接。

过滤器位于真实 lwIP2 Ethernet 收包入口，先检查原始目的地址，再执行 NAT。主网通过静态路由直接发往子网的包也被丢弃。上游入站 TCP SYN 被拦截；UDP 和 ICMP 回复由 lwIP 的 NAT 会话表匹配。未设置任何端口转发，也没有网络配置管理页面。

这不是二层桥接：两个网络的广播、IPv6和服务发现不会被桥接。为配合该版本 lwIP NAPT 的限制，丢弃 IPv4 分片和不支持的转发协议。主网仍可看到开发板的上游 IP、MAC 和无线热点，子网设备也能相互通信。

## 构建与配置

在项目根目录运行：

```powershell
.\build.ps1 -Sketch NatRelay -Upload -Port COM3
```

必须使用此构建脚本的 `--wrap=ethernet_input_LWIP2` 链接参数；ESP8266 的收包入口绕过 `netif.input`，因此普通 netif 回调不能提供这里的过滤效果。

工具位于 `tools/nat_control.py`，使用已安装的 ESP8266 Python 和 pyserial。首次使用 `provision` 前，请将 `UPSTREAM_SSID` 和 `UPSTREAM_PROFILE` 改为自己的 Wi-Fi 名称及 Windows 保存的 WLAN 配置名称。工具会读取对应配置中的密码，再生成热点密码。主机配置保存在 `<项目目录>\NatRelay\credentials.dpapi`，受 Windows 当前用户 DPAPI 保护；开发板的 Flash 配置不加密。密码不进入源码、命令行参数或诊断日志。

```powershell
$py = '<项目目录>\arduino-data\packages\esp8266\tools\python3\3.7.2-post1\python.exe'
& $py -B .\tools\nat_control.py status --seconds 10
& $py -B .\tools\nat_control.py test --seconds 5
& $py -B .\tools\nat_control.py test-ui --seconds 5
& $py -B .\tools\nat_control.py show-password
& $py -B .\tools\nat_control.py provision --seconds 20
```

配置通过 USB 串口传输并带 CRC32 校验。固件不回显配置包。变更 Wi-Fi 时可在本机工具中调整配置流程；不要将凭据提交到 Git。`connect-ap`、`connect-upstream`、`radio-on`、`cleanup` 是本机网络验证工具，会修改当前 Wi-Fi 连接；`cleanup` 删除测试配置并恢复本次验证前的 Wi-Fi 软件关闭状态。

## 验证证据

证据目录：`evidence/nat-relay`。

- 固件编译、刷写校验通过；反汇编确认实际收包函数调用过滤包装函数。
- 开发板执行 13 个报文过滤检查通过，覆盖 TCP 新连接/回复、直接路由、分片、截断、UDP、DHCP、IPv6、ARP。
- 电脑连接热点后通过 DHCP 获得子网地址、网关和上游 DNS。
- 绑定该客户端地址的路由器 DNS、公共 DNS、HTTP、HTTPS（证书验证及 TLS 1.2）均通过。
- 电脑切换到主网，临时添加直达子网客户端的路由，发送 20 个 UDP 探测并尝试入站 TCP。开发板拦截计数增加 22，入站 TCP 未连接，开发板 Ping 正常；临时路由已清理。该测试验证真实上游包进入过滤器并被丢弃，未在子网客户端上运行接收服务。
- 原 Cloudflare HTTPS 测试在直连主网和中继下均超时；改用本线路可访问的 HTTPS 站点后两边均通过。

本次未做吞吐量基准、长时间稳定性或多客户端压力测试。

按键与速率显示的新增验证证据在 `evidence/nat-button`。开发板执行按键消抖/长按/时钟回绕检查 26/26，速率采样/峰值保持/空闲归零/计数回绕检查 11/11；原有报文过滤检查仍为 13/13。

## 切回电脑监控

电脑监控托盘程序已经退出，避免占用 COM3。原监控源码、桌面入口和固件备份保留。要恢复监控，先刷入 `PCStats`，再打开桌面的电脑监控程序；NAT 固件和监控固件是两个独立程序。

```powershell
.\build.ps1 -Sketch PCStats -Upload -Port COM3
```
