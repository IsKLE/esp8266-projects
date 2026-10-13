using System.Globalization;

namespace PCMonitor;

public sealed record SensorSample(DateTimeOffset Timestamp, string CpuName, string GpuName,
    double? CpuTemperature, double? GpuTemperature, double? CpuLoad, double? GpuLoad,
    double? MemoryLoad, double? GpuMemoryUsedMB, double? GpuMemoryTotalMB)
{
    public string WirePacket(uint sequence) => string.Join(',', "PC1", sequence,
        Temperature(CpuTemperature), Temperature(GpuTemperature), Percent(CpuLoad),
        Percent(GpuLoad), Percent(MemoryLoad));

    private static int Temperature(double? value) => value is >= -40 and <= 150
        ? (int)Math.Round(value.Value * 10) : -999;
    private static int Percent(double? value) => value is >= 0 and <= 100
        ? (int)Math.Round(value.Value) : -1;
    public static string Display(double? value, string suffix = "") => value.HasValue
        ? value.Value.ToString("0", CultureInfo.InvariantCulture) + suffix : "--";
}

public sealed record BridgeResult(bool Connected, bool Applied, string Status, string? Packet, string? Ack);
public sealed record MonitorUpdate(SensorSample Sample, BridgeResult Bridge);
