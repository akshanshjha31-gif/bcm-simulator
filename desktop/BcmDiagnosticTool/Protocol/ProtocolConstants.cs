namespace BcmDiagnosticTool.Protocol;

/// <summary>
/// Wire constants from BCM-ICD-001. Mirrors firmware/services/protocol.h.
/// </summary>
public static class Icd
{
    public const byte Header = 0xAA;
    public const byte Footer = 0x55;
    public const byte MaxPayload = 32;
    public const byte FrameOverhead = 5;
    public const int MaxFrameSize = MaxPayload + FrameOverhead;

    /// <summary>OR'd into the command id of a response frame.</summary>
    public const byte ResponseFlag = 0x80;
}

public enum Cmd : byte
{
    Ping = 0x01,
    GetVersion = 0x02,
    GetStatus = 0x03,
    SetLamp = 0x04,
    GetLamp = 0x05,
    DoorLock = 0x06,
    DoorUnlock = 0x07,
    GetBattery = 0x08,
    GetDtc = 0x09,
    ClearDtc = 0x0A,
    Reset = 0x0B,
    Heartbeat = 0x0C,

    /// <summary>Unsolicited, BCM to host. Always carries the response flag.</summary>
    LogEvent = 0x0D,
}

public enum Status : byte
{
    Ok = 0x00,
    UnknownCmd = 0x01,
    BadLength = 0x02,
    BadParameter = 0x03,
    Busy = 0x04,
    NotPermitted = 0x05,
}

/// <summary>Lamp identifiers, BCM-ICD-001 section 4.2.</summary>
public enum LampId : byte
{
    Ignition = 0,
    Drl = 1,
    LowBeam = 2,
    HighBeam = 3,
    IndLeft = 4,
    IndRight = 5,
    Hazard = 6,
    Brake = 7,
    Reverse = 8,
    DoorLock = 9,
}

/// <summary>Switch identifiers, BCM-ICD-001 section 4.3.</summary>
public enum SwitchId : byte
{
    Ignition = 0,
    IndLeft = 1,
    IndRight = 2,
    Hazard = 3,
    Brake = 4,
    DoorLock = 5,
}

public enum PowerState : byte
{
    Sleep = 0,
    Wake = 1,
    Run = 2,
    Shutdown = 3,
}

/// <summary>Diagnostic trouble codes, from firmware/services/fault_mgr.h.</summary>
public enum Dtc : byte
{
    None = 0x00,
    BatteryLow = 0x01,
    CommsLost = 0x02,
    LockRefusedDoorOpen = 0x03,
    WatchdogReset = 0x04,
    UartOverrun = 0x05,
    PostFailed = 0x06,
}

/// <summary>Event identifiers carried by LOG_EVENT, from services/logger.h.</summary>
public enum LogEventId : byte
{
    None = 0,
    Startup,
    PostPassed,
    PostFailed,
    IgnitionOn,
    IgnitionOff,
    LightMode,
    IndicatorLeft,
    IndicatorRight,
    HazardOn,
    HazardOff,
    BrakeApplied,
    BrakeReleased,
    DoorLocked,
    DoorUnlocked,
    LockRefused,
    AutoLocked,
    BatteryLow,
    BatteryOk,
    CommsLost,
    CommsRestored,
    WatchdogReset,
    LoadShed,
}

public static class Names
{
    public static string Of(LampId id) => id switch
    {
        LampId.Ignition => "Ignition",
        LampId.Drl => "DRL",
        LampId.LowBeam => "Low Beam",
        LampId.HighBeam => "High Beam",
        LampId.IndLeft => "Indicator L",
        LampId.IndRight => "Indicator R",
        LampId.Hazard => "Hazard",
        LampId.Brake => "Brake",
        LampId.Reverse => "Reverse",
        LampId.DoorLock => "Door Lock",
        _ => id.ToString(),
    };

    public static string Of(SwitchId id) => id switch
    {
        SwitchId.Ignition => "Ignition",
        SwitchId.IndLeft => "Indicator L",
        SwitchId.IndRight => "Indicator R",
        SwitchId.Hazard => "Hazard",
        SwitchId.Brake => "Brake",
        SwitchId.DoorLock => "Door Lock",
        _ => id.ToString(),
    };

    public static string Of(Dtc code) => code switch
    {
        Dtc.BatteryLow => "Battery low",
        Dtc.CommsLost => "Communication lost",
        Dtc.LockRefusedDoorOpen => "Lock refused - door open",
        Dtc.WatchdogReset => "Watchdog reset",
        Dtc.UartOverrun => "UART overrun",
        Dtc.PostFailed => "POST failed",
        _ => $"Unknown (0x{(byte)code:X2})",
    };

    public static string Of(LogEventId ev) => ev switch
    {
        LogEventId.Startup => "Startup",
        LogEventId.PostPassed => "POST passed",
        LogEventId.PostFailed => "POST FAILED",
        LogEventId.IgnitionOn => "Ignition ON",
        LogEventId.IgnitionOff => "Ignition off",
        LogEventId.LightMode => "Light mode changed",
        LogEventId.IndicatorLeft => "Indicator left",
        LogEventId.IndicatorRight => "Indicator right",
        LogEventId.HazardOn => "Hazard ON",
        LogEventId.HazardOff => "Hazard off",
        LogEventId.BrakeApplied => "Brake applied",
        LogEventId.BrakeReleased => "Brake released",
        LogEventId.DoorLocked => "Door locked",
        LogEventId.DoorUnlocked => "Door unlocked",
        LogEventId.LockRefused => "LOCK REFUSED (door open)",
        LogEventId.AutoLocked => "Auto-locked",
        LogEventId.BatteryLow => "Battery LOW",
        LogEventId.BatteryOk => "Battery ok",
        LogEventId.CommsLost => "Comms LOST",
        LogEventId.CommsRestored => "Comms restored",
        LogEventId.WatchdogReset => "Watchdog reset",
        LogEventId.LoadShed => "Load shed",
        _ => $"Event 0x{(byte)ev:X2}",
    };

    public static string Of(Status s) => s switch
    {
        Status.Ok => "OK",
        Status.UnknownCmd => "Unknown command",
        Status.BadLength => "Bad length",
        Status.BadParameter => "Bad parameter",
        Status.Busy => "Busy",
        Status.NotPermitted => "Not permitted",
        _ => $"Status 0x{(byte)s:X2}",
    };
}
