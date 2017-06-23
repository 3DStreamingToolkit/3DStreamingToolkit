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

using Org.WebRtc;
using WebRtcWrapper;
using PeerConnectionClient.Model;
using PeerConnectionClient.Signalling;
using PeerConnectionClient.Utilities;

namespace XamlTestApp
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        private const string signallingHost = "127.0.0.1:8888";
        WebRtcControl _webRtcControl;

        public RawVideoSource rawVideo;
        public DecodedVideoSource encodedVideo;
        private MediaVideoTrack _peerVideoTrack;
        private readonly CoreDispatcher _uiDispatcher;
        private int frameCounter = 0;

        public MainPage()
        {
            this.InitializeComponent();
            _uiDispatcher = CoreApplication.MainView.CoreWindow.Dispatcher;
            StatusTextBlock.Text = string.Empty;
            MessagesTextBlock.Text = string.Empty;
            VideoTextBlock.Text = string.Empty;
            frameCounter = 0;

            _webRtcControl = new WebRtcControl();
            _webRtcControl.OnInitialized += _webRtcControl_OnInitialized;
            _webRtcControl.OnPeerMessageDataReceived += _webRtcControl_OnPeerMessageDataReceived;
            _webRtcControl.OnStatusMessageUpdate += _webRtcControl_OnStatusMessageUpdate;
            Conductor.Instance.OnAddRemoteStream += Instance_OnAddRemoteStream;
            Conductor.Instance.OnPeerDataChannelReceived += Instance_OnPeerDataChannelReceived;
            _webRtcControl.Initialize();            
        }

        private void Instance_OnPeerDataChannelReceived(int arg1, string arg2)
        {
            RunOnUiThread(() =>
            {                
                MessagesTextBlock.Text += string.Format("Conductor-PeerDataChannel:{0}-{1}\n", arg1, arg2);
            });
        }
        
        private void _webRtcControl_OnInitialized()
        {
            RunOnUiThread(() =>
            {
                _webRtcControl.SelectedVideoCodec =
                    _webRtcControl.VideoCodecs.FirstOrDefault(x => x.Name.Contains("H264"));
                StatusTextBlock.Text += "WebRTC Initialized\n";
            });
        }

        private void _webRtcControl_OnPeerMessageDataReceived(int arg1, string arg2)
        {
            RunOnUiThread(() =>
            {
                MessagesTextBlock.Text += string.Format("Signal:{0}-{1}\n", arg1, arg2);
            });
        }

        private void _webRtcControl_OnStatusMessageUpdate(string obj)
        {
            RunOnUiThread(() =>
            {
                StatusTextBlock.Text += string.Format("{0}\n", obj);
            });
        }

        private void Instance_OnAddRemoteStream(MediaStreamEvent evt)
        {
            RunOnUiThread(() =>
                {
                    System.Diagnostics.Debug.WriteLine("Conductor_OnAddRemoteStream()");

                    _peerVideoTrack = evt.Stream.GetVideoTracks().FirstOrDefault();
                    if (_peerVideoTrack != null)
                    {
                        System.Diagnostics.Debug.WriteLine(
                            "Conductor_OnAddRemoteStream() - GetVideoTracks: {0}",
                            evt.Stream.GetVideoTracks().Count);
                         // Raw Video from VP8 Encoded Sender
                         // H264 Encoded Stream does not trigger this event

                         // TODO:  Switch between RAW or ENCODED Frame
#if HACK_VP8
            rawVideo = Media.CreateMedia().CreateRawVideoSource(_peerVideoTrack);
            rawVideo.OnRawVideoFrame += Source_OnRawVideoFrame;
#else
                         encodedVideo = Media.CreateMedia().CreateDecodedVideoSource(_peerVideoTrack);
                        encodedVideo.OnDecodedVideoFrame += EncodedVideo_OnEncodedVideoFrame;


#endif
                    }
                    else
                    {
                        System.Diagnostics.Debug.WriteLine("Conductor_OnAddRemoteStream() - peerVideoTrack NULL");
                    }
                    _webRtcControl.IsReadyToDisconnect = true;
                }
            );
        }

        private void EncodedVideo_OnEncodedVideoFrame(uint w, uint h, byte[] data)
        {
            RunOnUiThread(() =>
            {
                frameCounter++;
                VideoTextBlock.Text = string.Format("Raw Frame: {0}\nData: {1}\n", frameCounter, data.Length);
            });
        }

        private void ConnectServerBtn_Click(object sender, RoutedEventArgs e)
        {
            RunOnUiThread(() =>
                {
                    var signalhost = signallingHost.Split(':');
                    var host = string.Empty;
                    var port = string.Empty;
                    if (signalhost.Length > 1)
                    {
                        host = signalhost[0];
                        port = signalhost[1];
                    }
                    else
                    {
                        host = signalhost[0];
                        port = "8888";
                    }
                    _webRtcControl.ConnectToServer(host, port, "WebRTCWrapper");
                }
            );
        }

        private void ConnectPeerBtn_Click(object sender, RoutedEventArgs e)
        {
            RunOnUiThread(() =>
                {
                    if (_webRtcControl.Peers.Count > 0)
                    {
                        _webRtcControl.SelectedPeer = _webRtcControl.Peers[0];
                        _webRtcControl.ConnectToPeer();
                    }
                }
            );
        }

        private void DisconnectPeerBtn_Click(object sender, RoutedEventArgs e)
        {
            RunOnUiThread(() =>
                {
                    if (encodedVideo != null)
                    {
                        encodedVideo.OnDecodedVideoFrame -= EncodedVideo_OnEncodedVideoFrame;
                    }
                    _webRtcControl.DisconnectFromPeer();
                }
            );
        }

        private void DisconnectServerBtn_Click(object sender, RoutedEventArgs e)
        {
            RunOnUiThread(() =>
                {
                    _webRtcControl.DisconnectFromServer();
                }
            );
        }

        private void ClearStatusBtn_Click(object sender, RoutedEventArgs e)
        {
            RunOnUiThread(() =>
                {
                    StatusTextBlock.Text = string.Empty;
                }
            );
        }

        private void ClearMessagesBtn_Click(object sender, RoutedEventArgs e)
        {
            RunOnUiThread(() =>
                {
                    MessagesTextBlock.Text = string.Empty;
                }
            );
        }

        private void ClearVideoBtn_Click(object sender, RoutedEventArgs e)
        {
            RunOnUiThread(() =>
                {
                    VideoTextBlock.Text = string.Empty;
                }
            );
        }

        private void SendMessagesBtn_Click(object sender, RoutedEventArgs e)
        {
            RunOnUiThread(() =>
            {
                //_webRtcControl.SendPeerMessageData(MessageInputText.Text);
                _webRtcControl.SendPeerDataChannelMessage(MessageInputText.Text);
            });
        }

        protected void RunOnUiThread(Action fn)
        {
            var asyncOp = _uiDispatcher.RunAsync(CoreDispatcherPriority.Normal, new DispatchedHandler(fn));
        }
    }
}
