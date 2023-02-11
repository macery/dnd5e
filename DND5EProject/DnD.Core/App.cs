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
                //.InNamespace("Model")
                .AsInterfaces()
                .RegisterAsLazySingleton();
                
            RegisterAppStart<HomeViewModel>();
        }
    }
}
