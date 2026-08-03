using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;

namespace BcmDiagnosticTool;

/// <summary>true -> lamp colour, false -> unlit colour.</summary>
public sealed class BoolToLampBrushConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        bool on = value is bool b && b;
        return on ? new SolidColorBrush(Color.FromRgb(0xFF, 0xC2, 0x4B))
                  : new SolidColorBrush(Color.FromRgb(0x2A, 0x30, 0x3A));
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}

/// <summary>true -> green, false -> red. Used for link health.</summary>
public sealed class BoolToHealthBrushConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        bool ok = value is bool b && b;
        return ok ? new SolidColorBrush(Color.FromRgb(0x3F, 0xBF, 0x7F))
                  : new SolidColorBrush(Color.FromRgb(0xE5, 0x48, 0x4D));
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}

/// <summary>Faults are highlighted; ordinary events are not.</summary>
public sealed class FaultToBrushConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        bool fault = value is bool b && b;
        return fault ? new SolidColorBrush(Color.FromRgb(0xE5, 0x48, 0x4D))
                     : new SolidColorBrush(Color.FromRgb(0xE4, 0xE7, 0xEC));
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}
