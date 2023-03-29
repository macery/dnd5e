using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using MvvmCross.Commands;
using MvvmCross.ViewModels;
using SQLitePCL;

namespace DnD.Core.ViewModels
{
    public partial class HomeViewModel : MvxViewModel
    {

        public IMvxCommand CharactersByPlayerID { get; set; }
    public void GetCharactersByPlayerID(int ID)
    {

    }
        
    }


}
