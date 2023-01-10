using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DnDLibrary.System;

public class LoggerFile : ILogger
{
    private readonly ILogger<LoggerFile> _log;

    public LoggerFile(ILogger<LoggerFile> log)
    {
        _log = log;
    }
    public void LogInformation(string message)
    {
        _log.LogInformation(message);
    }
}
