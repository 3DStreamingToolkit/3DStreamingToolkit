using DirectXClientComponent;
using Org.WebRtc;
using PeerConnectionClient.Signalling;
using System;
using System.Linq;
using WebRtcWrapper;
using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.ApplicationModel.Core;
using Windows.Media.Core;
using Windows.UI.Core;
using Windows.UI.ViewManagement;

namespace StreamingDirectXHololensClient
{
    class App : IFrameworkView, IFrameworkViewSource
    {
        private const int DEFAULT_FRAME_RATE = 30;
        private const string DEFAULT_MEDIA_SOURCE_ID = "media";

        private AppCallbacks _appCallbacks;
        private WebRtcControl _webRtcControl;

        public App()
        {
            _appCallbacks = new AppCallbacks(SendInputData);
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
        }

        public void Load(string entryPoint)
        {
        }

        public void Run()
        {
            // Initializes webrtc.
            _webRtcControl = new WebRtcControl("ms-appx:///webrtcConfig.json");
            _webRtcControl.OnInitialized += (() =>
            {
                _webRtcControl.ConnectToServer();
            });

            Conductor.Instance.OnAddRemoteStream += ((evt) =>
            {
                var peerVideoTrack = evt.Stream.GetVideoTracks().FirstOrDefault();
                if (peerVideoTrack != null)
                {
                    var media = Media.CreateMedia().CreateMediaStreamSource(
                        peerVideoTrack, DEFAULT_FRAME_RATE, DEFAULT_MEDIA_SOURCE_ID, (id, timestamp) =>
                        {
                            _appCallbacks.OnSampleTimestamp(id, timestamp);
                        });

                    _appCallbacks.SetMediaStreamSource((MediaStreamSource)media);
                }

                _webRtcControl.IsReadyToDisconnect = true;
            });

            _webRtcControl.Initialize();

            // Starts the main render loop.
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

        private bool SendInputData(string msg)
        {
            return Conductor.Instance.SendPeerDataChannelMessage(msg);
        }
    }
}
