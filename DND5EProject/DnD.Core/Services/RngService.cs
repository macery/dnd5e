using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DnD.Core.Services
{
    public class RngService : IRngService
    {
        private readonly Random _random;

        public RngService()
        {
            _random = new Random();
        }


        public int RandomHigh(int TopNumber)
        {
            var output = _random!.Next(1, TopNumber);   //_random.next(1, TopNumber);

            return output;
        }
    }
}
