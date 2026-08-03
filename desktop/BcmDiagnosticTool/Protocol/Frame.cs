namespace BcmDiagnosticTool.Protocol;

/// <summary>A decoded BCM-ICD-001 frame.</summary>
public sealed class Frame
{
    public byte Cmd { get; init; }
    public byte[] Payload { get; init; } = Array.Empty<byte>();

    public bool IsResponse => (Cmd & Icd.ResponseFlag) != 0;

    /// <summary>The request id this frame answers (response flag stripped).</summary>
    public byte RequestCmd => (byte)(Cmd & ~Icd.ResponseFlag);

    /// <summary>Every response begins with a status byte.</summary>
    public Status Status =>
        Payload.Length > 0 ? (Status)Payload[0] : Protocol.Status.BadLength;

    public bool Ok => IsResponse && Payload.Length > 0 && Status == Protocol.Status.Ok;

    /// <summary>Big-endian 16-bit value at <paramref name="offset"/>.</summary>
    public ushort ReadUInt16(int offset) =>
        (ushort)((Payload[offset] << 8) | Payload[offset + 1]);

    /// <summary>Big-endian 32-bit value at <paramref name="offset"/>.</summary>
    public uint ReadUInt32(int offset) =>
        (uint)((Payload[offset] << 24) | (Payload[offset + 1] << 16) |
               (Payload[offset + 2] << 8) | Payload[offset + 3]);

    public override string ToString() =>
        $"CMD=0x{Cmd:X2} LEN={Payload.Length} [{Convert.ToHexString(Payload)}]";
}
