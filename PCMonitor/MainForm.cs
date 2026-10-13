using System.IO.Ports;
using System.Runtime.InteropServices;

namespace PCMonitor;

public sealed class MainForm : Form
{
    private static readonly uint ShowMessage = RegisterWindowMessage("ESP8266.PCMonitor.Show");
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern uint RegisterWindowMessage(string name);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr FindWindow(string? className, string windowName);
    [DllImport("user32.dll")] private static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    private readonly ComboBox ports = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 115 };
    private readonly Button connect = new() { Text = "开始显示", AutoSize = true };
    private readonly Label cpuValue = MetricLabel();
    private readonly Label gpuValue = MetricLabel();
    private readonly Label memoryValue = MetricLabel();
    private readonly Label cpuDetail = DetailLabel();
    private readonly Label gpuDetail = DetailLabel();
    private readonly Label memoryDetail = DetailLabel();
    private readonly Label status = new() { AutoSize = true, ForeColor = Color.FromArgb(85, 96, 113) };
    private readonly Label hint = new() { AutoSize = false, Height = 42, Dock = DockStyle.Fill, ForeColor = Color.FromArgb(85, 96, 113) };
    private readonly NotifyIcon tray;
    private CancellationTokenSource? cancellation;
    private Task? worker;
    private volatile string currentPort = "COM3";
    private bool exiting;

    public MainForm(bool startMonitoring, bool startInTray = false)
    {
        Text = "ESP8266 电脑监控";
        Font = new Font("Microsoft YaHei UI", 10);
        BackColor = Color.FromArgb(245, 247, 250);
        ClientSize = new Size(760, 480);
        MinimumSize = new Size(776, 519);
        StartPosition = FormStartPosition.CenterScreen;
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(24), ColumnCount = 1, RowCount = 5 };
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 50));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 50));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        var heading = new Label { Text = "电脑状态 · OLED", Font = new Font(Font.FontFamily, 20, FontStyle.Bold), AutoSize = true };
        layout.Controls.Add(heading, 0, 0);
        var controls = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false };
        controls.Controls.Add(new Label { Text = "开发板串口", AutoSize = true, Padding = new Padding(0, 5, 6, 0) });
        controls.Controls.Add(ports);
        var refresh = new Button { Text = "刷新", AutoSize = true };
        refresh.Click += (_, _) => RefreshPorts();
        controls.Controls.Add(refresh);
        controls.Controls.Add(connect);
        var hide = new Button { Text = "缩到托盘", AutoSize = true };
        hide.Click += (_, _) => Hide();
        controls.Controls.Add(hide);
        var quit = new Button { Text = "退出", AutoSize = true };
        quit.Click += (_, _) => ExitApp();
        controls.Controls.Add(quit);
        layout.Controls.Add(controls, 0, 1);
        var cards = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1 };
        for (var i = 0; i < 3; i++) cards.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.333f));
        cards.Controls.Add(Card("CPU 温度", cpuValue, cpuDetail), 0, 0);
        cards.Controls.Add(Card("GPU 温度", gpuValue, gpuDetail), 1, 0);
        cards.Controls.Add(Card("内存占用", memoryValue, memoryDetail), 2, 0);
        layout.Controls.Add(cards, 0, 2);
        layout.Controls.Add(status, 0, 3);
        layout.Controls.Add(hint, 0, 4);
        Controls.Add(layout);
        tray = new NotifyIcon { Icon = SystemIcons.Application, Text = "ESP8266 电脑监控", Visible = true };
        var menu = new ContextMenuStrip();
        menu.Items.Add("打开监控", null, (_, _) => ShowWindow());
        menu.Items.Add("退出", null, (_, _) => ExitApp());
        tray.ContextMenuStrip = menu;
        tray.DoubleClick += (_, _) => ShowWindow();
        RefreshPorts();
        ports.SelectedIndexChanged += (_, _) => currentPort = ports.SelectedItem?.ToString() ?? "COM3";
        connect.Click += (_, _) => { if (cancellation == null) StartMonitor(); else StopMonitor(); };
        FormClosing += (_, e) => { if (!exiting && e.CloseReason == CloseReason.UserClosing) { e.Cancel = true; Hide(); } };
        Shown += (_, _) => { if (startMonitoring) StartMonitor(); if (startInTray) Hide(); };
        status.Text = "等待开始";
        hint.Text = "每秒更新 · 关闭窗口后继续在托盘运行";
    }

    private static Label MetricLabel() => new() { Text = "--", AutoSize = true,
        Font = new Font("Segoe UI", 32, FontStyle.Bold), ForeColor = Color.FromArgb(25, 60, 105) };
    private static Label DetailLabel() => new() { Text = "等待数据", AutoSize = false, Dock = DockStyle.Fill };
    private static Control Card(string title, Label value, Label detail)
    {
        var panel = new TableLayoutPanel { Dock = DockStyle.Fill, BackColor = Color.White, Padding = new Padding(14),
            Margin = new Padding(0, 6, 10, 8), RowCount = 3, ColumnCount = 1 };
        panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));
        panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 72));
        panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        panel.Controls.Add(new Label { Text = title, AutoSize = true }, 0, 0);
        panel.Controls.Add(value, 0, 1);
        panel.Controls.Add(detail, 0, 2);
        return panel;
    }

    private void RefreshPorts()
    {
        if (cancellation != null) return;
        var selected = ports.SelectedItem?.ToString() ?? currentPort;
        ports.Items.Clear();
        ports.Items.AddRange(SerialPort.GetPortNames().Order().Cast<object>().ToArray());
        if (ports.Items.Contains(selected)) ports.SelectedItem = selected;
        else if (ports.Items.Count > 0) ports.SelectedIndex = 0;
        currentPort = ports.SelectedItem?.ToString() ?? "COM3";
    }

    private void StartMonitor()
    {
        cancellation = new();
        var token = cancellation.Token;
        connect.Text = "停止显示";
        ports.Enabled = false;
        worker = Task.Run(async () =>
        {
            try
            {
                using var reader = new SensorReader();
                using var bridge = new SerialBridge();
                while (!token.IsCancellationRequested)
                {
                    var started = Environment.TickCount64;
                    var sample = reader.Read();
                    var update = new MonitorUpdate(sample, bridge.Send(currentPort, sample));
                    Program.Save(Path.Combine(Program.DataRoot, "latest.json"), update);
                    if (!IsDisposed && IsHandleCreated) BeginInvoke(() => ApplyUpdate(update));
                    await Task.Delay((int)Math.Max(100, 1000 - (Environment.TickCount64 - started)), token);
                }
            }
            catch (OperationCanceledException) { }
            catch (Exception ex)
            {
                Program.Save(Path.Combine(Program.DataRoot, "last-error.json"), new { Error = ex.ToString() });
                if (!IsDisposed && IsHandleCreated) BeginInvoke(() => { status.Text = "读取失败：" + ex.Message; StopMonitor(); });
            }
        }, token);
    }

    private void StopMonitor()
    {
        if (cancellation == null) return;
        cancellation.Cancel();
        try { worker?.Wait(2500); } catch (AggregateException) { }
        cancellation.Dispose();
        cancellation = null;
        worker = null;
        ports.Enabled = true;
        connect.Text = "开始显示";
        status.Text = "已停止 · OLED 会在 5 秒后显示离线";
    }

    public void ApplyUpdate(MonitorUpdate update)
    {
        cpuValue.Text = SensorSample.Display(update.Sample.CpuTemperature, "°C");
        gpuValue.Text = SensorSample.Display(update.Sample.GpuTemperature, "°C");
        memoryValue.Text = SensorSample.Display(update.Sample.MemoryLoad, "%");
        cpuDetail.Text = $"{update.Sample.CpuName}\n使用率 {SensorSample.Display(update.Sample.CpuLoad, "%")}";
        gpuDetail.Text = $"{update.Sample.GpuName}\n使用率 {SensorSample.Display(update.Sample.GpuLoad, "%")}";
        memoryDetail.Text = update.Sample.GpuMemoryUsedMB.HasValue
            ? $"显存 {update.Sample.GpuMemoryUsedMB / 1024:0.0} / {update.Sample.GpuMemoryTotalMB / 1024:0.0} GB" : "系统物理内存";
        status.Text = update.Bridge.Status + "  ·  " + update.Sample.Timestamp.ToString("HH:mm:ss");
        hint.Text = update.Sample.CpuTemperature.HasValue
            ? "每秒更新 · 关闭窗口后继续在托盘运行"
            : "CPU 温度未读取：请确认 PawnIO 驱动已安装，并以管理员身份运行。";
        tray.Text = $"CPU {SensorSample.Display(update.Sample.CpuTemperature, "C")} · GPU {SensorSample.Display(update.Sample.GpuTemperature, "C")}";
    }

    private void ShowWindow() { Show(); WindowState = FormWindowState.Normal; Activate(); }
    internal static void ShowExistingWindow()
    {
        var window = FindWindow(null, "ESP8266 电脑监控");
        if (window != IntPtr.Zero) PostMessage(window, ShowMessage, IntPtr.Zero, IntPtr.Zero);
    }
    protected override void WndProc(ref Message message)
    {
        if (message.Msg == ShowMessage) ShowWindow();
        base.WndProc(ref message);
    }
    private void ExitApp() { exiting = true; StopMonitor(); Close(); }
    protected override void Dispose(bool disposing)
    {
        if (disposing) { StopMonitor(); tray.Visible = false; tray.Dispose(); }
        base.Dispose(disposing);
    }
}
