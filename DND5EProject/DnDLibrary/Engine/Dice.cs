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

    public Dice(IRng random, ILoggerFactory logger)
    {
        _random = random;
        _logger = logger;
    }
    
    private int RolldX(int x)
    {
        int output = _random.RandomHigh(x + 1);
        Log.Logger.Debug("Rolled a {output} with a {x} Sided Die", output, (x));
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
    public List<int> D6(int num)
    {
        List<int> result = new List<int>();
        for (int i = 0; i < num; i++)
        {
            result.Add(RolldX(6));
        }
        return result;
    }
    public List<int> D8(int num)
    {
        List<int> result = new List<int>();
        for (int i = 0; i < num; i++)
        {
            result.Add(RolldX(8));
        }
        return result;
    }
    public List<int> D12(int num)
    {
        List<int> result = new List<int>();
        for (int i = 0; i < num; i++)
        {
            result.Add(RolldX(12));
        }
        return result;
    }
    public List<int> D20(int num)
    {
        List<int> result = new List<int>();
        for (int i = 0; i < num; i++)
        {
            result.Add(RolldX(20));
        }
        return result;
    }
    public List<int> D100(int num)
    {
        List<int> result = new List<int>();
        for (int i = 0; i < num; i++)
        {
            result.Add(RolldX(100));
        }
        return result;
    }

    public int RollTotal(List<int> list) 
    {
        var result = list.Sum();
        return result;
    }

   }
