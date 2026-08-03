using BcmDiagnosticTool.Protocol;
using Xunit;

namespace BcmDiagnosticTool.Tests;

/// <summary>
/// Conformance of the C# codec against BCM-ICD-001.
///
/// The firmware asserts the same worked examples in tests/test_icd_examples.cpp.
/// Two independent implementations agreeing on the same documented bytes is
/// the evidence that the ICD is unambiguous - which is the whole reason the
/// document exists.
/// </summary>
public class IcdConformanceTests
{
    [Fact]
    public void Crc8_MatchesPublishedCheckValue()
    {
        // The standard check value for polynomial 0x07, init 0x00.
        byte[] input = "123456789"u8.ToArray();
        Assert.Equal(0xF4, Crc8.Compute(input));
    }

    [Fact]
    public void Crc8_MatchesFirmwareVector()
    {
        byte[] abc = { (byte)'A', (byte)'B', (byte)'C' };
        Assert.Equal(0x52, Crc8.Compute(abc));
    }

    [Fact]
    public void Crc8_DetectsByteReordering()
    {
        // The case a plain XOR checksum cannot see at all.
        byte[] a = { 0x01, 0x02, 0x03 };
        byte[] b = { 0x03, 0x02, 0x01 };
        Assert.NotEqual(Crc8.Compute(a), Crc8.Compute(b));
    }

    [Fact]
    public void Icd_5_1_PingRequestBytes()
    {
        byte[] expected = { 0xAA, 0x01, 0x00, 0x15, 0x55 };
        Assert.Equal(expected, FrameCodec.Encode(Cmd.Ping));
    }

    [Fact]
    public void Icd_5_2_SetLampRequestBytes()
    {
        // Brake lamp (id 7) on.
        byte[] expected = { 0xAA, 0x04, 0x02, 0x07, 0x01, 0xE2, 0x55 };
        Assert.Equal(expected, FrameCodec.Encode(Cmd.SetLamp, 0x07, 0x01));
    }

    [Fact]
    public void Icd_5_3_MalformedRequestBytes()
    {
        byte[] expected = { 0xAA, 0x04, 0x01, 0x07, 0xAB, 0x55 };
        Assert.Equal(expected, FrameCodec.Encode(Cmd.SetLamp, 0x07));
    }

    [Fact]
    public void Icd_3_FrameSizeBounds()
    {
        Assert.Equal(5, FrameCodec.Encode(Cmd.Ping).Length);
        Assert.Equal(37, FrameCodec.Encode(Cmd.Ping, new byte[32]).Length);
    }

    [Fact]
    public void Encode_RejectsOversizedPayload()
    {
        Assert.Throws<ArgumentException>(() =>
            FrameCodec.Encode((byte)Cmd.Ping, new byte[33]));
    }

    /* ---- Parser ---------------------------------------------------------- */

    private static FrameParser.Result FeedAll(FrameParser p, byte[] bytes)
    {
        var outcome = FrameParser.Result.NeedMore;
        foreach (byte b in bytes)
        {
            FrameParser.Result r = p.Feed(b);
            // Errors are raised mid-frame and the bytes after them answer
            // NeedMore, so remember the last conclusive outcome.
            if (r != FrameParser.Result.NeedMore) { outcome = r; }
        }
        return outcome;
    }

    [Fact]
    public void Parser_DecodesTheDocumentedPingResponse()
    {
        byte[] wire = { 0xAA, 0x81, 0x01, 0x00, 0x75, 0x55 };

        var parser = new FrameParser();
        Assert.Equal(FrameParser.Result.Complete, FeedAll(parser, wire));

        Frame f = parser.Frame!;
        Assert.True(f.IsResponse);
        Assert.Equal((byte)Cmd.Ping, f.RequestCmd);
        Assert.Equal(Status.Ok, f.Status);
    }

    [Fact]
    public void Parser_RoundTripsEveryCommand()
    {
        foreach (Cmd cmd in Enum.GetValues<Cmd>())
        {
            byte[] wire = FrameCodec.Encode(cmd, 0x11, 0x22);

            var parser = new FrameParser();
            Assert.Equal(FrameParser.Result.Complete, FeedAll(parser, wire));
            Assert.Equal((byte)cmd, parser.Frame!.Cmd);
            Assert.Equal(new byte[] { 0x11, 0x22 }, parser.Frame.Payload);
        }
    }

    [Fact]
    public void Parser_RejectsACorruptedPayload()
    {
        byte[] wire = FrameCodec.Encode(Cmd.SetLamp, 0x07, 0x01);
        wire[3] ^= 0x01;

        var parser = new FrameParser();
        Assert.Equal(FrameParser.Result.ChecksumError, FeedAll(parser, wire));
        Assert.Equal(1u, parser.FramesBad);
    }

    [Fact]
    public void Parser_RejectsAnImpossibleLength()
    {
        var parser = new FrameParser();
        parser.Feed(0xAA);
        parser.Feed(0x01);
        Assert.Equal(FrameParser.Result.Overflow, parser.Feed(33));
    }

    [Fact]
    public void Parser_CarriesHeaderAndFooterBytesInThePayload()
    {
        // Payloads are not escaped - the case that breaks a naive parser
        // scanning for the footer.
        byte[] wire = FrameCodec.Encode(Cmd.Ping, 0xAA, 0x55, 0xAA);

        var parser = new FrameParser();
        Assert.Equal(FrameParser.Result.Complete, FeedAll(parser, wire));
        Assert.Equal(new byte[] { 0xAA, 0x55, 0xAA }, parser.Frame!.Payload);
    }

    [Fact]
    public void Parser_DiscardsLeadingNoise()
    {
        var stream = new List<byte> { 0x00, 0xFF, 0x12, 0x99 };
        stream.AddRange(FrameCodec.Encode(Cmd.GetVersion));

        var parser = new FrameParser();
        Assert.Equal(FrameParser.Result.Complete, FeedAll(parser, stream.ToArray()));
        Assert.Equal((byte)Cmd.GetVersion, parser.Frame!.Cmd);
    }

    [Fact]
    public void Parser_ResynchronisesAfterACorruptFrame()
    {
        // One bad frame must cost exactly one frame, not the session.
        byte[] bad = FrameCodec.Encode(Cmd.SetLamp, 0x01, 0x01);
        bad[3] ^= 0xFF;

        var parser = new FrameParser();
        Assert.Equal(FrameParser.Result.ChecksumError, FeedAll(parser, bad));
        Assert.Equal(FrameParser.Result.Complete,
                     FeedAll(parser, FrameCodec.Encode(Cmd.GetStatus)));
        Assert.Equal((byte)Cmd.GetStatus, parser.Frame!.Cmd);
    }

    [Fact]
    public void Icd_4_0_LogEventIsAlwaysAResponse()
    {
        // Unsolicited events carry the response flag, so they can never be
        // mistaken for a request (BCM-ICD-001 section 4.0).
        byte cmd = (byte)Cmd.LogEvent | Icd.ResponseFlag;
        Assert.Equal(0x8D, cmd);

        byte[] wire = FrameCodec.Encode(cmd,
            new byte[] { 0x00, 0x00, 0x9F, 0xC4, (byte)LogEventId.BrakeApplied, 0x00 });

        var parser = new FrameParser();
        Assert.Equal(FrameParser.Result.Complete, FeedAll(parser, wire));

        Frame f = parser.Frame!;
        Assert.True(f.IsResponse);
        Assert.Equal(40900u, f.ReadUInt32(0));
        Assert.Equal(LogEventId.BrakeApplied, (LogEventId)f.Payload[4]);
    }

    [Fact]
    public void Icd_4_2_LampIdsMatchTheDocument()
    {
        Assert.Equal(0, (byte)LampId.Ignition);
        Assert.Equal(2, (byte)LampId.LowBeam);
        Assert.Equal(7, (byte)LampId.Brake);
        Assert.Equal(8, (byte)LampId.Reverse);
        Assert.Equal(9, (byte)LampId.DoorLock);
    }
}
