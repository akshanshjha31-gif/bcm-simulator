using System.Collections.Concurrent;
using System.IO.Ports;
using BcmDiagnosticTool.Protocol;

namespace BcmDiagnosticTool.Services;

/// <summary>Raised for every unsolicited LOG_EVENT frame.</summary>
public sealed class BcmEventArgs : EventArgs
{
    public uint TimestampMs { get; init; }
    public LogEventId Event { get; init; }
    public byte Arg { get; init; }
}

/// <summary>
/// Talks BCM-ICD-001 over a serial port.
///
/// A background reader owns the port and decodes frames continuously.
/// Responses are matched to the outstanding request by command id, and
/// anything else - notably unsolicited LOG_EVENT frames - is dispatched as an
/// event. BCM-ICD-001 section 4.0 requires exactly this: a host must not
/// assume the next frame is the answer to what it just sent.
/// </summary>
public sealed class BcmClient : IDisposable
{
    private readonly FrameParser _parser = new();
    private readonly BlockingCollection<Frame> _responses = new(new ConcurrentQueue<Frame>());
    private readonly SemaphoreSlim _requestLock = new(1, 1);

    private SerialPort? _port;
    private CancellationTokenSource? _cts;
    private Task? _reader;

    public bool IsConnected => _port?.IsOpen == true;
    public string? PortName => _port?.PortName;

    public uint FramesOk => _parser.FramesOk;
    public uint FramesBad => _parser.FramesBad;

    public event EventHandler<BcmEventArgs>? EventReceived;
    public event EventHandler<string>? Diagnostic;

    public static string[] AvailablePorts() => SerialPort.GetPortNames();

    public void Connect(string portName, int baud = 115200)
    {
        Disconnect();

        _port = new SerialPort(portName, baud, Parity.None, 8, StopBits.One)
        {
            ReadTimeout = 200,
            WriteTimeout = 500,
        };
        _port.Open();
        _port.DiscardInBuffer();

        _cts = new CancellationTokenSource();
        _reader = Task.Run(() => ReadLoop(_cts.Token));

        Diagnostic?.Invoke(this, $"Connected to {portName} @ {baud} 8N1");
    }

    public void Disconnect()
    {
        _cts?.Cancel();
        try { _reader?.Wait(1000); } catch { /* shutting down */ }

        if (_port is not null)
        {
            if (_port.IsOpen) { _port.Close(); }
            _port.Dispose();
            _port = null;
        }

        _cts?.Dispose();
        _cts = null;
        _reader = null;

        while (_responses.TryTake(out _)) { }
        _parser.Reset();
    }

    private void ReadLoop(CancellationToken token)
    {
        var buffer = new byte[256];

        while (!token.IsCancellationRequested)
        {
            int read;
            try
            {
                read = _port!.Read(buffer, 0, buffer.Length);
            }
            catch (TimeoutException)
            {
                continue;
            }
            catch (Exception ex)
            {
                if (!token.IsCancellationRequested)
                {
                    Diagnostic?.Invoke(this, $"Serial read failed: {ex.Message}");
                }
                return;
            }

            for (int i = 0; i < read; i++)
            {
                if (_parser.Feed(buffer[i]) != FrameParser.Result.Complete) { continue; }

                Frame frame = _parser.Frame!;

                if (frame.Cmd == ((byte)Cmd.LogEvent | Icd.ResponseFlag))
                {
                    DispatchEvent(frame);
                }
                else
                {
                    _responses.Add(frame, CancellationToken.None);
                }
            }
        }
    }

    private void DispatchEvent(Frame frame)
    {
        if (frame.Payload.Length < 6) { return; }

        EventReceived?.Invoke(this, new BcmEventArgs
        {
            TimestampMs = frame.ReadUInt32(0),
            Event = (LogEventId)frame.Payload[4],
            Arg = frame.Payload[5],
        });
    }

    /// <summary>
    /// Send a request and wait for its matching response.
    /// </summary>
    /// <remarks>
    /// Frames that are not the expected reply are discarded here rather than
    /// returned, so a stale response can never be mistaken for the answer to
    /// a later request.
    /// </remarks>
    public async Task<Frame?> RequestAsync(Cmd cmd, byte[]? payload = null,
                                           int timeoutMs = 1000)
    {
        if (!IsConnected) { return null; }

        await _requestLock.WaitAsync().ConfigureAwait(false);
        try
        {
            byte[] frame = FrameCodec.Encode(cmd, payload ?? Array.Empty<byte>());
            _port!.Write(frame, 0, frame.Length);

            byte expected = (byte)((byte)cmd | Icd.ResponseFlag);
            var deadline = DateTime.UtcNow.AddMilliseconds(timeoutMs);

            while (DateTime.UtcNow < deadline)
            {
                var remaining = (int)(deadline - DateTime.UtcNow).TotalMilliseconds;
                if (remaining <= 0) { break; }

                if (!_responses.TryTake(out Frame? candidate, remaining)) { continue; }
                if (candidate.Cmd == expected) { return candidate; }

                // Not ours: a late reply to a previous request. Drop it.
                Diagnostic?.Invoke(this,
                    $"Discarded unexpected frame 0x{candidate.Cmd:X2} " +
                    $"while awaiting 0x{expected:X2}");
            }

            Diagnostic?.Invoke(this, $"Timeout waiting for reply to {cmd}");
            return null;
        }
        catch (Exception ex)
        {
            Diagnostic?.Invoke(this, $"Request {cmd} failed: {ex.Message}");
            return null;
        }
        finally
        {
            _requestLock.Release();
        }
    }

    /* ---- Typed helpers ---------------------------------------------------- */

    public async Task<(bool ok, string version)> GetVersionAsync()
    {
        Frame? r = await RequestAsync(Cmd.GetVersion).ConfigureAwait(false);
        if (r is null || !r.Ok || r.Payload.Length < 4) { return (false, "-"); }
        return (true, $"{r.Payload[1]}.{r.Payload[2]}.{r.Payload[3]}");
    }

    public sealed record BcmStatus(ushort LampBitmap, byte SwitchBitmap, PowerState Power);

    public async Task<BcmStatus?> GetStatusAsync()
    {
        Frame? r = await RequestAsync(Cmd.GetStatus).ConfigureAwait(false);
        if (r is null || !r.Ok || r.Payload.Length < 5) { return null; }
        return new BcmStatus(r.ReadUInt16(1), r.Payload[3], (PowerState)r.Payload[4]);
    }

    public async Task<ushort?> GetBatteryAsync()
    {
        Frame? r = await RequestAsync(Cmd.GetBattery).ConfigureAwait(false);
        if (r is null || !r.Ok || r.Payload.Length < 3) { return null; }
        return r.ReadUInt16(1);
    }

    public async Task<Status> SetLampAsync(LampId lamp, bool on)
    {
        Frame? r = await RequestAsync(Cmd.SetLamp, new[] { (byte)lamp, (byte)(on ? 1 : 0) })
                        .ConfigureAwait(false);
        return r?.Status ?? Status.Busy;
    }

    public async Task<Status> DoorAsync(bool locked)
    {
        Frame? r = await RequestAsync(locked ? Cmd.DoorLock : Cmd.DoorUnlock)
                        .ConfigureAwait(false);
        return r?.Status ?? Status.Busy;
    }

    public async Task<IReadOnlyList<Dtc>> GetDtcsAsync()
    {
        Frame? r = await RequestAsync(Cmd.GetDtc).ConfigureAwait(false);
        if (r is null || !r.Ok || r.Payload.Length < 2) { return Array.Empty<Dtc>(); }

        int count = Math.Min(r.Payload[1], r.Payload.Length - 2);
        var codes = new List<Dtc>(count);
        for (int i = 0; i < count; i++) { codes.Add((Dtc)r.Payload[2 + i]); }
        return codes;
    }

    public async Task<Status> ClearDtcsAsync()
    {
        Frame? r = await RequestAsync(Cmd.ClearDtc).ConfigureAwait(false);
        return r?.Status ?? Status.Busy;
    }

    public async Task<bool> PingAsync()
    {
        Frame? r = await RequestAsync(Cmd.Ping, new byte[] { 0xDE, 0xAD }, 500)
                        .ConfigureAwait(false);
        return r is not null && r.Ok && r.Payload.Length >= 3
               && r.Payload[1] == 0xDE && r.Payload[2] == 0xAD;
    }

    public void Dispose() => Disconnect();
}
