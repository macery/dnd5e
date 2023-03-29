using MvvmCross.Binding.Extensions;
using System;
using System.Collections.Generic;
using System.Data.SqlTypes;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DnD.Core.Engine
{
    public static class SQLCommands
    {
        public static string SelectStatement(string table,params object[] args) 
        {
            
            StringBuilder sqlCmd = new StringBuilder();
            sqlCmd.Append($"Select * from {table}");
            if (args != null)
            {
                foreach ( object arg in args ) 
                {
                    sqlCmd.Append($" {arg.ToString()} ");
                }
            }

            return sqlCmd.ToString();
        }

    }
}
