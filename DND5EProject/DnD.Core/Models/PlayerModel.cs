using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DnD.Core.Models
{
    public class PlayerModel : IPlayerModel
    {
        public int PlayerID { get; set; }
        public string PlayerName { get; set; }

    }
}
