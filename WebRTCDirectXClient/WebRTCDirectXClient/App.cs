using Org.WebRtc;
using PeerConnectionClient.Model;
using PeerConnectionClient.Signalling;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using WebRTCDirectXClientComponent;
using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.ApplicationModel.Core;
using Windows.UI.Core;
using Windows.UI.ViewManagement;

namespace WebRTCDirectXClient
{
    class App : IFrameworkView, IFrameworkViewSource
    {
        private AppCallbacks _appCallbacks;
        //public RawVideoSource _rawVideo;
        public EncodedVideoSource _encodedVideo;
        private MediaVideoTrack _peerVideoTrack;
        private Peer _selectedPeer;

        public App()
        {
            _appCallbacks = new AppCallbacks();
        }

        public virtual void Initialize(CoreApplicationView applicationView)
        {
            applicationView.Activated += ApplicationView_Activated;
            CoreApplication.Suspending += CoreApplication_Suspending;

            _appCallbacks.Initialize(applicationView);
        }

        private void CoreApplication_Suspending(object sender, SuspendingEventArgs e)
        {
        }

        private void ApplicationView_Activated(CoreApplicationView sender, IActivatedEventArgs args)
        {
            CoreWindow.GetForCurrentThread().Activate();
        }

        public void SetWindow(CoreWindow window)
        {
            ApplicationView.GetForCurrentView().TryEnterFullScreenMode();

            // Initializes DirectX.
            _appCallbacks.SetWindow(window);

            // Initializes webrtc.
            WebRTC.Initialize(CoreApplication.MainView.CoreWindow.Dispatcher);
            Conductor.Instance.ETWStatsEnabled = false;

            Conductor.Instance.Signaller.OnPeerConnected += (peerId, peerName) =>
            {
                Conductor.Instance.Peers.Add(
                    _selectedPeer = new Peer { Id = peerId, Name = peerName });
            };

            Conductor.Instance.Signaller.OnPeerDisconnected += peerId =>
            {
                var peerToRemove = Conductor.Instance.Peers?.FirstOrDefault(p => p.Id == peerId);
                if (peerToRemove != null)
                {
                    Conductor.Instance.Peers.Remove(peerToRemove);
                }
            };

            Conductor.Instance.OnAddRemoteStream += Conductor_OnAddRemoteStream;
            Conductor.Instance.OnRemoveRemoteStream += Conductor_OnRemoveRemoteStream;
            Conductor.Instance.OnAddLocalStream += Conductor_OnAddLocalStream;
            LoadSettings();

            if (Conductor.Instance.Peers == null)
            {
                Conductor.Instance.Peers = new ObservableCollection<Peer>();
            }

            Task.Run(() =>
            {
                var videoCodecList = WebRTC.GetVideoCodecs().OrderBy(codec =>
                {
                    switch (codec.Name)
                    {
                        case "VP8": return 1;
                        case "VP9": return 2;
                        case "H264": return 3;
                        default: return 99;
                    }
                });

                //Conductor.Instance.VideoCodec = videoCodecList.FirstOrDefault(x => x.Name.Contains("VP8"));
                Conductor.Instance.VideoCodec = videoCodecList.FirstOrDefault(x => x.Name.Contains("H264"));

                var audioCodecList = WebRTC.GetAudioCodecs();
                string[] incompatibleAudioCodecs = new string[] { "CN32000", "CN16000", "CN8000", "red8000", "telephone-event8000" };
                var audioCodecs = new List<CodecInfo>();
                foreach (var audioCodec in audioCodecList)
                {
                    if (!incompatibleAudioCodecs.Contains(audioCodec.Name + audioCodec.ClockRate))
                    {
                        audioCodecs.Add(audioCodec);
                    }
                }

                if (audioCodecs.Count > 0)
                {
                    Conductor.Instance.AudioCodec = audioCodecs.FirstOrDefault(x => x.Name.Contains("PCMU"));
                }

                Conductor.Instance.DisableLocalVideoStream();
                Conductor.Instance.MuteMicrophone();
            });
        }

        private void Conductor_OnAddLocalStream(MediaStreamEvent obj)
        {
        }

        private void Conductor_OnRemoveRemoteStream(MediaStreamEvent obj)
        {
        }

        public void Load(string entryPoint)
        {
        }

        public void Run()
        {
            Task.Run(() =>
            {
                Conductor.Instance.StartLogin("127.0.0.1", "8888");
            });

            _appCallbacks.Run();
        }

        public void Uninitialize()
        {
        }

        [MTAThread]
        static void Main(string[] args)
        {
            var app = new App();
            CoreApplication.Run(app);
        }

        public IFrameworkView CreateView()
        {
            return this;
        }

        void LoadSettings()
        {
            var configIceServers = new ObservableCollection<IceServer>();

            bool useDefaultIceServers = true;
            if (useDefaultIceServers)
            {
                // Default values:
                configIceServers.Clear();
                configIceServers.Add(new IceServer("stun.l.google.com:19302", IceServer.ServerType.STUN));
                configIceServers.Add(new IceServer("stun1.l.google.com:19302", IceServer.ServerType.STUN));
                configIceServers.Add(new IceServer("stun2.l.google.com:19302", IceServer.ServerType.STUN));
                configIceServers.Add(new IceServer("stun3.l.google.com:19302", IceServer.ServerType.STUN));
                configIceServers.Add(new IceServer("stun4.l.google.com:19302", IceServer.ServerType.STUN));
            }

            Conductor.Instance.ConfigureIceServers(configIceServers);
        }

        private void Conductor_OnAddRemoteStream(MediaStreamEvent evt)
        {
            _peerVideoTrack = evt.Stream.GetVideoTracks().FirstOrDefault();
            if (_peerVideoTrack != null)
            {
                //_rawVideo = Media.CreateMedia().CreateRawVideoSource(_peerVideoTrack);
                //_rawVideo.OnRawVideoFrame += Source_OnRawVideoFrame;
                _encodedVideo = Media.CreateMedia().CreateEncodedVideoSource(_peerVideoTrack);
                _encodedVideo.OnEncodedVideoFrame += Source_OnEncodedVideoFrame;
            }
        }

        private void Source_OnRawVideoFrame(
            uint width,
            uint height,
            byte[] dataY,
            uint strideY,
            byte[] dataU,
            uint strideU,
            byte[] dataV,
            uint strideV)
        {
            _appCallbacks.OnFrame(width, height, dataY, strideY, dataU, strideU, dataV, strideV);
        }

        private void Source_OnEncodedVideoFrame(
            uint width,
            uint height,
            byte[] encodedData)
        {
            _appCallbacks.OnEncodedFrame(width, height, encodedData);
        }
    }
}
