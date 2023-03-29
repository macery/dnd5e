using DnD.Core.Models;
using DnD.Core.Services;
using MvvmCross.Commands;
using MvvmCross.ViewModels;
using Serilog;
using SQLitePCL;
using System;
//using System.Drawing ;
//using Windows.UI.Xaml.Controls;
//using Windows.UI.Xaml.Media;
using System.Drawing;

//using DnD.Core.Engine;
using Windows.UI.Xaml.Media.Imaging;
//using Windows.UI.Xaml.Media.Imaging;
//using Windows.UI.Xaml.Controls;

//Windows.UI.Xaml.Media.Imaging;
//using Image = System.Drawing.Bitmap;
//using Windows.UI.Xaml.Media.Imaging;



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
        //private SQLiteService _sqlite;
        public BitmapImage IconSource;
        public ISQLiteService _sqlite ;
        public HomeViewModel(IDiceService diceService, ISQLiteService ISqlite)
        {
            
            _diceService = diceService;
           
            _imageSRC = "C:\\Repos\\dnd5e\\DND5EProject\\DnD.Core\\Assests\\Icons\\1\\12_5.jpg";
            //_characterModels = _sqlite.GetCharacters();
            _sqlite= ISqlite;

            _player = _sqlite.GetPlayers();

        }
        public Image sendImg { get; set; }

        private BitmapImage _firstImage;
        //private BitmapImage _imageSrc;
        
        public BitmapImage FirstImage
        {
            get { return _firstImage; }
            set { SetProperty(ref _firstImage, value); }
        }

        public string Text
        {
            get { return _text; }
            set { SetProperty(ref _text, value); }
        }

        private string _imageSRC;

        public string ImageSrc
        {
            get { return _imageSRC; }
            set { SetProperty(ref _imageSRC , value); }
        }


       public void testTiles()
        {
            var tiler = new TileSheetService("C:\\Repos\\dnd5e\\DND5EProject\\DnD.Core\\Assests\\Icons\\#1 - Transparent Icons.png", 16, 27);
            tiler.GenerateTiles("C:\\Repos\\dnd5e\\DND5EProject\\DnD.Core\\Assests\\Icons\\1\\");
        }


    }

}
