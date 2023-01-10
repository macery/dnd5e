using Microsoft.Extensions.Logging;
using Serilog;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DnDLibrary.Engine;

public class Dice : IDice
{
    private readonly IRng _random;
    private readonly ILoggerFactory _logger;

    public Dice(IRng random, ILoggerFactory logger )
    {
        _random = random;
        _logger = logger;
    }
    private int RolldX(int x)
    {
        int output = _random.RandomHigh(x + 1);
        Log.Logger.Debug("Rolled a {output} with a {x} Sided Die",output, (x)); 
        return output;
        
    }
    public List<int> D4(int num) 
    {
    List<int> result = new List<int>();
        //int roll = 0;
        for (int i = 0; i < num; i++)
        {
            result.Add(RolldX(4));
        }
        return result;
    }
    public int D6() { return RolldX(6); }
    public int D8() { return RolldX(8); }
    public int D12() { return RolldX(12); }
    public int D20() { return RolldX(20); }
    public int D100() { return RolldX(100); }

    
}
