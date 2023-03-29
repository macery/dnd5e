using MvvmCross.IoC;
using MvvmCross.ViewModels;
using DnD.Core.ViewModels;

namespace DnD.Core
{
    public  class App : MvxApplication
    {
        public override void Initialize()
        {
            CreatableTypes()
                .EndingWith("Service")
                .AsInterfaces()
                .RegisterAsLazySingleton();

            CreatableTypes()
                .InNamespace("Models")
                .AsInterfaces()
                .RegisterAsLazySingleton();
            //CreatableTypes()
            //    .EndingWith("")
            //    .
            CreatableTypes()
                .EndingWith("Engine")
                .AsInterfaces()
                .RegisterAsLazySingleton();
            RegisterAppStart<HomeViewModel>();
        }
    }
}
