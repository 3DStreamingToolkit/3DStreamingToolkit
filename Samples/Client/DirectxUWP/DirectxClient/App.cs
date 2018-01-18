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

namespace StreamingDirectXHololensClient
{
    class App : IFrameworkView, IFrameworkViewSource
    {
        private const string DEFAULT_MEDIA_SOURCE_ID = "media";
        private const int VIDEO_FRAME_WIDTH = 1280 * 2;
        private const int VIDEO_FRAME_HEIGHT = 720;

        private AppCallbacks _appCallbacks;
        private WebRtcControl _webRtcControl;
        private IMediaSource _mediaSource;

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
                    MediaSourceReadyDelegate mediaSourceReadyDelegate = (mediaSource) =>
                    {
                        _appCallbacks.SetMediaStreamSource(
                            (MediaStreamSource)mediaSource,
                            VIDEO_FRAME_WIDTH,
                            VIDEO_FRAME_HEIGHT);
                    };

                    SampleTimestampDelegate sampleTimestampDelegate = (id, timestamp) =>
                    {
                        _appCallbacks.OnSampleTimestamp(id, timestamp);
                    };

                    FpsReportRequestedDelegate fpsReportRequestedDelegate = () =>
                    {
                        return _appCallbacks.OnFpsReportRequested();
                    };

                    Media.CreateMedia().CreateMediaStreamSource(
                        peerVideoTrack,
                        DEFAULT_MEDIA_SOURCE_ID,
                        VIDEO_FRAME_WIDTH,
                        VIDEO_FRAME_HEIGHT,
                        mediaSourceReadyDelegate,
                        sampleTimestampDelegate,
                        fpsReportRequestedDelegate);
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
