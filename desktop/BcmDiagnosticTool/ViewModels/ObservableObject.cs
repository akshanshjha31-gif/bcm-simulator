using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Input;

namespace BcmDiagnosticTool.ViewModels;

/// <summary>Minimal INotifyPropertyChanged base - no external MVVM dependency.</summary>
public abstract class ObservableObject : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;

    protected void OnPropertyChanged([CallerMemberName] string? name = null) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    protected bool Set<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) { return false; }
        field = value;
        OnPropertyChanged(name);
        return true;
    }
}

/// <summary>Command wrapper for button bindings.</summary>
public sealed class RelayCommand : ICommand
{
    private readonly Func<object?, Task> _execute;
    private readonly Func<object?, bool>? _canExecute;
    private bool _running;

    public RelayCommand(Func<object?, Task> execute, Func<object?, bool>? canExecute = null)
    {
        _execute = execute;
        _canExecute = canExecute;
    }

    public RelayCommand(Action<object?> execute, Func<object?, bool>? canExecute = null)
        : this(p => { execute(p); return Task.CompletedTask; }, canExecute)
    {
    }

    public event EventHandler? CanExecuteChanged;

    public void RaiseCanExecuteChanged() =>
        CanExecuteChanged?.Invoke(this, EventArgs.Empty);

    public bool CanExecute(object? parameter) =>
        !_running && (_canExecute?.Invoke(parameter) ?? true);

    public async void Execute(object? parameter)
    {
        // Re-entrancy guard: a slow request must not queue up behind repeated
        // clicks and flood the link.
        _running = true;
        RaiseCanExecuteChanged();
        try { await _execute(parameter).ConfigureAwait(true); }
        finally
        {
            _running = false;
            RaiseCanExecuteChanged();
        }
    }
}
