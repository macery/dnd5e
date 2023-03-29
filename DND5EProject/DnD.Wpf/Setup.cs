using Microsoft.Extensions.Logging;
using MvvmCross.Platforms.Wpf.Core;
using Serilog;
using Serilog.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DnD.Wpf
{
    class Setup : MvxWpfSetup<DnD.Core.App>
    {
        protected override ILoggerProvider CreateLogProvider()
        {
            return new SerilogLoggerProvider();
        }
        protected override ILoggerFactory CreateLogFactory()
        {
            // serilog configuration
            Log.Logger = new LoggerConfiguration()
                .MinimumLevel.Debug()
                .Enrich.FromLogContext()
                // add more sinks here
                .WriteTo.Debug()
                .WriteTo.File("..\\Wpf.Log")
                .CreateLogger();
            Log.Debug("Logger Built");
            return new SerilogLoggerFactory();
        }
    }
}
