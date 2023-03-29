using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DnD.Core.Models;

public class CharacterModel : ICharacterModel
{
    public int CharacterId { get; set; }
    public string CharacterName { get; set; }
    public int Class { get; set; }
    public int Race { get; set; }
    public int Alignment { get; set; }
    public int Level { get; set; }

}
