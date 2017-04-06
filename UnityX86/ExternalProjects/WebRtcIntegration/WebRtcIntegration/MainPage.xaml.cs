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

namespace WebRtcIntegration
{
    public sealed partial class MainPage : Page
    {
        private WebRtcUtils _webRtcUtils;        

        public MainPage()
        {
            this.InitializeComponent();            
            _webRtcUtils = new WebRtcUtils();
            _webRtcUtils.OnInitialized += _webRtcUtils_OnInitialized;
            _webRtcUtils.OnPeerMessageDataReceived += _webRtcUtils_OnPeerMessageDataReceived;
            _webRtcUtils.OnStatusMessageUpdate += _webRtcUtils_OnStatusMessageUpdate;            
            _webRtcUtils.PeerVideo = PeerVideo;
            _webRtcUtils.SelfVideo = SelfVideo;
            _webRtcUtils.Initialize();
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
                StatusTextBox.Text += string.Format("{0}\n", msg);
            });
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
