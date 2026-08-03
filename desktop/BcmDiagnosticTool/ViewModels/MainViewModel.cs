using System.Collections.ObjectModel;
using System.Windows.Threading;
using BcmDiagnosticTool.Protocol;
using BcmDiagnosticTool.Services;

namespace BcmDiagnosticTool.ViewModels;

public sealed class LampViewModel : ObservableObject
{
    private bool _isOn;

    public LampViewModel(LampId id)
    {
        Id = id;
        Name = Names.Of(id);
    }

    public LampId Id { get; }
    public string Name { get; }

    public bool IsOn
    {
        get => _isOn;
        set => Set(ref _isOn, value);
    }
}

public sealed class SwitchViewModel : ObservableObject
{
    private bool _isPressed;

    public SwitchViewModel(SwitchId id)
    {
        Id = id;
        Name = Names.Of(id);
    }

    public SwitchId Id { get; }
    public string Name { get; }

    public bool IsPressed
    {
        get => _isPressed;
        set => Set(ref _isPressed, value);
    }
}

public sealed class EventEntry
{
    public string Timestamp { get; init; } = "";
    public string Description { get; init; } = "";
    public bool IsFault { get; init; }
}

public sealed class MainViewModel : ObservableObject, IDisposable
{
    private readonly BcmClient _client = new();
    private readonly DispatcherTimer _poll;

    private string? _selectedPort;
    private bool _isConnected;
    private string _connectionState = "Disconnected";
    private string _firmwareVersion = "-";
    private string _powerState = "-";
    private double _batteryPercent;
    private string _statistics = "";
    private bool _linkHealthy;

    public MainViewModel()
    {
        foreach (LampId id in Enum.GetValues<LampId>()) { Lamps.Add(new LampViewModel(id)); }
        foreach (SwitchId id in Enum.GetValues<SwitchId>()) { Switches.Add(new SwitchViewModel(id)); }

        _client.EventReceived += OnEventReceived;
        _client.Diagnostic += (_, message) => AddEvent(null, message, false);

        ConnectCommand = new RelayCommand(_ => ToggleConnectionAsync(),
                                          _ => IsConnected || SelectedPort is not null);
        SetLampCommand = new RelayCommand(SetLampAsync, _ => IsConnected);
        LockCommand = new RelayCommand(_ => DoorAsync(true), _ => IsConnected);
        UnlockCommand = new RelayCommand(_ => DoorAsync(false), _ => IsConnected);
        ClearDtcCommand = new RelayCommand(_ => ClearDtcAsync(), _ => IsConnected);
        RefreshPortsCommand = new RelayCommand(_ => RefreshPorts());
        ClearEventsCommand = new RelayCommand(_ => Events.Clear());

        // AFTER the commands exist: the SelectedPort setter notifies
        // ConnectCommand, so populating the list any earlier dereferences null.
        RefreshPorts();

        // 200 ms keeps the dashboard feeling live without saturating a
        // 115200 link that is also carrying unsolicited event frames.
        _poll = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(200) };
        _poll.Tick += async (_, _) => await PollAsync();
    }

    public ObservableCollection<string> Ports { get; } = new();
    public ObservableCollection<LampViewModel> Lamps { get; } = new();
    public ObservableCollection<SwitchViewModel> Switches { get; } = new();
    public ObservableCollection<EventEntry> Events { get; } = new();
    public ObservableCollection<string> Dtcs { get; } = new();

    public RelayCommand ConnectCommand { get; }
    public RelayCommand SetLampCommand { get; }
    public RelayCommand LockCommand { get; }
    public RelayCommand UnlockCommand { get; }
    public RelayCommand ClearDtcCommand { get; }
    public RelayCommand RefreshPortsCommand { get; }
    public RelayCommand ClearEventsCommand { get; }

    public string? SelectedPort
    {
        get => _selectedPort;
        set { if (Set(ref _selectedPort, value)) { ConnectCommand.RaiseCanExecuteChanged(); } }
    }

    public bool IsConnected
    {
        get => _isConnected;
        private set
        {
            if (!Set(ref _isConnected, value)) { return; }
            OnPropertyChanged(nameof(ConnectButtonText));
            SetLampCommand.RaiseCanExecuteChanged();
            LockCommand.RaiseCanExecuteChanged();
            UnlockCommand.RaiseCanExecuteChanged();
            ClearDtcCommand.RaiseCanExecuteChanged();
        }
    }

    public string ConnectButtonText => IsConnected ? "Disconnect" : "Connect";

    public string ConnectionState
    {
        get => _connectionState;
        private set => Set(ref _connectionState, value);
    }

    public bool LinkHealthy
    {
        get => _linkHealthy;
        private set => Set(ref _linkHealthy, value);
    }

    public string FirmwareVersion
    {
        get => _firmwareVersion;
        private set => Set(ref _firmwareVersion, value);
    }

    public string PowerState
    {
        get => _powerState;
        private set => Set(ref _powerState, value);
    }

    public double BatteryPercent
    {
        get => _batteryPercent;
        private set => Set(ref _batteryPercent, value);
    }

    public string Statistics
    {
        get => _statistics;
        private set => Set(ref _statistics, value);
    }

    public void RefreshPorts()
    {
        string? previous = SelectedPort;
        Ports.Clear();
        foreach (string p in BcmClient.AvailablePorts()) { Ports.Add(p); }

        SelectedPort = previous is not null && Ports.Contains(previous)
            ? previous
            : Ports.FirstOrDefault();
    }

    private async Task ToggleConnectionAsync()
    {
        if (IsConnected)
        {
            _poll.Stop();
            _client.Disconnect();
            IsConnected = false;
            LinkHealthy = false;
            ConnectionState = "Disconnected";
            FirmwareVersion = "-";
            PowerState = "-";
            return;
        }

        if (SelectedPort is null) { return; }

        try
        {
            _client.Connect(SelectedPort);
            IsConnected = true;
            ConnectionState = $"Connected to {SelectedPort}";

            (bool ok, string version) = await _client.GetVersionAsync();
            FirmwareVersion = ok ? $"v{version}" : "unreachable";
            LinkHealthy = ok;

            if (!ok)
            {
                AddEvent(null, "Connected, but the BCM did not answer GET_VERSION", true);
            }

            await RefreshDtcsAsync();
            _poll.Start();
        }
        catch (Exception ex)
        {
            ConnectionState = "Connect failed";
            AddEvent(null, $"Connect failed: {ex.Message}", true);
            IsConnected = false;
        }
    }

    private async Task PollAsync()
    {
        if (!IsConnected) { return; }

        BcmClient.BcmStatus? status = await _client.GetStatusAsync();
        if (status is null)
        {
            LinkHealthy = false;
            ConnectionState = $"{SelectedPort} - no response";
            return;
        }

        LinkHealthy = true;
        ConnectionState = $"Connected to {SelectedPort}";

        foreach (LampViewModel lamp in Lamps)
        {
            lamp.IsOn = (status.LampBitmap & (1 << (byte)lamp.Id)) != 0;
        }
        foreach (SwitchViewModel sw in Switches)
        {
            sw.IsPressed = (status.SwitchBitmap & (1 << (byte)sw.Id)) != 0;
        }
        PowerState = status.Power.ToString();

        ushort? battery = await _client.GetBatteryAsync();
        if (battery is not null) { BatteryPercent = battery.Value / 10.0; }

        Statistics = $"frames ok {_client.FramesOk}   bad {_client.FramesBad}";
    }

    private async Task SetLampAsync(object? parameter)
    {
        if (parameter is not LampViewModel lamp) { return; }

        Status status = await _client.SetLampAsync(lamp.Id, !lamp.IsOn);
        if (status != Status.Ok)
        {
            AddEvent(null, $"SET_LAMP {lamp.Name}: {Names.Of(status)}", true);
        }
    }

    private async Task DoorAsync(bool locked)
    {
        Status status = await _client.DoorAsync(locked);

        // NOT_PERMITTED is the expected answer when a door is open - the BCM
        // refusing to lock is correct behaviour, not an error.
        string action = locked ? "Lock" : "Unlock";
        AddEvent(null, $"{action}: {Names.Of(status)}", status != Status.Ok);

        await RefreshDtcsAsync();
    }

    private async Task ClearDtcAsync()
    {
        await _client.ClearDtcsAsync();
        await RefreshDtcsAsync();
        AddEvent(null, "DTCs cleared", false);
    }

    private async Task RefreshDtcsAsync()
    {
        IReadOnlyList<Dtc> codes = await _client.GetDtcsAsync();
        Dtcs.Clear();
        foreach (Dtc c in codes) { Dtcs.Add($"0x{(byte)c:X2}  {Names.Of(c)}"); }
    }

    private void OnEventReceived(object? sender, BcmEventArgs e)
    {
        bool fault = e.Event is LogEventId.LockRefused or LogEventId.BatteryLow
                     or LogEventId.CommsLost or LogEventId.PostFailed
                     or LogEventId.WatchdogReset;

        // The reader runs on a background thread; marshal onto the UI thread.
        App.Current?.Dispatcher.Invoke(() =>
            AddEvent(e.TimestampMs, Names.Of(e.Event), fault));
    }

    private void AddEvent(uint? timestampMs, string description, bool isFault)
    {
        Events.Insert(0, new EventEntry
        {
            Timestamp = timestampMs is null
                ? DateTime.Now.ToString("HH:mm:ss")
                : $"{timestampMs.Value} ms",
            Description = description,
            IsFault = isFault,
        });

        // Bounded, so a long session cannot grow without limit.
        while (Events.Count > 500) { Events.RemoveAt(Events.Count - 1); }
    }

    public void Dispose()
    {
        _poll.Stop();
        _client.Dispose();
    }
}
