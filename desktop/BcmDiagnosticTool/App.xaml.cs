using System.IO;
using System.Windows;
using System.Windows.Threading;

namespace BcmDiagnosticTool;

public partial class App : Application
{
    /// <summary>Typed accessor, so view models need no cast.</summary>
    public static new App? Current => Application.Current as App;

    /// <summary>
    /// Where an unhandled exception is recorded. A diagnostic tool that dies
    /// silently is worse than useless, so failures are written somewhere the
    /// user can find them rather than vanishing into an exit code.
    /// </summary>
    public static string CrashLogPath =>
        Path.Combine(Path.GetTempPath(), "BcmDiagnosticTool.crash.log");

    protected override void OnStartup(StartupEventArgs e)
    {
        AppDomain.CurrentDomain.UnhandledException += (_, args) =>
            Record(args.ExceptionObject as Exception, "AppDomain");

        DispatcherUnhandledException += OnDispatcherUnhandledException;

        base.OnStartup(e);
    }

    private void OnDispatcherUnhandledException(object sender,
                                                DispatcherUnhandledExceptionEventArgs e)
    {
        Record(e.Exception, "Dispatcher");

        MessageBox.Show(
            $"{e.Exception.Message}\n\nDetails written to:\n{CrashLogPath}",
            "BCM Diagnostic Tool - unexpected error",
            MessageBoxButton.OK, MessageBoxImage.Error);

        // Keep running: a failed request must not take the whole tool down.
        e.Handled = true;
    }

    private static void Record(Exception? ex, string source)
    {
        if (ex is null) { return; }

        try
        {
            File.AppendAllText(CrashLogPath,
                $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] {source}\n{ex}\n\n");
        }
        catch
        {
            // Nothing useful to do if even logging fails.
        }
    }
}
