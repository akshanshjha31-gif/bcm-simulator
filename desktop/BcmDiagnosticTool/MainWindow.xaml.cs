using System.Windows;
using BcmDiagnosticTool.ViewModels;

namespace BcmDiagnosticTool;

public partial class MainWindow : Window
{
    private readonly MainViewModel _viewModel = new();

    public MainWindow()
    {
        InitializeComponent();
        DataContext = _viewModel;
        Closed += (_, _) => _viewModel.Dispose();

        // `--connect COMx` opens the link on startup. Handy day to day, and it
        // makes the tool verifiable end-to-end without a human clicking.
        string[] args = Environment.GetCommandLineArgs();
        for (int i = 0; i < args.Length - 1; i++)
        {
            if (!string.Equals(args[i], "--connect", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            string port = args[i + 1];
            Loaded += (_, _) =>
            {
                _viewModel.SelectedPort = port;
                if (_viewModel.ConnectCommand.CanExecute(null))
                {
                    _viewModel.ConnectCommand.Execute(null);
                }
            };
            break;
        }
    }
}

/// <summary>
/// Inverts a boolean binding. Declared as a static instance so XAML can use
/// it via x:Static without an extra resource entry.
/// </summary>
public sealed class Inverse : System.Windows.Data.IValueConverter
{
    public static readonly Inverse Bool = new();

    public object Convert(object value, Type targetType, object parameter,
                          System.Globalization.CultureInfo culture) =>
        value is bool b && !b;

    public object ConvertBack(object value, Type targetType, object parameter,
                              System.Globalization.CultureInfo culture) =>
        value is bool b && !b;
}
