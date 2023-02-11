using MvvmCross.Core;
using MvvmCross.Platforms.Wpf.Core;
//using MvvmCross.ViewModels;
using MvvmCross.Platforms.Wpf.Views;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;

namespace DnD.Wpf
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : MvxApplication
    {
        protected override void RegisterSetup()
        {
            //Log.Information("WPF-App Initialized");
            this.RegisterSetupType<Setup>();
        }
    }
}
