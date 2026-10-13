using System.Text.Json;

namespace PCMonitor;

internal static class Program
{
    internal static readonly string DataRoot = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "ESP8266", "PCMonitor");
    internal static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };

    [STAThread]
    private static int Main(string[] args)
    {
        Directory.CreateDirectory(DataRoot);
        try
        {
            if (args.Contains("--self-test"))
            {
                var sample = new SensorSample(DateTimeOffset.Now, "CPU", "GPU", 57.4, 62.8, 25, 9, 48, null, null);
                if (sample.WirePacket(7) != "PC1,7,574,628,25,9,48") throw new Exception("Wire encoding mismatch");
                if ((sample with { CpuTemperature = null, GpuLoad = null }).WirePacket(8) != "PC1,8,-999,628,25,-1,48")
                    throw new Exception("Missing sensors must be encoded as unavailable");
                Save(Option(args, "--log") ?? Path.Combine(DataRoot, "self-test.json"), new { Passed = true });
                return 0;
            }
            if (args.Contains("--diagnose"))
            {
                using var reader = new SensorReader();
                Thread.Sleep(1000);
                var sample = reader.Read();
                Save(Option(args, "--diagnose")!, new { Sample = sample, Hardware = reader.Inventory() });
                return 0;
            }
            if (args.Contains("--verify-port"))
            {
                var count = int.Parse(Option(args, "--samples") ?? "8");
                using var reader = new SensorReader();
                using var bridge = new SerialBridge();
                var updates = new List<MonitorUpdate>();
                for (var i = 0; i < count; i++)
                {
                    var sample = reader.Read();
                    updates.Add(new(sample, bridge.Send(Option(args, "--verify-port")!, sample)));
                    Thread.Sleep(1000);
                }
                Save(Option(args, "--log")!, updates);
                return updates.All(u => u.Sample.CpuTemperature.HasValue && u.Sample.GpuTemperature.HasValue && u.Bridge.Applied) ? 0 : 2;
            }
            ApplicationConfiguration.Initialize();
            if (args.Contains("--render-ui"))
            {
                using var form = new MainForm(false);
                var snapshotPath = Option(args, "--snapshot");
                if (snapshotPath != null)
                {
                    using var doc = JsonDocument.Parse(File.ReadAllText(snapshotPath));
                    var sample = JsonSerializer.Deserialize<SensorSample>(doc.RootElement.GetProperty("Sample"), JsonOptions)!;
                    form.ApplyUpdate(new(sample, new(true, true, "COM3 · 屏幕已更新", null, null)));
                }
                CreateRenderHandles(form);
                form.PerformLayout();
                Application.DoEvents();
                using var bitmap = new Bitmap(form.Width, form.Height);
                form.DrawToBitmap(bitmap, new Rectangle(0, 0, bitmap.Width, bitmap.Height));
                bitmap.Save(Option(args, "--render-ui")!);
                return 0;
            }
            using var mutex = new Mutex(true, @"Local\ESP8266-PCMonitor", out var created);
            if (!created) { MainForm.ShowExistingWindow(); return 0; }
            Application.Run(new MainForm(true, args.Contains("--tray")));
            return 0;
        }
        catch (Exception ex)
        {
            Save(Path.Combine(DataRoot, "last-error.json"), new { Time = DateTimeOffset.Now, Error = ex.ToString() });
            if (args.Length == 0) MessageBox.Show(ex.Message, "ESP8266 电脑监控", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }

    private static string? Option(string[] args, string key)
    {
        var index = Array.IndexOf(args, key);
        return index >= 0 && index + 1 < args.Length ? args[index + 1] : null;
    }

    internal static void Save(string path, object value)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
        File.WriteAllText(path, JsonSerializer.Serialize(value, JsonOptions));
    }

    private static void CreateRenderHandles(Control control)
    {
        _ = control.Handle;
        foreach (Control child in control.Controls) CreateRenderHandles(child);
        control.PerformLayout();
    }
}
