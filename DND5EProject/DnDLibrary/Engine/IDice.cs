using System.Collections.Generic;

namespace DnDLibrary.Engine
{
    public interface IDice
    {
        int D100();
        int D12();
        int D20();
        //int D4();
        List<int> D4(int num);
        int D6();
        int D8();
    }
}