using DnD.Core.Models;
using DnD.Core.Services;
using MvvmCross.Commands;
using MvvmCross.ViewModels;
using Serilog;

namespace DnD.Core.ViewModels
{
    public partial class HomeViewModel : MvxViewModel
    {
            public IMvxCommand ResetTextCommand => new MvxCommand(ResetText);
            private void ResetText()
            {
                Text = "Hello MvvmCross";
            }

            private string _text = "Hello MvvmCross";
        private IDiceService _diceService;
        public HomeViewModel(IDiceService diceService)
        {
            _diceService = diceService;
             _char = new CharacterModel();
            InterfaceTest();
            AddCharacter("Macery");
        }

        public string Text
            {
                get { return _text; }
                set { SetProperty(ref _text, value); }
            }
        public void InterfaceTest()
        {
            //IDiceService diceService;
            List <int> results = _diceService.D100(1);
            Log.Debug(results[0].ToString());
        }
    }
    
}
