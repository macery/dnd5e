using DnD.Core.Models;
using MvvmCross.ViewModels;
using Serilog;
using SQLitePCL;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DnD.Core.ViewModels
{
    public partial class HomeViewModel : MvxViewModel
    {
		private int _playerID;

		public int PlayerID
		{
			get { return _playerID; }
			set { SetProperty(ref _playerID , value); }
		}
		private string _playerName;

		public string PlayerName
		{
			get { return _playerName; }
			set { SetProperty(ref _playerName , value); }
		}
		private ObservableCollection<IPlayerModel> _player;

		public ObservableCollection<IPlayerModel> Player
		{
			get { return _player; }
			set { SetProperty(ref _player, value);
				Log.Debug("test");
			}
		}
		private int _selectedPlayer;

		public int SelectedPlayer
		{
			get { return _selectedPlayer; }
			set { SetProperty(ref _selectedPlayer , value);
				Log.Debug(value.ToString());
				LoadPlayersCharacters(value);
			}
		}
        public void LoadPlayersCharacters(int PlayerID)
        {
			_characterModels= _sqlite.GetCharactersByPlayer(PlayerID);
			RaisePropertyChanged(() => CharacterModels);

        }
        public void GetPlayers()
		{

		}
	}
}
