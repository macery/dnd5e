namespace DnD.Core.Models
{
    public interface ICharacterModel
    {
        int Alignment { get; set; }
        int CharacterId { get; set; }
        string CharacterName { get; set; }
        int Class { get; set; }
        int Race { get; set; }
        int Level { get; set; }
    }
}