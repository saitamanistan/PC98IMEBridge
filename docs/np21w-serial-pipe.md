# np21w COM and Named Pipe Setup

This procedure is verified with np21w rev103 (`np21x64w.exe`). PC98IMEBridge uses the
PC-98's normal RS-232C interface, shown as the `COM1` page in np21w, and maps
that guest interface to a Windows Named Pipe.

## Configure the main transport

1. In np21w, open **Device > Serial/Parallel option...**.
2. Select the **COM1** page. Do not select the `CH.1`, `CH.2`, or `COM3`
   alternatives for the main PC98IMEBridge transport.
3. Set **Port** to `PIPE`. The physical serial options are replaced by the
   Named Pipe fields.
4. Set **Pipe Name** to `NP2-NamedPipe`.
5. Set **Server Name** to `.` for the local Windows machine.
6. Confirm that the read-only preview is
   `\\.\pipe\NP2-NamedPipe`, then select **OK**.

Enter only `NP2-NamedPipe` in **Pipe Name**, not the full
`\\.\pipe\NP2-NamedPipe` path. The speed controls may be hidden when `PIPE` is
selected. The verified configuration retains 19200 bps, while the guest-side
timing is selected by `TIMER_COUNT=0008` in `IME98.CFG`.

Start the Bridge before np21w. np21w exposes the pipe endpoint and the Bridge
connects to it as a client:

```text
ImeDosBridge.exe --pipe NP2-NamedPipe --pipe-client
```

## Change the main pipe name

The pipe name is a Windows-side endpoint name. To change it, use the same bare
name in both np21w and the Bridge command. For example:

```text
np21w COM1 Pipe Name: PC98IMEBridge-PC98
np21w COM1 Server Name: .
ImeDosBridge.exe --pipe PC98IMEBridge-PC98 --pipe-client
```

`IME98.CFG` does not contain the pipe name and does not need to change. Close
and restart both programs after changing the name, again starting the Bridge
first. The repository's `start-pc98-test.bat` uses the default
`NP2-NamedPipe`; use a manual Bridge launch when testing a custom name.

The Bridge supports only a local np21w pipe, so keep **Server Name** as `.`.
Remote Named Pipe servers are outside the supported configuration.

## COM names versus the guest interface

The **Port** list on np21w's `COM1` page selects the Windows-side destination:
`PIPE`, a physical Windows `COM1` through `COM4`, or another backend. Choosing
physical `COM3` there does not rename the PC-98 interface; it routes the guest
COM1 traffic to a Windows physical port. PC98IMEBridge's qualified path requires
`PIPE`.

The PC98IMEBridge guest protocol remains on normal PC-98 COM1, using data/status ports
`0030h`/`0032h`. Do not change `DATA_PORT` and `STATUS_PORT` merely to match a
Windows COM number. The `0130h`/`0132h` pair is np21w FIFO mode and is not an
interchangeable COM1 setting.

Moving the main protocol to the PC-9861K `CH.1` or `CH.2` pages is not a
supported rename. Those are different PC-98 devices with different ports and
interrupt behavior.

## Optional diagnostic pipe

Development builds can carry diagnostic messages over PC-9861K CH.1. In the
same **Serial/Parallel option** dialog:

1. Select the **CH.1** page (stored by np21w as its `com2` configuration).
2. Set **Port** to `PIPE`.
3. Set **Pipe Name** to `NP2-ImeDebug` and **Server Name** to `.`.
4. Start the Bridge with `--debug-pipe NP2-ImeDebug` in addition to the main
   pipe options.

To rename this diagnostic pipe, change the CH.1 **Pipe Name** and the Bridge's
`--debug-pipe` value together. The diagnostic channel is optional and does not
replace the COM1 protocol pipe.

## Saved settings and troubleshooting

For `np21x64w.exe`, np21w normally persists these values in `np21x64w.ini`:

```ini
com1port=7
com1_bps=19200
com1pnam=NP2-NamedPipe
com1psrv=.
```

`com1port=7` is np21w rev103's saved value for `PIPE`. Prefer the GUI over
editing the INI directly, and never edit it while np21w is running. Other np21w
executables use their corresponding INI filenames.

If the Bridge repeatedly reports a pipe timeout:

- verify that the main transport was configured on the `COM1` page;
- verify that **Port** is `PIPE` and both pipe names have identical spelling;
- verify that **Server Name** is `.`;
- stop only this Bridge and np21w, then start the Bridge before np21w; and
- do not reuse the main pipe name for the optional diagnostic channel.

The dialog names, `PIPE` value, and INI keys above are verified for np21w
rev103. Other revisions may use different labels or saved values.
