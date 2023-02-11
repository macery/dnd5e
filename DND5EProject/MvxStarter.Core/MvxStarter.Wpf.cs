using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MvxStarter.Core
{
    class MvxStarter
    {
    public partial class App : MvxApplication
    {
        protected override void RegisterSetup()
        {
            this.RegisterSetupType<Setup>();
          
        }
   
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
