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

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace WebRtcIntegration
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        private WebRtcUtils _webRtcUtils;        

        public MainPage()
        {
            this.InitializeComponent();            
            _webRtcUtils = new WebRtcUtils();
            _webRtcUtils.OnInitialized += _webRtcUtils_OnInitialized;
            _webRtcUtils.PeerVideo = PeerVideo;
            _webRtcUtils.SelfVideo = SelfVideo;            
            _webRtcUtils.Initialize();
        }

        private void _webRtcUtils_OnInitialized()
        {
            // Initialize Setup
            _webRtcUtils.SelectedVideoCodec = _webRtcUtils.VideoCodecs.FirstOrDefault(x => x.Name.Contains("H264"));
            _webRtcUtils.IsMicrophoneEnabled = false;
        }

        private void ConnectButton_Click(object sender, RoutedEventArgs e)
        {
            _webRtcUtils.ConnectCommand.Execute(PeerTextBox.Text);
        }

        private void DisconnectButton_Click(object sender, RoutedEventArgs e)
        {
            _webRtcUtils.DisconnectFromServerCommand.Execute(null);        }

        private void ConnectToPeerButton_Click(object sender, RoutedEventArgs e)
        {
            _webRtcUtils.SelectedPeer = _webRtcUtils.Peers[0];
            _webRtcUtils.ConnectToPeerCommand.Execute(null);
        }

        private void SendMessageButton_Click(object sender, RoutedEventArgs e)
        {
            StatusTextBox.Text = string.Empty;
        }
    }
}
