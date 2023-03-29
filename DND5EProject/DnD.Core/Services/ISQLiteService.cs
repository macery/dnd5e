using DnD.Core.Models;
using System.Collections.ObjectModel;
using System.Runtime.InteropServices;

namespace DnD.Core.Services
{
    public interface ISQLiteService
    {
        string ConnectionString { get; set; }

        ObservableCollection<ICharacterModel> GetCharacters();
        ObservableCollection<ICharacterModel> GetCharactersByPlayer(int PlayerID);
        ObservableCollection<IPlayerModel> GetPlayers([Optional] string[] args);
    }
}