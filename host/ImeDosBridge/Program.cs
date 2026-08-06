using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Windows.Forms;

namespace ImeDosBridge;

internal static class Program
{
    private const byte Hello = 1;
    private const byte Ping = 2;
    private const byte Pong = 3;
    private const byte OpenIme = 4;
    private const byte CloseIme = 7;
    private const byte Text = 5;
    private const byte TextAck = 6;

    public static async Task Main(string[] args)
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        var port = GetOption(args, "--port", 9821);
        var pipe = GetOption(args, "--pipe", (string?)null);
        var pipeClient = args.Contains("--pipe-client");
        var debugPipe = GetOption(args, "--debug-pipe", (string?)null);
        var text = GetOption(args, "--text", (string?)null);
        var textHex = GetOption(args, "--text-hex", (string?)null);
        var automaticText = textHex is not null
            ? ParseHex(textHex)
            : text is not null ? Encoding.GetEncoding(932).GetBytes(text) : null;
        if (automaticText is null || pipe is not null)
        {
            ApplicationConfiguration.Initialize();
            Application.Run(new BridgeForm(port, pipe, pipeClient, debugPipe, automaticText));
            return;
        }

        var listener = new TcpListener(IPAddress.Loopback, port);
        listener.Start();
        Console.WriteLine($"ImeDosBridge test mode listening on 127.0.0.1:{port}");
        using var client = await listener.AcceptTcpClientAsync();
        listener.Stop();
        Console.WriteLine($"Connected: {client.Client.RemoteEndPoint}");
        await RunClientAsync(client, automaticText);
    }

    private static async Task RunClientAsync(TcpClient client, byte[] text)
    {
        await using var stream = client.GetStream();
        while (true)
        {
            Packet packet;
            try { packet = await ReadPacketAsync(stream); }
            catch (EndOfStreamException) { Console.WriteLine("Disconnected"); return; }
            Console.WriteLine($"RX type={packet.Type} seq={packet.Sequence} length={packet.Payload.Length}");
            switch (packet.Type)
            {
                case Hello:
                case Ping:
                    await SendAsync(stream, new Packet(Pong, packet.Sequence, Array.Empty<byte>()));
                    break;
                case OpenIme:
                    await SendAsync(stream, new Packet(Text, packet.Sequence, text));
                    break;
                case CloseIme:
                    Console.WriteLine($"CLOSE_IME seq={packet.Sequence}");
                    break;
                case TextAck:
                    Console.WriteLine($"TEXT_ACK seq={packet.Sequence}");
                    break;
            }
        }
    }

    private static async Task SendAsync(NetworkStream stream, Packet packet)
    {
        var wire = packet.Encode();
        await stream.WriteAsync(wire);
        Console.WriteLine($"TX type={packet.Type} seq={packet.Sequence} length={packet.Payload.Length}");
    }

    private static async Task<Packet> ReadPacketAsync(NetworkStream stream)
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
        if (length > Packet.MaxPayload)
            throw new InvalidDataException("Payload too large.");
        var rest = new byte[length + 2];
        await ReadExactlyAsync(stream, rest);
        var wire = new byte[header.Length + rest.Length];
        header.CopyTo(wire, 0);
        rest.CopyTo(wire, header.Length);
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

    private static T GetOption<T>(string[] args, string name, T fallback)
    {
        for (var i = 0; i + 1 < args.Length; i++)
            if (args[i] == name && (typeof(T) != typeof(int) || int.TryParse(args[i + 1], out _)))
                return (T)Convert.ChangeType(args[i + 1], typeof(T));
        return fallback;
    }

    private static byte[] ParseHex(string value)
    {
        var compact = new string(value.Where(Uri.IsHexDigit).ToArray());
        if (compact.Length == 0 || (compact.Length & 1) != 0)
            throw new ArgumentException("--text-hex must contain an even number of hexadecimal digits.");
        return Convert.FromHexString(compact);
    }
}
