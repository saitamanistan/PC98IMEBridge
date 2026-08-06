using System.Windows.Forms;

namespace ImeDosBridge;

internal readonly record struct KeyBinding(Keys Key, Keys Modifiers, bool Enabled = true)
{
    public bool Matches(KeyEventArgs e) =>
        Enabled && e.KeyCode == Key && e.Modifiers == Modifiers;

    public override string ToString()
    {
        if (!Enabled)
            return "None";
        var parts = new List<string>(4);
        if ((Modifiers & Keys.Control) != 0) parts.Add("Ctrl");
        if ((Modifiers & Keys.Shift) != 0) parts.Add("Shift");
        if ((Modifiers & Keys.Alt) != 0) parts.Add("Alt");
        parts.Add(Key == Keys.Back ? "Backspace" : Key.ToString());
        return string.Join("+", parts);
    }

    public static bool TryParse(string text, out KeyBinding binding)
    {
        binding = default;
        if (text.Trim().Equals("NONE", StringComparison.OrdinalIgnoreCase))
        {
            binding = new KeyBinding(Keys.None, Keys.None, false);
            return true;
        }

        Keys modifiers = Keys.None;
        Keys key = Keys.None;
        foreach (string rawPart in text.Split('+', StringSplitOptions.TrimEntries |
                                                     StringSplitOptions.RemoveEmptyEntries))
        {
            string part = rawPart.ToUpperInvariant();
            if (part is "CTRL" or "CONTROL") modifiers |= Keys.Control;
            else if (part == "SHIFT") modifiers |= Keys.Shift;
            else if (part == "ALT") modifiers |= Keys.Alt;
            else
            {
                if (part == "BACKSPACE") part = "BACK";
                if (!Enum.TryParse(part, true, out key) || key == Keys.None)
                    return false;
            }
        }
        if (key == Keys.None)
            return false;
        binding = new KeyBinding(key, modifiers);
        return true;
    }
}

internal sealed class BridgeKeyBindings
{
    public KeyBinding Send { get; private set; } = new(Keys.Enter, Keys.None);
    public KeyBinding Close { get; private set; } = new(Keys.Escape, Keys.None);
    public KeyBinding RemoteEnter { get; private set; } = new(Keys.Enter, Keys.None);
    public KeyBinding RemoteBackspace { get; private set; } = new(Keys.Back, Keys.None);
    public KeyBinding RemoteLeft { get; private set; } = new(Keys.Left, Keys.None);
    public KeyBinding RemoteRight { get; private set; } = new(Keys.Right, Keys.None);
    public KeyBinding RemoteUp { get; private set; } = new(Keys.Up, Keys.None);
    public KeyBinding RemoteDown { get; private set; } = new(Keys.Down, Keys.None);

    public static BridgeKeyBindings Load()
    {
        var result = new BridgeKeyBindings();
        string path = Path.Combine(AppContext.BaseDirectory, "IMEBRIDGE.CFG");
        if (!File.Exists(path))
            return result;

        foreach (string rawLine in File.ReadLines(path))
        {
            string line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith(';') || line.StartsWith('#'))
                continue;
            int separator = line.IndexOf('=');
            if (separator <= 0 || !KeyBinding.TryParse(line[(separator + 1)..], out var binding))
                continue;
            switch (line[..separator].Trim().ToUpperInvariant())
            {
                case "SEND": result.Send = binding; break;
                case "CLOSE": result.Close = binding; break;
                case "REMOTE_ENTER": result.RemoteEnter = binding; break;
                case "REMOTE_BACKSPACE": result.RemoteBackspace = binding; break;
                case "REMOTE_LEFT": result.RemoteLeft = binding; break;
                case "REMOTE_RIGHT": result.RemoteRight = binding; break;
                case "REMOTE_UP": result.RemoteUp = binding; break;
                case "REMOTE_DOWN": result.RemoteDown = binding; break;
            }
        }
        return result;
    }
}
