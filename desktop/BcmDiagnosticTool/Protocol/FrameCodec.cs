namespace BcmDiagnosticTool.Protocol;

/// <summary>
/// Encodes frames onto the wire, per BCM-ICD-001 section 3.
/// </summary>
public static class FrameCodec
{
    /// <summary>
    /// Build the on-the-wire bytes for a request.
    /// </summary>
    /// <exception cref="ArgumentException">payload longer than 32 bytes.</exception>
    public static byte[] Encode(byte cmd, ReadOnlySpan<byte> payload)
    {
        if (payload.Length > Icd.MaxPayload)
        {
            throw new ArgumentException(
                $"payload is {payload.Length} bytes, maximum is {Icd.MaxPayload}",
                nameof(payload));
        }

        var frame = new byte[payload.Length + Icd.FrameOverhead];
        int i = 0;

        frame[i++] = Icd.Header;
        frame[i++] = cmd;
        frame[i++] = (byte)payload.Length;
        foreach (byte b in payload) { frame[i++] = b; }

        // CRC covers CMD, LEN and PAYLOAD - not the header or the footer.
        frame[i++] = Crc8.Compute(frame.AsSpan(1, payload.Length + 2));
        frame[i] = Icd.Footer;

        return frame;
    }

    public static byte[] Encode(Cmd cmd, params byte[] payload) =>
        Encode((byte)cmd, payload);
}

/// <summary>
/// Incremental byte-at-a-time parser, mirroring the firmware's state machine.
///
/// On a bad checksum or a missing footer it reports the error and resumes
/// hunting for the next header, so a corrupted frame costs one frame rather
/// than desynchronising the link.
/// </summary>
public sealed class FrameParser
{
    public enum Result
    {
        NeedMore,
        Complete,
        ChecksumError,
        FramingError,
        Overflow,
    }

    private enum State { Header, Command, Length, Payload, Checksum, Footer }

    private State _state = State.Header;
    private byte _cmd;
    private byte _len;
    private byte[] _payload = Array.Empty<byte>();
    private int _index;
    private readonly List<byte> _crcInput = new();

    public Frame? Frame { get; private set; }
    public uint FramesOk { get; private set; }
    public uint FramesBad { get; private set; }

    public void Reset()
    {
        _state = State.Header;
        _index = 0;
        _crcInput.Clear();
    }

    private Result Fail(Result why)
    {
        FramesBad++;
        Reset();
        return why;
    }

    public Result Feed(byte b)
    {
        switch (_state)
        {
            case State.Header:
                // Anything that is not a header is discarded. This is how the
                // parser recovers from corruption or a mid-stream connect.
                if (b == Icd.Header)
                {
                    _crcInput.Clear();
                    _index = 0;
                    _state = State.Command;
                }
                return Result.NeedMore;

            case State.Command:
                _cmd = b;
                _crcInput.Add(b);
                _state = State.Length;
                return Result.NeedMore;

            case State.Length:
                if (b > Icd.MaxPayload)
                {
                    // A bogus length would make us swallow arbitrary bytes.
                    return Fail(Result.Overflow);
                }
                _len = b;
                _crcInput.Add(b);
                _payload = new byte[_len];
                _index = 0;
                _state = _len == 0 ? State.Checksum : State.Payload;
                return Result.NeedMore;

            case State.Payload:
                _payload[_index++] = b;
                _crcInput.Add(b);
                if (_index >= _len) { _state = State.Checksum; }
                return Result.NeedMore;

            case State.Checksum:
                if (b != Crc8.Compute(System.Runtime.InteropServices
                                            .CollectionsMarshal.AsSpan(_crcInput)))
                {
                    return Fail(Result.ChecksumError);
                }
                _state = State.Footer;
                return Result.NeedMore;

            case State.Footer:
                if (b != Icd.Footer) { return Fail(Result.FramingError); }
                Frame = new Frame { Cmd = _cmd, Payload = _payload };
                FramesOk++;
                Reset();
                return Result.Complete;

            default:
                return Fail(Result.FramingError);
        }
    }
}
