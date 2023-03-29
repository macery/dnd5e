using DnD.Core.Engine;
using MvvmCross.Platforms.Wpf.Views;
using System;
using System.Collections.Generic;
using System.Drawing;
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
using System.Windows.Shapes;
using System.IO;
using Serilog;
using MvvmCross.Platforms.Wpf.Binding;
using DnD.Core.ViewModels;

namespace DnD.Wpf.Views
{
    /// <summary>
    /// Interaction logic for HomeView.xaml
    /// </summary>
    public partial class HomeView : MvxWpfView
    {
        public HomeView()
        {
            
            InitializeComponent();
            
           
        }

        private void comboPlayer_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
           this.playerSelected.Text = comboPlayer.SelectedValue.ToString();
        }
    }

}
