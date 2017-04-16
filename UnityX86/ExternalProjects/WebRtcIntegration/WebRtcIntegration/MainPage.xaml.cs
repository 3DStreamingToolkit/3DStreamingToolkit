using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.ApplicationModel.Core;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using PeerConnectionClient.Signalling;
using WebRtcWrapper;
using System.Threading.Tasks;
using Windows.Graphics.Imaging;
using Windows.UI.Xaml.Media.Imaging;


namespace WebRtcIntegration
{
    public sealed partial class MainPage : Page
    {
        private WebRtcUtils _webRtcUtils;
        private WriteableBitmap wb;
        private byte[] sourcePixels;
        private int frameCounter = 0;

        public MainPage()
        {
            this.InitializeComponent();
            _webRtcUtils = new WebRtcUtils();
            _webRtcUtils.OnInitialized += _webRtcUtils_OnInitialized;
            _webRtcUtils.OnPeerMessageDataReceived += _webRtcUtils_OnPeerMessageDataReceived;
            _webRtcUtils.OnStatusMessageUpdate += _webRtcUtils_OnStatusMessageUpdate;
            _webRtcUtils.OnRawVideoFrameSend += _webRtcUtils_OnRawVideoFrameSend;
            _webRtcUtils.OnEncodedFrameReceived += _webRtcUtils_OnEncodedFrameReceived;
            _webRtcUtils.OnRawFrameReceived2 += _webRtcUtils_OnRawFrameReceived2;
            _webRtcUtils.PeerVideo = PeerVideo;
            _webRtcUtils.SelfVideo = SelfVideo;
            _webRtcUtils.Initialize();

            sourcePixels = new byte[409600 * 4];
            wb = new WriteableBitmap(640, 640);
            UpdateImage();
        }

        private void _webRtcUtils_OnRawFrameReceived2()
        {
            RunOnUi(() =>
            {
                StatusTextBox.Text = string.Format("Raw Frame: {0}", frameCounter++);
            });
        }

        private void _webRtcUtils_OnEncodedFrameReceived(uint arg1, uint arg2, byte[] arg3)
        {         
        }

        public async void RunOnUi(DispatchedHandler handler)
        {
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                handler
            );
        }
        private void _webRtcUtils_OnStatusMessageUpdate(string msg)
        {
            RunOnUi(() =>
            {
                StatusTextBox.Text = string.Format("{0}\n", msg);
            });
        }

        public static byte XClamp(double val, byte min, byte max)
        {
            if (val.CompareTo(min) < 0) return min;
            else if (val.CompareTo(max) > 0) return max;
            else return (byte) val;
        }

        private void _webRtcUtils_OnRawVideoFrameSend(uint width, uint height, byte[] yPlane, uint yPitch, byte[] vPlane, uint vPitch, byte[] uPlane, uint uPitch)
        {
            
            RunOnUi(() =>
            {
                StatusTextBox.Text = string.Format(
                    "{0}-{1}\n{2}\n{3}\n{4}\n{5}\n{6}\n{7}",
                    width, // Width
                    height, // Height
                    yPlane != null ? yPlane.Length.ToString() : "null",     // yPlane
                    yPitch,                                             // yPitch
                    uPlane != null ? uPlane.Length.ToString() : "null",     // vPlane
                    uPitch,                                             // vPitch
                    vPlane != null ? vPlane.Length.ToString() : "null",     // uPlane
                    vPitch                                             // uPitch
                );

            /*
            //int c = 0;
            //for (int i = 0; i < 409600 * 4; i += 4)
            //{
            //    sourcePixels[i] = yPlane[c];
            //    sourcePixels[i + 1] = 100;
            //    sourcePixels[i + 2] = 100;
            //    sourcePixels[i + 3] = 255;
            //    c++;
            //}

            int c = 0;
            int index = 0;
            for (int j = 0; j < height; j++)
            {
                for (int i = 0; i < width; i++)
                {
                    //yuv2rgb(y[i], u[i / 2], v[i / 2], dst[0], dst[1], dst[2]);
                    float y = yPlane[index];
                    float v = vPlane[(j / 2) + (i / 2)];
                    float u = vPlane[(j / 2) + (i / 2)];

                    //sourcePixels[c + 0] = XClamp(y + 1.403f * v, 0, 255); ;
                    //sourcePixels[c + 1] = XClamp(y - 0.344f * u - 1.403 * v, 0, 255);
                    //sourcePixels[c + 2] = XClamp(y + 1.770f * u, 0, 255);
                    //sourcePixels[c + 3] = 255;

                    sourcePixels[c + 0] = 240;
                    sourcePixels[c + 1] = 50;
                    sourcePixels[c + 2] = 50;
                    sourcePixels[c + 3] = 255;                        

                    c +=4;
                    index++;
                }

                //if (j & 0x01)
                //{
                //    u += srcStride / 2;
                //    v += srcStride / 2;
                //}

                //y += srcStride;
                //dst += (dstStride - width * 3);
            }

            using (Stream stream = wb.PixelBuffer.AsStream())
            {
                stream.WriteAsync(sourcePixels, 0, sourcePixels.Length);
            }
            VideoImage.Source = wb;
            */
        });
        }

        private void UpdateImage()
        {


            RunOnUi(() =>
            {
                //byte[] sourcePixels = new byte[409600 * 4];
                for (int i = 0; i < 409600 * 4; i += 4)
                {
                    sourcePixels[i] = 240;
                    sourcePixels[i + 1] = 50;
                    sourcePixels[i + 2] = 50;
                    sourcePixels[i + 3] = 255;
                }

                using (Stream stream = wb.PixelBuffer.AsStream())
                {
                    stream.WriteAsync(sourcePixels, 0, sourcePixels.Length);
                }
                VideoImage.Source = wb;
            });

            //new Task(async () => { 
            //using (Stream stream = wb.PixelBuffer.AsStream())
            //{
            //    await stream.WriteAsync(sourcePixels, 0, sourcePixels.Length);
            //}
            //}).Start();
        }

        private void _webRtcUtils_OnPeerMessageDataReceived(int peerId, string message)
        {
            RunOnUi(() =>
            {
                PeerMessageTextBox.Text += string.Format("{0}-{1}\n", peerId, message);
            });
        }

        private void _webRtcUtils_OnInitialized()
        {
            // Initialize Setup
            _webRtcUtils.SelectedVideoCodec = _webRtcUtils.VideoCodecs.FirstOrDefault(x => x.Name.Contains("H264"));
            _webRtcUtils.IsMicrophoneEnabled = false;
        }

        private void ConnectButton_Click(object sender, RoutedEventArgs e)
        {
            _webRtcUtils.ConnectCommandExecute(PeerTextBox.Text);
        }

        private void DisconnectButton_Click(object sender, RoutedEventArgs e)
        {
            _webRtcUtils.DisconnectFromServerExecute(null);
        }

        private void ConnectToPeerButton_Click(object sender, RoutedEventArgs e)
        {
            _webRtcUtils.SelectedPeer = _webRtcUtils.Peers[0];
            _webRtcUtils.ConnectToPeerCommandExecute(null);
        }

        private void SendMessageButton_Click(object sender, RoutedEventArgs e)
        {
            _webRtcUtils.SendPeerMessageDataExecute(SendMessageTextBox.Text);
        }

        private void ClearStatusTextButton_Click(object sender, RoutedEventArgs e)
        {
            StatusTextBox.Text = string.Empty;
        }

        private void ClearPeerMessageButton_Click(object sender, RoutedEventArgs e)
        {
            PeerMessageTextBox.Text = string.Empty;
        }
    }
}
