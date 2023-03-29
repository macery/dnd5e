using Serilog;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation;
using Size = System.Drawing.Size;

namespace DnD.Core.Services
{
    public class TileSheetService
    {
        private Image _image;
        private Size _size;
        public TileSheetService(string inputFile, int xSize, int ySize)
        {
            if (!File.Exists(inputFile)) throw new FileNotFoundException();

            _image = Image.FromFile(inputFile);
            _size = new Size(xSize, ySize);
        }
        

        public void GenerateTiles(string outputPath)
        {
            int xMax = _image.Width;
            int yMax = _image.Height;
            int tileWidth = xMax / _size.Width;
            int tileHeight = yMax / _size.Height;

            if (!Directory.Exists(outputPath)) { Directory.CreateDirectory(outputPath); }


            
            
            
            for (int x = 0; x < _size.Width; x++)
            {
                Log.Debug(string.Format("X equals: {0}", x));
                
                for (int y = 0; y < _size.Height; y++)
                {
                    Log.Debug(string.Format("Y equals: {0}", y));

                    string outputFileName = Path.Combine(outputPath, string.Format("{0}_{1}.jpg", x, y));

                    Rectangle tileBounds = new Rectangle(x * tileWidth, y * tileHeight, tileWidth, tileHeight);
                    Bitmap target = new Bitmap(tileWidth, tileHeight);

                    using (Graphics _graphics = Graphics.FromImage(target))
                    {
                        _graphics.DrawImage(
                        _image,
                        new Rectangle(0, 0, tileWidth, tileHeight),
                        tileBounds,
                        GraphicsUnit.Pixel);
                    }
                    target.Save(outputFileName, ImageFormat.Jpeg);
                }
            }
        }
    }
}

