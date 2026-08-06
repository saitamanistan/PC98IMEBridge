using System.IO.Pipes;
using System.Text;

namespace ImeDosBridge;

internal sealed class DebugPipeService
{
    private readonly string pipeName;
    private readonly SemaphoreSlim sendLock = new(1, 1);
    private NamedPipeClientStream? stream;

    public event Action<string>? LineReceived;
    public event Action<string>? StatusChanged;

    public DebugPipeService(string pipeName)
    {
        this.pipeName = pipeName;
    }

    public async Task RunAsync(CancellationToken token)
    {
        while (!token.IsCancellationRequested)
        {
            await using var pipe = new NamedPipeClientStream(".", pipeName,
                PipeDirection.InOut, PipeOptions.Asynchronous);
            try
            {
                await pipe.ConnectAsync(5000, token);
                stream = pipe;
                ReportStatus($"Connected: {pipeName}");
                using var reader = new StreamReader(pipe, Encoding.GetEncoding(932),
                    false, 256, leaveOpen: true);
                while (!token.IsCancellationRequested)
                {
                    var line = await reader.ReadLineAsync(token);
                    if (line is null)
                        break;
                    ReportLine(line);
                }
            }
            catch (TimeoutException)
            {
                ReportStatus("Connection timeout");
            }
            catch (IOException ex)
            {
                ReportStatus($"Error: {ex.Message}");
            }
            catch (OperationCanceledException)
            {
                return;
            }
            finally
            {
                if (ReferenceEquals(stream, pipe))
                    stream = null;
            }
        }
    }

    private void Report(string message, Action<string>? handler)
    {
        try
        {
            File.AppendAllText(Path.Combine(AppContext.BaseDirectory, "pc98-debug.log"),
                $"{DateTime.Now:O} {message}{Environment.NewLine}");
        }
        catch
        {
        }
        handler?.Invoke(message);
    }

    private void ReportLine(string message) => Report(message, LineReceived);

    private void ReportStatus(string message) => Report(message, StatusChanged);

    public async Task SendCommandAsync(string command, CancellationToken token)
    {
        var current = stream ?? throw new IOException("Debug pipe is not connected.");
        var bytes = Encoding.GetEncoding(932).GetBytes(command + "\r\n");
        await sendLock.WaitAsync(token);
        try
        {
            await current.WriteAsync(bytes, token);
            await current.FlushAsync(token);
        }
        finally
        {
            sendLock.Release();
        }
    }
}
