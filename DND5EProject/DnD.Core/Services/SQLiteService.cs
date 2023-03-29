using Dapper;
using DnD.Core.Models;
using Microsoft.Data.Sqlite;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Reflection.Metadata.Ecma335;
using System.Runtime.InteropServices;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace DnD.Core.Services;

public class SQLiteService : ISQLiteService
{
    private string _connectionString;

    public string ConnectionString
    {
        get { return _connectionString; }
        set { _connectionString = value; }
    }
    public SQLiteService()
    {
        _connectionString = LoadConnectionString();
    }
    private static string LoadConnectionString(string id = "Default")
    {
        var output = ConfigurationManager.ConnectionStrings[id].ConnectionString;
        return output;
    }


    public ObservableCollection<ICharacterModel> GetCharacters()
    {

        //var cmd = "Select * from Characters";
        //var results = SQLSelect<ICharacterModel>(cmd, "Characters");
        //return results;

        using (IDbConnection cnn = new SqliteConnection(LoadConnectionString()))
        {
            var cmd = "Select * from Characters";
            //msg($"Executing SQL Command | '{cmd}'");
            var sqlValues = cnn.Query<CharacterModel>(cmd, new DynamicParameters());
            var output = new ObservableCollection<ICharacterModel>(sqlValues);
            return output;
        }
    }
    public ObservableCollection<ICharacterModel> GetCharactersByPlayer(int PlayerID)
    {


        using (IDbConnection cnn = new SqliteConnection(LoadConnectionString()))
        {
            var cmd = $"Select * from CharacterByPlayerID where PlayerID = {PlayerID}";
            //msg($"Executing SQL Command | '{cmd}'");
            var sqlValues = cnn.Query<CharacterModel>(cmd, new DynamicParameters());
            var output = new ObservableCollection<ICharacterModel>(sqlValues);
            return output;
        }
    }
    public ObservableCollection<IPlayerModel> GetPlayers([Optional] string[] args )
    {
        using (IDbConnection cnn = new SqliteConnection(LoadConnectionString()))
        {
            //var cmd = "Select * from Players";
            var cmd = Engine.SQLCommands.SelectStatement("Players", args).ToString();
            var sqlValues = cnn.Query<PlayerModel>(cmd, new DynamicParameters());
            var output = new ObservableCollection<IPlayerModel>(sqlValues);
            return output;
        }
        //var cmd = "Select * from Players";
        //var results = SQLSelect<IPlayerModel>(cmd,"Player" );
        //return results;
    }

   //private ObservableCollection<T> SQLSelect<T>(string sql, string model ) 
   // {
   //     using (IDbConnection cnn = new SqliteConnection(LoadConnectionString()))
   //     {
   //         var cmd = sql;
   //         switch (model)
   //         {
   //            case "Characters":
   //                 {
   //                     var sqlValues = cnn.Query<CharacterModel>(cmd, new DynamicParameters());
   //                     var output = new ObservableCollection<T>(sqlValues);
   //                     return output;
   //                 }
   //             case "Player":
   //                 {
   //                     var sqlValues = cnn.Query<IPlayerModel>(cmd, new DynamicParameters());
   //                     var output = new ObservableCollection<T>(sqlValues);
   //                     return output;
   //                 }
   //             default:
   //                 break;
   //         }
   //         //var sqlValues = cnn.Query<T>(cmd, new DynamicParameters());
            
            
   //     }
    //}
}
