using Microsoft.Extensions.Logging;
using MvvmCross.Core;

using MvvmCross.Platforms.Wpf.Core;
using MvvmCross.Platforms.Wpf.Views;
using Serilog;
using Serilog.Extensions.Logging;

namespace MvxStarter.Wpf
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : MvxApplication
    {
        protected override void RegisterSetup()
        {
            this.RegisterSetupType<Setup>();
        }
    }
    public class Setup : MvxWpfSetup<App>
    {
        protected override ILoggerFactory? CreateLogFactory()
        {
            return new SerilogLoggerFactory();
        }

        protected override ILoggerProvider? CreateLogProvider()
        {
            Log.Logger = new LoggerConfiguration()
                .MinimumLevel.Debug()
                .CreateLogger();

            return new SerilogLoggerProvider();
        }
    }
}
