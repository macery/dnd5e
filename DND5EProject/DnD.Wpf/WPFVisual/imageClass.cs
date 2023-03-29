using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Interop;
using System.Windows;
using System.Windows.Media.Imaging;

namespace DnD.Core.Engine
{
    public class imageClass
    {
        // Create a Bitmap object from a file.
        //Bitmap myBitmap = new Bitmap("Grapes.jpg");

        // Clone a portion of the Bitmap object.
        //RectangleF cloneRect = new RectangleF(0, 0, 100, 100);
        //System.Drawing.Imaging.PixelFormat format =
        //    myBitmap.PixelFormat;
        //Bitmap cloneBitmap = myBitmap.Clone(cloneRect, format);

        // Draw the cloned portion of the Bitmap object.
        //e.Graphics.DrawImage(cloneBitmap, 0, 0);
        public imageClass(string source, int xStart, int yStart)
        {
            _bitmapIn= new Bitmap(source);
            _rectClone= new RectangleF(xStart,yStart,32,32) ;
            _myBitmap = new BitmapImage();
            


        }
        private Bitmap _bitmapIn ;

       public Bitmap BitmapIn 
        {
            get {  return _bitmapIn; }
            set { _bitmapIn = value; }
        }
        private RectangleF _rectClone;

        public RectangleF RectClone
        {
            get { return _rectClone; }
            set { _rectClone = value; }
        }
        
        public BitmapImage currentBM()
        {
            System.Drawing.Imaging.PixelFormat format =_bitmapIn.PixelFormat;
            Bitmap cloneBitMap = _bitmapIn.Clone(_rectClone, format );
            var output = Bitmap2BitmapImage(cloneBitMap);
            return output;
        }
       // public BitmapImage GetBitmapImage (string sourceImg, int xStart, int yStart)
       // {
       //     _rectClone=new RectangleF(xStart,yStart,32,32);

       //     _myBitmap.BeginInit();
       //     _myBitmap.UriSource = (new Uri(sourceImg, UriKind.RelativeOrAbsolute));
       //     _myBitmap.EndInit();
       //     System.Drawing.Imaging.PixelFormat format =
       //_myBitmap.PixelFormat;
       //     var output = _myBitmap.Clone(_rectClone, format);
       //     return output;

       // }
        private BitmapImage _myBitmap;

        public BitmapImage MyBitmap
        {
            get { return _myBitmap; }
            set { _myBitmap = value; }
        }
        private Bitmap BitmapImage2Bitmap(BitmapImage bitmapImage)
        {
            // BitmapImage bitmapImage = new BitmapImage(new Uri("../Images/test.png", UriKind.Relative));

            using (MemoryStream outStream = new MemoryStream())
            {
                BitmapEncoder enc = new BmpBitmapEncoder();
                enc.Frames.Add(BitmapFrame.Create(bitmapImage));
                enc.Save(outStream);
                System.Drawing.Bitmap bitmap = new System.Drawing.Bitmap(outStream);

                return new Bitmap(bitmap);
            }

        }
        [System.Runtime.InteropServices.DllImport("gdi32.dll")]
        public static extern bool DeleteObject(IntPtr hObject);

        private static BitmapImage Bitmap2BitmapImage(Bitmap bitmap)
        {
            IntPtr hBitmap = bitmap.GetHbitmap();
            BitmapImage retval;

            try
            {
                retval = (BitmapImage)Imaging.CreateBitmapSourceFromHBitmap(
                             hBitmap,
                             IntPtr.Zero,
                             Int32Rect.Empty,
                             BitmapSizeOptions.FromEmptyOptions());
            }
            finally
            {
                DeleteObject(hBitmap);
            }

            return retval;
        }


    }
}
