using DnD.Core.Models;
using MvvmCross.ViewModels;
using Serilog;
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
		private int _characterID;

		public int CharacterID
		{
			get { return _characterID; }
			set { SetProperty(ref _characterID, value); }
		}
		private string _characterName;

		public string CharacterName
		{
			get { return _characterName; }
			set { SetProperty(ref _characterName , value); }
		}
		private ObservableCollection<ICharacterModel> _characterModels = new ObservableCollection<ICharacterModel>();
			public ObservableCollection<ICharacterModel> CharacterModels
		{		get { return _characterModels; }
				set { SetProperty(ref _characterModels, value); }
		}
		private ICharacterModel _char;

		public ICharacterModel Char
        {
			get { return _char; }
			set { SetProperty(ref _char , value); }
		}

		public void AddCharacter(string characterName)
		{
			CharacterModel c = new CharacterModel()
			{
				CharacterName = characterName
			};
			CharacterModels.Add(c);
			Log.Debug(c.CharacterName.ToString());

		}
		
		

	}
}
