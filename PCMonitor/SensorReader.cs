using LibreHardwareMonitor.Hardware;

namespace PCMonitor;

public sealed class SensorReader : IDisposable
{
    private readonly Computer computer = new()
    {
        IsCpuEnabled = true, IsGpuEnabled = true, IsMemoryEnabled = true,
        IsMotherboardEnabled = false, IsStorageEnabled = false,
        IsNetworkEnabled = false, IsControllerEnabled = false
    };

    public SensorReader() => computer.Open();

    public SensorSample Read()
    {
        foreach (var hardware in computer.Hardware) Update(hardware);
        var cpu = computer.Hardware.FirstOrDefault(h => h.HardwareType == HardwareType.Cpu);
        var gpu = computer.Hardware.FirstOrDefault(h => h.HardwareType == HardwareType.GpuNvidia)
            ?? computer.Hardware.FirstOrDefault(h => h.HardwareType is HardwareType.GpuAmd or HardwareType.GpuIntel);
        var memory = computer.Hardware.FirstOrDefault(h => h.HardwareType == HardwareType.Memory && h.Identifier.ToString() == "/ram")
            ?? computer.Hardware.FirstOrDefault(h => h.HardwareType == HardwareType.Memory && h.Name == "Total Memory");
        var cpuTemperature = Sensor(cpu, SensorType.Temperature, "CPU Package")
            ?? Sensor(cpu, SensorType.Temperature, "Core Max")
            ?? MaxTemperature(cpu);
        return new(DateTimeOffset.Now, cpu?.Name ?? "CPU", gpu?.Name ?? "GPU",
            cpuTemperature, Sensor(gpu, SensorType.Temperature, "GPU Core") ?? MaxTemperature(gpu),
            Sensor(cpu, SensorType.Load, "CPU Total"), Sensor(gpu, SensorType.Load, "GPU Core")
                ?? Sensor(gpu, SensorType.Load, "D3D 3D"),
            Sensor(memory, SensorType.Load, "Memory"),
            Sensor(gpu, SensorType.SmallData, "GPU Memory Used"),
            Sensor(gpu, SensorType.SmallData, "GPU Memory Total"));
    }

    private static void Update(IHardware hardware)
    {
        hardware.Update();
        foreach (var child in hardware.SubHardware) Update(child);
    }

    private static IEnumerable<ISensor> Sensors(IHardware? hardware) => hardware == null
        ? [] : hardware.Sensors.Concat(hardware.SubHardware.SelectMany(Sensors));
    private static double? Sensor(IHardware? hardware, SensorType type, string name) =>
        Sensors(hardware).FirstOrDefault(s => s.SensorType == type && s.Name.Equals(name, StringComparison.OrdinalIgnoreCase))?.Value;
    private static double? MaxTemperature(IHardware? hardware) => Sensors(hardware)
        .Where(s => s.SensorType == SensorType.Temperature && s.Value.HasValue
            && !s.Name.Contains("Distance", StringComparison.OrdinalIgnoreCase))
        .Select(s => (double?)s.Value!.Value).DefaultIfEmpty(null).Max();

    public object Inventory() => computer.Hardware.Select(h => new
    {
        h.Name, Type = h.HardwareType.ToString(), Sensors = Sensors(h).Select(s => new
        { s.Name, Type = s.SensorType.ToString(), s.Value, Id = s.Identifier.ToString() }).ToArray()
    }).ToArray();

    public void Dispose() => computer.Close();
}
