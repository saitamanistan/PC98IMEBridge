using System.Net;
using System.Net.Sockets;
using System.Text;
using System.IO.Pipes;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace ImeDosBridge;

internal sealed class BridgeForm : Form
{
    private const int SwRestore = 9;

    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr windowHandle);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr windowHandle, int command);

    [DllImport("user32.dll")]
    private static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr windowHandle,
                                                         IntPtr processId);

    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();

    [DllImport("user32.dll")]
    private static extern bool AttachThreadInput(uint idAttach,
                                                  uint idAttachTo,
                                                  bool attach);

    [DllImport("user32.dll")]
    private static extern bool BringWindowToTop(IntPtr windowHandle);

    [DllImport("user32.dll")]
    private static extern IntPtr SetActiveWindow(IntPtr windowHandle);

    [DllImport("user32.dll")]
    private static extern IntPtr SetFocus(IntPtr windowHandle);

    [DllImport("user32.dll")]
    private static extern short GetAsyncKeyState(int virtualKey);

    private const byte Hello = 1;
    private const byte Ping = 2;
    private const byte Pong = 3;
    private const byte OpenIme = 4;
    private const byte CloseIme = 7;
    private const byte TextMessage = 5;
    private const byte TextAck = 6;
    private const byte KeyMessage = 8;
    private const byte KeyAck = 9;
    private const byte KeyCodeEnter = 1;
    private const byte KeyCodeLeft = 2;
    private const byte KeyCodeRight = 3;
    private const byte KeyCodeUp = 4;
    private const byte KeyCodeDown = 5;
    private const byte KeyCodeBackspace = 6;

    private readonly int port;
    private readonly string? pipeName;
    private readonly bool pipeClient;
    private readonly DebugPipeService? debugPipe;
    private readonly byte[]? automaticText;
    private readonly BridgeKeyBindings bindings = BridgeKeyBindings.Load();
    private readonly TextBox input = new()
    {
        Multiline = false,
        Dock = DockStyle.Top,
        Font = new Font("Yu Gothic UI", 14),
        TabIndex = 0
    };
    private readonly TextBox diagnosticLog = new()
    {
        Multiline = true,
        ReadOnly = true,
        Dock = DockStyle.Fill,
        ScrollBars = ScrollBars.Vertical,
        WordWrap = false,
        Font = new Font("Consolas", 9),
        BackColor = SystemColors.Window,
        Visible = false
    };
    private readonly Label status = new() { AutoSize = true, Text = "Waiting for PC-98" };
    private readonly Label target = new() { AutoSize = true, Text = "Target: --" };
    private readonly CheckBox showLog = new()
    {
        AutoSize = true,
        Text = "Activity",
        Checked = false
    };
    private readonly object sendLock = new();
    private Stream? stream;
    private ushort openSequence;
    private bool imeReady;
    private int targetMaxTextBytes = Packet.MaxPayload;
    private CancellationTokenSource cancellation = new();
    private string lastStatus = "Waiting for PC-98";

    public BridgeForm(int port, string? pipeName = null, bool pipeClient = false,
                      string? debugPipeName = null, byte[]? automaticText = null)
    {
        this.port = port;
        this.pipeName = pipeName;
        this.pipeClient = pipeClient;
        this.automaticText = automaticText?.ToArray();
        if (debugPipeName is not null)
        {
            debugPipe = new DebugPipeService(debugPipeName);
            debugPipe.LineReceived += line =>
            {
                Log($"[PC98 -> HOST][DEBUG] RX {line}");
                SetStatus($"PC98 debug: {line}");
            };
            debugPipe.StatusChanged += message =>
                Log($"[HOST][DEBUG PIPE] {message}");
        }
        Text = pipeName is null ? "DOS Japanese Input" : "DOS Japanese Input (PC-98)";
        ClientSize = new Size(540, 68);
        MinimumSize = new Size(420, 107);
        StartPosition = FormStartPosition.CenterScreen;
        KeyPreview = true;
        var top = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            Height = 28,
            WrapContents = false,
            AutoSize = false
        };
        top.Controls.Add(target);
        top.Controls.Add(status);
        top.Controls.Add(showLog);
        var layout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 3,
            Padding = new Padding(6)
        };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        layout.Controls.Add(top, 0, 0);
        layout.Controls.Add(input, 0, 1);
        layout.Controls.Add(diagnosticLog, 0, 2);
        Controls.Add(layout);
        showLog.CheckedChanged += (_, _) => SetLogVisible(showLog.Checked);
        KeyDown += OnKeyDown;
        Shown += (_, _) =>
        {
            _ = ListenAsync(cancellation.Token);
            if (debugPipe is not null)
                _ = debugPipe.RunAsync(cancellation.Token);
        };
        FormClosed += (_, _) => cancellation.Cancel();
    }

    private void SetLogVisible(bool visible)
    {
        diagnosticLog.Visible = visible;
        ClientSize = new Size(ClientSize.Width, visible ? 300 : 68);
    }

    private void OnKeyDown(object? sender, KeyEventArgs e)
    {
        if (input.TextLength > 0 && bindings.Send.Matches(e))
        {
            e.SuppressKeyPress = true;
            SendText();
        }
        else if (input.Focused && input.TextLength == 0 &&
                 bindings.RemoteEnter.Matches(e))
        {
            e.SuppressKeyPress = true;
            SendKey(KeyCodeEnter, "Enter");
        }
        else if (input.Focused && input.TextLength == 0 &&
                 bindings.RemoteBackspace.Matches(e))
        {
            e.SuppressKeyPress = true;
            SendKey(KeyCodeBackspace, "Backspace");
        }
        else if (bindings.Close.Matches(e))
        {
            e.SuppressKeyPress = true;
            if (input.TextLength > 0)
                input.Clear();
            else
                SendCloseIme();
        }
        else if (input.Focused && input.TextLength == 0)
        {
            byte keyCode;
            string keyName;
            if (bindings.RemoteLeft.Matches(e))
            {
                keyCode = KeyCodeLeft;
                keyName = "Left";
            }
            else if (bindings.RemoteRight.Matches(e))
            {
                keyCode = KeyCodeRight;
                keyName = "Right";
            }
            else if (bindings.RemoteUp.Matches(e))
            {
                keyCode = KeyCodeUp;
                keyName = "Up";
            }
            else if (bindings.RemoteDown.Matches(e))
            {
                keyCode = KeyCodeDown;
                keyName = "Down";
            }
            else return;
            e.SuppressKeyPress = true;
            SendKey(keyCode, keyName);
        }
    }

    private void FocusBridgeInput()
    {
        if (IsDisposed)
            return;
        BeginInvoke(() =>
        {
            if (WindowState == FormWindowState.Minimized)
                WindowState = FormWindowState.Normal;
            Show();
            bool focused = ForceForegroundWindow(Handle);
            input.Focus();
            input.Select(input.TextLength, 0);
            Log(focused ? "Input window focused" : "Could not focus input window");
        });
    }

    private async void FocusPc98()
    {
        // CLOSE can arrive while the hotkey key-down that requested it
        // is still being dispatched to the bridge. Moving focus before the
        // corresponding key-up lets Windows restore focus to the bridge.
        // Wait for the host-visible modifier (Shift, Ctrl, or Alt) to be
        // released, then verify once after the message settles. The TSR
        // supports SHIFT+SPACE, CTRL+SPACE, and GRAPH+SPACE; on np21w rev103
        // Windows Alt (VK_MENU) is delivered to the guest as the PC-98 GRPH
        // key (win9x/winkbd.cpp key106[0x12]=0x73, keystat.tbl 0x73=GRPH/ALT).
        for (int attempt = 0;
             attempt < 50 && HotkeyModifierStillDown();
             ++attempt)
            await Task.Delay(20);
        await Task.Delay(50);
        if (IsDisposed)
            return;

        BeginInvoke(() =>
        {
            var process = Process.GetProcessesByName("np21x64w")
                .FirstOrDefault(candidate => candidate.MainWindowHandle != IntPtr.Zero);
            if (process is null)
            {
                SetStatus("IME off; np21w window not found");
                return;
            }
            ShowWindow(process.MainWindowHandle, SwRestore);
            bool focused = ForceForegroundWindow(process.MainWindowHandle);
            BeginInvoke(async () =>
            {
                await Task.Delay(100);
                if (GetForegroundWindow() != process.MainWindowHandle)
                    focused = ForceForegroundWindow(process.MainWindowHandle);
                Log(focused ? "Returned to np21w" : "Could not return focus to np21w");
            });
        });
    }

    private static bool HotkeyModifierStillDown()
    {
        const int highBit = 0x8000;
        return (GetAsyncKeyState((int)Keys.ShiftKey) & highBit) != 0
            || (GetAsyncKeyState((int)Keys.ControlKey) & highBit) != 0
            || (GetAsyncKeyState((int)Keys.Menu) & highBit) != 0;
    }

    private static bool ForceForegroundWindow(IntPtr targetWindow)
    {
        IntPtr foregroundWindow = GetForegroundWindow();
        uint currentThread = GetCurrentThreadId();
        uint foregroundThread = foregroundWindow == IntPtr.Zero
            ? 0
            : GetWindowThreadProcessId(foregroundWindow, IntPtr.Zero);
        uint targetThread = GetWindowThreadProcessId(targetWindow, IntPtr.Zero);
        var attachedThreads = new List<uint>(2);

        try
        {
            foreach (uint thread in new[] { foregroundThread, targetThread }.Distinct())
            {
                if (thread != 0 && thread != currentThread &&
                    AttachThreadInput(currentThread, thread, true))
                    attachedThreads.Add(thread);
            }

            ShowWindow(targetWindow, SwRestore);
            BringWindowToTop(targetWindow);
            SetForegroundWindow(targetWindow);
            SetActiveWindow(targetWindow);
            SetFocus(targetWindow);
            return GetForegroundWindow() == targetWindow;
        }
        finally
        {
            for (int index = attachedThreads.Count - 1; index >= 0; --index)
                AttachThreadInput(currentThread, attachedThreads[index], false);
        }
    }

    private async Task ListenAsync(CancellationToken token)
    {
        if (pipeName is not null)
        {
            if (pipeClient)
            {
                while (!token.IsCancellationRequested)
                {
                    using var pipe = new NamedPipeClientStream(".", pipeName,
                        PipeDirection.InOut, PipeOptions.Asynchronous);
                    try
                    {
                        await pipe.ConnectAsync(5000, token);
                        await ServeAsync(pipe, $"PC-98 ({pipeName})", token);
                    }
                    catch (TimeoutException) { SetStatus("Pipe connect timeout"); }
                    catch (EndOfStreamException) { SetStatus("Disconnected"); }
                    catch (IOException ex) { SetStatus($"Pipe error: {ex.Message}"); }
                    catch (InvalidDataException ex) { SetStatus($"Protocol error: {ex.Message}"); }
                    catch (OperationCanceledException) { return; }
                }
                return;
            }
            while (!token.IsCancellationRequested)
            {
                await using var pipe = new NamedPipeServerStream(
                    pipeName, PipeDirection.InOut, 1, PipeTransmissionMode.Byte,
                    PipeOptions.Asynchronous);
                try
                {
                    await pipe.WaitForConnectionAsync(token);
                    await ServeAsync(pipe, $"NamedPipe:{pipeName}", token);
                }
                catch (EndOfStreamException) { SetStatus("Disconnected"); }
                catch (IOException ex) { SetStatus($"Pipe error: {ex.Message}"); }
                catch (InvalidDataException ex) { SetStatus($"Protocol error: {ex.Message}"); }
                catch (OperationCanceledException) { return; }
                catch (Exception ex) { SetStatus($"Error: {ex.Message}"); }
            }
            return;
        }

        var listener = new TcpListener(IPAddress.Loopback, port);
        listener.Start();
        try
        {
            while (!token.IsCancellationRequested)
            {
                using var client = await listener.AcceptTcpClientAsync(token);
                try { await ServeAsync(client.GetStream(), $"TCP:{client.Client.RemoteEndPoint}", token); }
                catch (EndOfStreamException) { SetStatus("Disconnected"); }
                catch (IOException ex) { SetStatus($"TCP error: {ex.Message}"); }
                catch (InvalidDataException ex) { SetStatus($"Protocol error: {ex.Message}"); }
            }
        }
        catch (OperationCanceledException) { }
        catch (Exception ex) { SetStatus($"Error: {ex.Message}"); }
        finally { listener.Stop(); }
    }

    private async Task ServeAsync(Stream connected, string description, CancellationToken token)
    {
        stream = connected;
        openSequence = 0;
        imeReady = false;
        targetMaxTextBytes = Packet.MaxPayload;
        SetStatus($"Connected: {description}");
        try
        {
            while (!token.IsCancellationRequested)
            {
                Packet packet = await ReadPacketAsync(connected);
                if (packet.Type == Hello)
                {
                    targetMaxTextBytes = packet.Payload.Length >= 10
                        ? BitConverter.ToUInt16(packet.Payload, 8)
                        : Packet.MaxPayload;
                    if (targetMaxTextBytes <= 0 || targetMaxTextBytes > Packet.MaxPayload)
                        targetMaxTextBytes = Packet.MaxPayload;
                    SetStatus("PC-98 connection confirmed");
                    SetTarget(packet.Payload.Length > 0 && packet.Payload[0] == 1
                        ? "PC-98" : "Unknown");
                    await SendAsync(connected, new Packet(Pong, packet.Sequence, Array.Empty<byte>()));
                }
                else if (packet.Type == Ping)
                {
                    await SendAsync(connected, new Packet(Pong, packet.Sequence, Array.Empty<byte>()));
                }
                else if (packet.Type == OpenIme)
                {
                    openSequence = packet.Sequence;
                    imeReady = true;
                    SetStatus("Input ready");
                    if (automaticText is not null)
                    {
                        if (automaticText.Length > targetMaxTextBytes)
                        {
                            SetStatus($"Automatic text is too long: {automaticText.Length}/{targetMaxTextBytes} bytes");
                            continue;
                        }
                        await SendAsync(connected,
                            new Packet(TextMessage, openSequence, automaticText));
                        SetStatus("Automatic text sent");
                    }
                    else
                    {
                        FocusBridgeInput();
                    }
                }
                else if (packet.Type == CloseIme)
                {
                    openSequence = 0;
                    imeReady = false;
                    BeginInvoke(() => input.Clear());
                    SetStatus("Input cancelled");
                    FocusPc98();
                }
                else if (packet.Type == TextAck)
                {
                    openSequence = 0;
                    imeReady = false;
                    SetStatus("PC-98 received text");
                }
                else if (packet.Type == KeyAck)
                {
                    openSequence = 0;
                    imeReady = false;
                    SetStatus("PC-98 received key");
                }
            }
        }
        finally
        {
            // A reconnect can replace stream before the previous ServeAsync
            // reaches its finally block. Do not clear the newer connection.
            if (ReferenceEquals(stream, connected))
            {
                stream = null;
                openSequence = 0;
                imeReady = false;
                targetMaxTextBytes = Packet.MaxPayload;
            }
        }
    }

    private async void SendText()
    {
        var current = stream;
        if (current is null || !imeReady || string.IsNullOrEmpty(input.Text)) return;
        var bytes = Encoding.GetEncoding(932).GetBytes(input.Text);
        if (bytes.Length > targetMaxTextBytes)
        {
            SetStatus($"Text is too long: {bytes.Length}/{targetMaxTextBytes} CP932 bytes");
            return;
        }
        try
        {
            imeReady = false;
            await SendAsync(current, new Packet(TextMessage, openSequence, bytes));
            input.Clear();
            SetStatus("Text sent; waiting for acknowledgement");
        }
        catch (Exception ex)
        {
            imeReady = true;
            SetStatus($"Send error: {ex.Message}");
        }
    }

    private async void SendKey(byte keyCode, string keyName)
    {
        var current = stream;
        if (current is null || !imeReady)
            return;
        try
        {
            imeReady = false;
            await SendAsync(current,
                new Packet(KeyMessage, openSequence, new[] { keyCode }));
            SetStatus($"{keyName} sent; waiting for acknowledgement");
        }
        catch (Exception ex)
        {
            imeReady = true;
            SetStatus($"Key send error: {ex.Message}");
        }
    }

    private async void SendCloseIme()
    {
        var current = stream;
        if (current is null || !imeReady)
            return;
        try
        {
            imeReady = false;
            await SendAsync(current,
                new Packet(CloseIme, openSequence, Array.Empty<byte>()));
            SetStatus("IME off requested by Escape");
        }
        catch (Exception ex)
        {
            imeReady = true;
            SetStatus($"IME off request error: {ex.Message}");
        }
    }

    private async Task SendAsync(Packet packet)
    {
        var current = stream ?? throw new IOException("No client connected.");
        await SendAsync(current, packet);
    }

    private async Task SendAsync(Stream current, Packet packet)
    {
        var wire = packet.Encode();
        lock (sendLock)
        {
            current.Write(wire, 0, wire.Length);
            current.Flush();
        }
        await Task.CompletedTask;
    }

    private async Task<Packet> ReadPacketAsync(Stream stream)
    {
        var header = new byte[8];
        while (true)
        {
            await ReadExactlyAsync(stream, header.AsMemory(0, 1));
            if (header[0] != 0x49) continue;
            await ReadExactlyAsync(stream, header.AsMemory(1, 1));
            if (header[1] == 0x44) break;
        }
        await ReadExactlyAsync(stream, header.AsMemory(2, 6));
        var length = BitConverter.ToUInt16(header, 6);
        if (length > Packet.MaxPayload) throw new InvalidDataException("Payload too large.");
        var rest = new byte[length + 2];
        await ReadExactlyAsync(stream, rest);
        var wire = header.Concat(rest).ToArray();
        return Packet.Decode(wire);
    }

    private static async Task ReadExactlyAsync(Stream stream, Memory<byte> buffer)
    {
        var offset = 0;
        while (offset < buffer.Length)
        {
            var count = await stream.ReadAsync(buffer[offset..]);
            if (count == 0) throw new EndOfStreamException();
            offset += count;
        }
    }

    private void SetStatus(string value)
    {
        if (IsDisposed) return;
        if (value == lastStatus) return;
        lastStatus = value;
        Log(value);
        if (InvokeRequired) BeginInvoke(() => status.Text = value); else status.Text = value;
    }

    private void Log(string message)
    {
        AppendDiagnostic(message);
        try
        {
            File.AppendAllText(Path.Combine(AppContext.BaseDirectory, "bridge-status.log"),
                $"{DateTime.Now:O} {message}{Environment.NewLine}");
        }
        catch { }
    }

    private void AppendDiagnostic(string message)
    {
        if (IsDisposed) return;
        void Append()
        {
            if (diagnosticLog.TextLength > 64 * 1024)
                diagnosticLog.Text = diagnosticLog.Text[^32768..];
            diagnosticLog.AppendText($"{DateTime.Now:HH:mm:ss.fff} {message}{Environment.NewLine}");
        }
        if (InvokeRequired) BeginInvoke(Append); else Append();
    }

    private void SetTarget(string value)
    {
        if (InvokeRequired) BeginInvoke(() => target.Text = $"Target: {value}"); else target.Text = $"Target: {value}";
    }
}
