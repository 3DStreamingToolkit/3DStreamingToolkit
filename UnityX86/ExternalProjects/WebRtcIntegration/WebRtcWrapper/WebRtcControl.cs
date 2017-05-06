// Based on PeerConnection Client Sample
// https://github.com/webrtc-uwp/PeerCC

// INTIAL WORK FOR UNITY SPECIFIC WRAPPER -- without XAML hooks

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.ApplicationModel.Core;
using Windows.UI.Core;
using Org.WebRtc;
using PeerConnectionClient.Model;
using PeerConnectionClient.Signalling;
using PeerConnectionClient.MVVM;
using PeerConnectionClient.Utilities;

namespace WebRtcWrapper
{
    public class WebRtcControl : DispatcherBindableBase
    {
        public event Action OnInitialized;
        public event Action<int, string> OnPeerMessageDataReceived;
        public event Action<string> OnStatusMessageUpdate;

        public RawVideoSource rawVideo;
        public EncodedVideoSource encodedVideoSource;

        // Message Data Type
        private static readonly string kMessageDataType = "message";

        private readonly CoreDispatcher _uiDispatcher;

        public WebRtcControl() : base(CoreApplication.MainView.CoreWindow.Dispatcher)
        {
            _uiDispatcher = CoreApplication.MainView.CoreWindow.Dispatcher;
        }
    }
}
