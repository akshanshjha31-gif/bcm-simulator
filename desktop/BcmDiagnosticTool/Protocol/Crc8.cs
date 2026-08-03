namespace BcmDiagnosticTool.Protocol;

/// <summary>
/// CRC-8 frame check sequence, per BCM-ICD-001 section 3.2.
///
/// Polynomial 0x07 (CRC-8/ATM), initial value 0x00, no reflection, no final
/// XOR. Written from the ICD rather than ported from the firmware, so that
/// agreement between the two is evidence the document is unambiguous.
/// </summary>
public static class Crc8
{
    public const byte Polynomial = 0x07;
    public const byte Initial = 0x00;

    public static byte Compute(ReadOnlySpan<byte> data)
    {
        byte crc = Initial;
        foreach (byte b in data)
        {
            crc ^= b;
            for (int bit = 0; bit < 8; bit++)
            {
                crc = (crc & 0x80) != 0
                    ? (byte)((crc << 1) ^ Polynomial)
                    : (byte)(crc << 1);
            }
        }
        return crc;
    }
}
