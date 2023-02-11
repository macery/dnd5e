namespace DnD.Core.Services
{
    public interface IDiceService
    {
        List<int> D100(int num);
        List<int> D12(int num);
        List<int> D20(int num);
        List<int> D4(int num);
        List<int> D6(int num);
        List<int> D8(int num);
        int RollTotal(List<int> list);
    }
}