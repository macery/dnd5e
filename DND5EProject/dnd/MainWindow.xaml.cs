using DnDLibrary.Engine;
using Serilog;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace dnd
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private readonly IRng _random;
        private readonly IDice _dice;

        //private readonly Rng _newrandom;

        public MainWindow(IRng random, IDice dice)
        {
            InitializeComponent();
            _random = random;
            _dice = dice;
            roll_xDy(4, 1);
            //_newrandom = newrandom;
        }
        private void roll_xDy(int x, int y)
        {
            var rolls = _dice.D20(x);
            var total = _dice.RollTotal(rolls);
        }
        
        
    }
}
