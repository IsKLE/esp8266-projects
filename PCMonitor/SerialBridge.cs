using System.IO.Ports;

namespace PCMonitor;

public sealed class SerialBridge : IDisposable
{
    private SerialPort? serial;
    private string? openedPort;
    private uint sequence;

    public BridgeResult Send(string port, SensorSample sample)
    {
        if (!SerialPort.GetPortNames().Contains(port, StringComparer.OrdinalIgnoreCase))
        {
            Dispose();
            return new(false, false, $"等待开发板 {port}", null, null);
        }
        try
        {
            if (serial == null || openedPort != port || !serial.IsOpen)
            {
                Dispose();
                serial = new(port, 115200) { DtrEnable = false, RtsEnable = false,
                    ReadTimeout = 1200, WriteTimeout = 1000, NewLine = "\n" };
                serial.Open();
                openedPort = port;
                Thread.Sleep(250);
            }
            var packet = sample.WirePacket(++sequence);
            serial.WriteLine(packet);
            var expected = packet.Replace("PC1,", "ACK1,", StringComparison.Ordinal);
            var deadline = Environment.TickCount64 + 1500;
            while (Environment.TickCount64 < deadline)
            {
                try
                {
                    var line = serial.ReadLine().Trim();
                    if (line == expected) return new(true, true, $"{port} · 屏幕已更新", packet, line);
                    if (line.StartsWith("ERR1,")) return new(true, false, "开发板拒绝数据：" + line, packet, line);
                }
                catch (TimeoutException) { break; }
            }
            return new(true, false, "串口已连接，等待匹配固件应答", packet, null);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidOperationException or TimeoutException)
        {
            Dispose();
            return new(false, false, "串口连接失败：" + ex.Message, null, null);
        }
    }

    public void Dispose()
    {
        if (serial != null)
        {
            try { serial.Close(); } catch (IOException) { }
            serial.Dispose();
        }
        serial = null;
        openedPort = null;
    }
}
