//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

using System;
using System.Linq;
using System.Collections.Generic;
using Windows.Networking.Connectivity;
using Windows.Networking;
using Windows.Data.Json;
using System.Diagnostics;
using System.Threading.Tasks;
using PeerConnectionClient.Model;
using System.Collections.ObjectModel;
using System.Threading;
using System.Text.RegularExpressions;
using static System.String;
#if ORTCLIB
using Org.Ortc;
using Org.Ortc.Adapter;
using PeerConnectionClient.Ortc;
using PeerConnectionClient.Ortc.Utilities;
using CodecInfo = Org.Ortc.RTCRtpCodecCapability;
using MediaVideoTrack = Org.Ortc.MediaStreamTrack;
using MediaAudioTrack = Org.Ortc.MediaStreamTrack;
using RTCIceCandidate = Org.Ortc.Adapter.RTCIceCandidate;
#else
using Org.WebRtc;
using PeerConnectionClient.Utilities;
#endif

namespace PeerConnectionClient.Signalling
{
    /// <summary>
    /// A singleton conductor for WebRTC session.
    /// </summary>
    public class Conductor
    {
        private static readonly object InstanceLock = new object();
        private static Conductor _instance;
#if ORTCLIB
        private RTCPeerConnectionSignalingMode _signalingMode;
#endif
        /// <summary>
        ///  The single instance of the Conductor class.
        /// </summary>
        public static Conductor Instance
        {
            get
            {
                if (_instance == null)
                {
                    lock (InstanceLock)
                    {
                        if (_instance == null)
                        {
                            _instance = new Conductor();
                        }
                    }
                }
                return _instance;
            }
        }

        private readonly Signaller _signaller;

        /// <summary>
        /// The signaller property.
        /// Helps to pass WebRTC session signals between client and server.
        /// </summary>
        public Signaller Signaller => _signaller;
        
        /// <summary>
        /// Video codec used in WebRTC session.
        /// </summary>
        public CodecInfo VideoCodec { get; set; }

        /// <summary>
        /// Audio codec used in WebRTC session.
        /// </summary>
        public CodecInfo AudioCodec { get; set; }

        /// <summary>
        /// Video capture details (frame rate, resolution)
        /// </summary>
        public CaptureCapability VideoCaptureProfile;

        // SDP negotiation attributes
        private static readonly string kCandidateSdpMidName = "sdpMid";
        private static readonly string kCandidateSdpMlineIndexName = "sdpMLineIndex";
        private static readonly string kCandidateSdpName = "candidate";
        private static readonly string kSessionDescriptionTypeName = "type";
        private static readonly string kSessionDescriptionSdpName = "sdp";

        // Message Data Type
        private static readonly string kMessageDataType = "message";

#if ORTCLIB
        private static readonly string kSessionDescriptionJsonName = "session";
#endif
        RTCPeerConnection _peerConnection;
        private RTCDataChannel _peerSendDataChannel;
        private RTCDataChannel _peerReceiveDataChannel;

        readonly Media _media;

        /// <summary>
        /// Media details.
        /// </summary>
        public Media Media => _media;	

        public ObservableCollection<Peer> Peers;
        public Peer Peer;
        private MediaStream _mediaStream;
        readonly List<RTCIceServer> _iceServers;

        private int _peerId = -1;
        protected bool VideoEnabled;
        protected bool AudioEnabled;
        protected string SessionId;

        bool _etwStatsEnabled;

        /// <summary>
        /// Enable/Disable ETW stats used by WebRTCDiagHubTool Visual Studio plugin.
        /// If the ETW Stats are disabled, no data will be sent to the plugin.
        /// </summary>
        public bool ETWStatsEnabled
        {
            get
            {
                return _etwStatsEnabled;
            }
            set
            {
                _etwStatsEnabled = value;
#if !ORTCLIB
                if (_peerConnection != null)
                {
                    _peerConnection.EtwStatsEnabled = value;
                }
#endif
            }
        }

        bool _peerConnectionStatsEnabled;

        /// <summary>
        /// Enable/Disable connection health stats.
        /// Connection health stats are delivered by the OnConnectionHealthStats event. 
        /// </summary>
        public bool PeerConnectionStatsEnabled
        {
            get
            {
                return _peerConnectionStatsEnabled;
            }
            set
            {
                _peerConnectionStatsEnabled = value;
#if !ORTCLIB
                if (_peerConnection != null)
                {
                    _peerConnection.ConnectionHealthStatsEnabled = value;
                }
#endif
            }
        }

        public object MediaLock { get; set; } = new object();

        CancellationTokenSource _connectToPeerCancelationTokenSource;
        Task<bool> _connectToPeerTask;

        // Public events for adding and removing the local stream
        public event Action<MediaStreamEvent> OnAddLocalStream;

        // Public events to notify about connection status
        public event Action OnPeerConnectionCreated;
        public event Action OnPeerConnectionClosed;
        public event Action OnReadyToConnect;

        // Specialized Message Handling Messages
        public event Action<int, string> OnPeerMessageDataReceived;
        public event Action<int, string> OnPeerDataChannelReceived;



        /// <summary>
        /// Updates the preferred video frame rate and resolution.
        /// </summary>
        public void UpdatePreferredFrameFormat()
        {
          if (VideoCaptureProfile != null)
          {
#if ORTCLIB
            _media.SetPreferredVideoCaptureFormat(
              (int)VideoCaptureProfile.Width, (int)VideoCaptureProfile.Height, (int)VideoCaptureProfile.FrameRate);
#else
            Org.WebRtc.WebRTC.SetPreferredVideoCaptureFormat(
                          (int)VideoCaptureProfile.Width, (int)VideoCaptureProfile.Height, (int)VideoCaptureProfile.FrameRate);
#endif
            }
        }

        /// <summary>
        /// Creates a peer connection.
        /// </summary>
        /// <returns>True if connection to a peer is successfully created.</returns>
        private async Task<bool> CreatePeerConnection(CancellationToken cancelationToken)
        {
            Debug.Assert(_peerConnection == null);
            if(cancelationToken.IsCancellationRequested)
            {
                return false;
            }
            
            var config = new RTCConfiguration()
            {
                BundlePolicy = RTCBundlePolicy.Balanced,
#if ORTCLIB
                SignalingMode = _signalingMode,
                GatherOptions = new RTCIceGatherOptions()
                {
                    IceServers = new List<RTCIceServer>(_iceServers),
                }
#else
                IceTransportPolicy = RTCIceTransportPolicy.All,
                IceServers = _iceServers
#endif
            };

            Debug.WriteLine("Conductor: Creating peer connection.");
            _peerConnection = new RTCPeerConnection(config);

            if (_peerConnection == null)
                throw new NullReferenceException("Peer connection is not created.");

#if !ORTCLIB
            _peerConnection.EtwStatsEnabled = _etwStatsEnabled;
            _peerConnection.ConnectionHealthStatsEnabled = _peerConnectionStatsEnabled;
#endif
            if (cancelationToken.IsCancellationRequested)
            {
                return false;
            }
#if ORTCLIB
            OrtcStatsManager.Instance.Initialize(_peerConnection);
#endif
            OnPeerConnectionCreated?.Invoke();

            _peerConnection.OnIceCandidate += PeerConnection_OnIceCandidate;
#if ORTCLIB
            _peerConnection.OnTrack += PeerConnection_OnAddTrack;
            _peerConnection.OnTrackGone += PeerConnection_OnRemoveTrack;
            _peerConnection.OnIceConnectionStateChange += () => { Debug.WriteLine("Conductor: Ice connection state change, state=" + (null != _peerConnection ? _peerConnection.IceConnectionState.ToString() : "closed")); };
#else
            _peerConnection.OnAddStream += PeerConnection_OnAddStream;
            _peerConnection.OnRemoveStream += PeerConnection_OnRemoveStream;
            _peerConnection.OnConnectionHealthStats += PeerConnection_OnConnectionHealthStats;
#endif

            // Setup Data Channel            
            _peerSendDataChannel = _peerConnection.CreateDataChannel(
                "SendDataChannel", 
                new RTCDataChannelInit(){
                    Ordered = true
                });
            _peerSendDataChannel.OnOpen += PeerSendDataChannelOnOpen;
            _peerSendDataChannel.OnClose += PeerSendDataChannelOnClose;
            _peerSendDataChannel.OnError += _peerSendDataChannel_OnError;
            _peerConnection.OnDataChannel += _peerConnection_OnDataChannel;     // DataChannel Setup Completed            

            Debug.WriteLine("Conductor: Getting user media.");
            RTCMediaStreamConstraints mediaStreamConstraints = new RTCMediaStreamConstraints
            {
                // Always include audio/video enabled in the media stream,
                // so it will be possible to enable/disable audio/video if 
                // the call was initiated without microphone/camera

                audioEnabled = true,
                videoEnabled = true
            };

            if (cancelationToken.IsCancellationRequested)
            {
                return false;
            }

#if ORTCLIB
            var tracks = await _media.GetUserMedia(mediaStreamConstraints);
            if (tracks != null)
            {
                RTCRtpCapabilities audioCapabilities = RTCRtpSender.GetCapabilities("audio");
                RTCRtpCapabilities videoCapabilities = RTCRtpSender.GetCapabilities("video");

                _mediaStream = new MediaStream(tracks);
                Debug.WriteLine("Conductor: Adding local media stream.");
                IList<MediaStream> mediaStreamList = new List<MediaStream>();
                mediaStreamList.Add(_mediaStream);
                foreach (var mediaStreamTrack in tracks)
                {
                    //Create stream track configuration based on capabilities
                    RTCMediaStreamTrackConfiguration configuration = null;
                    if (mediaStreamTrack.Kind == MediaStreamTrackKind.Audio && audioCapabilities != null)
                    {
                        configuration =
                            await Helper.GetTrackConfigurationForCapabilities(audioCapabilities, AudioCodec);
                    }
                    else if (mediaStreamTrack.Kind == MediaStreamTrackKind.Video && videoCapabilities != null)
                    {
                        configuration =
                            await Helper.GetTrackConfigurationForCapabilities(videoCapabilities, VideoCodec);
                    }
                    if (configuration != null)
                        _peerConnection.AddTrack(mediaStreamTrack, mediaStreamList, configuration);
                }
            }
#else            
            _mediaStream = await _media.GetUserMedia(mediaStreamConstraints);
#endif
            if (cancelationToken.IsCancellationRequested)
            {
                return false;
            }

#if !ORTCLIB
            Debug.WriteLine("Conductor: Adding local media stream.");
            // HACK
            if (_mediaStream == null)
            {
                Debug.WriteLine("CONDUCTOR: LOCAL MEDIA STREAM NULL");
            }
            _peerConnection.AddStream(_mediaStream);                       
#endif

            OnAddLocalStream?.Invoke(new MediaStreamEvent() { Stream = _mediaStream });
            if (cancelationToken.IsCancellationRequested)
            {
                return false;
            }
            return true;
        }

        private void _peerSendDataChannel_OnError()
        {
            // TODO: _peerSendDataChannel_OnError()
            Debug.WriteLine("Peer Data Channel Error");
        }

        private void _peerConnection_OnDataChannel(RTCDataChannelEvent rtcDataChannel)
        {
            _peerReceiveDataChannel = rtcDataChannel.Channel;
            _peerReceiveDataChannel.OnMessage += _peerReceiveDataChannel_OnMessage;
        }

        private void _peerReceiveDataChannel_OnMessage(RTCDataChannelMessageEvent rtcMessage)
        {
            var msg = ((StringDataChannelMessage) rtcMessage.Data).StringData;
            OnPeerDataChannelReceived?.Invoke(_peerId, msg);
            Debug.WriteLine("DataChannel: {0}-{1}", _peerId, msg);
        }

        public void SendPeerDataChannelMessage(string msg)
        {
            _peerSendDataChannel.Send(new StringDataChannelMessage(msg));
        }

        private void PeerSendDataChannelOnClose()
        {
            // TODO: PeerSendDataChannelOnClose()            
            Debug.WriteLine("Peer Data Channel OnClose()");
        }

        private void PeerSendDataChannelOnOpen()
        {
            // TODO: PeerSendDataChannelOnOpen()
            Debug.WriteLine("Peer Data Channel Close");
        }

        /// <summary>
        /// Closes a peer connection.
        /// </summary>
        //private async Task ClosePeerConnection()
        private void ClosePeerConnection()
        {                
            lock (MediaLock)
            {                
                if (_peerConnection != null)
                {
                    _peerId = -1;
                    if (_mediaStream != null)
                    {
                        foreach (var track in _mediaStream.GetTracks())
                        {
                            // Check Track Status before action to avoid reference errors
                            // CRASH condition previously on non-XAML usage
                            if (track != null)
                            {
                                if (track.Enabled)
                                {
                                    track.Stop();
                                }                                        
                                _mediaStream.RemoveTrack(track);
                            }
                        }
                    }
                    _mediaStream = null;
                    
                    // TODO: Cleanup DataChannel
                    if(_peerSendDataChannel != null)
                    {                        
                        _peerSendDataChannel.Close();
                        _peerSendDataChannel = null;
                    }

                    if(_peerReceiveDataChannel != null)
                    {
                        _peerReceiveDataChannel.Close();
                        _peerReceiveDataChannel = null;
                    }

                    OnPeerConnectionClosed?.Invoke();

                    _peerConnection.Close(); // Slow, so do this after UI updated and camera turned off

                    SessionId = null;
    #if ORTCLIB
            OrtcStatsManager.Instance.CallEnded();
    #endif
                    _peerConnection = null;

                    OnReadyToConnect?.Invoke();

                    // TODO: handle GC
                    //GC.Collect(); // Ensure all references are truly dropped.
                }
            }                
        }

        /// <summary>
        /// Called when WebRTC detects another ICE candidate. 
        /// This candidate needs to be sent to the other peer.
        /// </summary>
        /// <param name="evt">Details about RTC Peer Connection Ice event.</param>
        private void PeerConnection_OnIceCandidate(RTCPeerConnectionIceEvent evt)
        {
            if (evt.Candidate == null) // relevant: GlobalObserver::OnIceComplete in Org.WebRtc
            {
                return;
            }

            double index = null != evt.Candidate.SdpMLineIndex ? (double)evt.Candidate.SdpMLineIndex : -1;

            JsonObject json;
#if ORTCLIB
            if (RTCPeerConnectionSignalingMode.Json == _signalingMode)
            {
                json = JsonObject.Parse(evt.Candidate.ToJsonString());
            }
            else
#endif
            {
                json = new JsonObject
                {
                    {kCandidateSdpMidName, JsonValue.CreateStringValue(evt.Candidate.SdpMid)},
                    {kCandidateSdpMlineIndexName, JsonValue.CreateNumberValue(index)},
                    {kCandidateSdpName, JsonValue.CreateStringValue(evt.Candidate.Candidate)}
                };
            }
            Debug.WriteLine("Conductor: Sending ice candidate.\n" + json.Stringify());
            SendMessage(json);
        }

#if ORTCLIB
        /// <summary>
        /// Invoked when the remote peer added a media track to the peer connection.
        /// </summary>
        public event Action<RTCTrackEvent> OnAddRemoteTrack;
        private void PeerConnection_OnAddTrack(RTCTrackEvent evt)
        {
            OnAddRemoteTrack?.Invoke(evt);
        }

        /// <summary>
        /// Invoked when the remote peer removed a media track from the peer connection.
        /// </summary>
        public event Action<RTCTrackEvent> OnRemoveTrack;
        private void PeerConnection_OnRemoveTrack(RTCTrackEvent evt)
        {
            OnRemoveTrack?.Invoke(evt);
        }
#else
        /// <summary>
        /// Invoked when the remote peer added a media stream to the peer connection.
        /// </summary>
        public event Action<MediaStreamEvent> OnAddRemoteStream;
        private void PeerConnection_OnAddStream(MediaStreamEvent evt)
        {
            OnAddRemoteStream?.Invoke(evt);
        }

        /// <summary>
        /// Invoked when the remote peer removed a media stream from the peer connection.
        /// </summary>
        public event Action<MediaStreamEvent> OnRemoveRemoteStream;
        private void PeerConnection_OnRemoveStream(MediaStreamEvent evt)
        {
            OnRemoveRemoteStream?.Invoke(evt);
        }

        /// <summary>
        /// Invoked when new connection health stats are available.
        /// Use ToggleConnectionHealthStats to turn on/of the connection health stats.
        /// </summary>
        public event Action<RTCPeerConnectionHealthStats> OnConnectionHealthStats;
        private void PeerConnection_OnConnectionHealthStats(RTCPeerConnectionHealthStats stats)
        {
            OnConnectionHealthStats?.Invoke(stats);
        }
#endif
        /// <summary>
        /// Private constructor for singleton class.
        /// </summary>
        private Conductor()
        {
#if ORTCLIB
            _signalingMode = RTCPeerConnectionSignalingMode.Json;
//#else
            //_signalingMode = RTCPeerConnectionSignalingMode.Sdp;
#endif
            _signaller = new Signaller();
            _media = Media.CreateMedia();

            Signaller.OnDisconnected += Signaller_OnDisconnected;
            Signaller.OnMessageFromPeer += Signaller_OnMessageFromPeer;
            Signaller.OnPeerConnected += Signaller_OnPeerConnected;
            Signaller.OnPeerHangup += Signaller_OnPeerHangup;
            Signaller.OnPeerDisconnected += Signaller_OnPeerDisconnected;
            Signaller.OnServerConnectionFailure += Signaller_OnServerConnectionFailure;
            Signaller.OnSignedIn += Signaller_OnSignedIn;

            _iceServers = new List<RTCIceServer>();
        }

        /// <summary>
        /// Handler for Signaller's OnPeerHangup event.
        /// </summary>
        /// <param name="peerId">ID of the peer to hung up the call with.</param>
        void Signaller_OnPeerHangup(int peerId)
        {
            if (peerId != _peerId) return;

            Debug.WriteLine("Conductor: Our peer hung up.");
            ClosePeerConnection();
        }

        /// <summary>
        /// Handler for Signaller's OnSignedIn event.
        /// </summary>
        private void Signaller_OnSignedIn()
        {
        }

        /// <summary>
        /// Handler for Signaller's OnServerConnectionFailure event.
        /// </summary>
        private void Signaller_OnServerConnectionFailure()
        {
            Debug.WriteLine("[Error]: Connection to server failed!");
        }

        /// <summary>
        /// Handler for Signaller's OnPeerDisconnected event.
        /// </summary>
        /// <param name="peerId">ID of disconnected peer.</param>
        private void Signaller_OnPeerDisconnected(int peerId)
        {
            // is the same peer or peer_id does not exist (0) in case of 500 Error
            if (peerId != _peerId && peerId != 0) return;

            Debug.WriteLine("Conductor: Our peer disconnected.");
            ClosePeerConnection();
        }

        /// <summary>
        /// Handler for Signaller's OnPeerConnected event.
        /// </summary>
        /// <param name="id">ID of the connected peer.</param>
        /// <param name="name">Name of the connected peer.</param>
        private void Signaller_OnPeerConnected(int id, string name)
        {
        }

        /// <summary>
        /// Handler for Signaller's OnMessageFromPeer event.
        /// </summary>
        /// <param name="peerId">ID of the peer.</param>
        /// <param name="message">Message from the peer.</param>
        private void Signaller_OnMessageFromPeer(int peerId, string message)
        {
            Task.Run(async () =>
            {
                Debug.Assert(_peerId == peerId || _peerId == -1);
                Debug.Assert(message.Length > 0);

                if (_peerId != peerId && _peerId != -1)
                {
                    Debug.WriteLine("[Error] Conductor: Received a message from unknown peer while already in a conversation with a different peer.");
                    return;
                }

                JsonObject jMessage;
                if (!JsonObject.TryParse(message, out jMessage))
                {
                    Debug.WriteLine("[Error] Conductor: Received unknown message." + message);
                    return;
                }

                string type = jMessage.ContainsKey(kSessionDescriptionTypeName) ? jMessage.GetNamedString(kSessionDescriptionTypeName) : null;
#if ORTCLIB
                bool created = false;
#endif
                if (_peerConnection == null)
                {
                    if (!IsNullOrEmpty(type))
                    {
                        // Create the peer connection only when call is
                        // about to get initiated. Otherwise ignore the
                        // messages from peers which could be a result
                        // of old (but not yet fully closed) connections.
                        if (type == "offer" || type == "answer" || type == "json")
                        {
                            Debug.Assert(_peerId == -1);
                            _peerId = peerId;              

                            IEnumerable<Peer> enumerablePeer = Peers.Where(x => x.Id == peerId);
                            Peer = enumerablePeer.First();
#if ORTCLIB
                            created = true;
                            _signalingMode = Helper.SignalingModeForClientName(Peer.Name);
#endif
                            _connectToPeerCancelationTokenSource = new CancellationTokenSource();
                            _connectToPeerTask = CreatePeerConnection(_connectToPeerCancelationTokenSource.Token);
                            bool connectResult = await _connectToPeerTask;
                            _connectToPeerTask = null;
                            _connectToPeerCancelationTokenSource.Dispose();
                            if (!connectResult)
                            {
                                Debug.WriteLine("[Error] Conductor: Failed to initialize our PeerConnection instance");
                                await Signaller.SignOut();
                                return;
                            }
                            else if (_peerId != peerId)
                            {
                                Debug.WriteLine("[Error] Conductor: Received a message from unknown peer while already in a conversation with a different peer.");
                                return;
                            }
                        }
                    }
                    else
                    {
                        Debug.WriteLine("[Warn] Conductor: Received an untyped message after closing peer connection.");
                        return;
                    }
                }

                if (_peerConnection != null && !IsNullOrEmpty(type))
                {
                    // Data Message Intercept
                    if (type == kMessageDataType)
                    {
                        OnPeerMessageDataReceived?.Invoke(peerId, message);
                    }

                    if (type == "offer-loopback")
                    {
                        // Loopback not supported
                        Debug.Assert(false);
                    }
                    string sdp = null;
#if ORTCLIB
                    if (jMessage.ContainsKey(kSessionDescriptionJsonName))
                    {
                        var containerObject = new JsonObject { { kSessionDescriptionJsonName, jMessage.GetNamedObject(kSessionDescriptionJsonName) } };
                        sdp = containerObject.Stringify();
                    }
                    else if (jMessage.ContainsKey(kSessionDescriptionSdpName))
                    {
                        sdp = jMessage.GetNamedString(kSessionDescriptionSdpName);
                    }
#else
                    sdp = jMessage.ContainsKey(kSessionDescriptionSdpName) ? jMessage.GetNamedString(kSessionDescriptionSdpName) : null;
#endif
                    if (IsNullOrEmpty(sdp))
                    {
                        Debug.WriteLine("[Error] Conductor: Can't parse received session description message.");
                        return;
                    }

#if ORTCLIB
                    RTCSessionDescriptionSignalingType messageType = RTCSessionDescriptionSignalingType.SdpOffer;
                    switch (type)
                    {
                        case "json": messageType = RTCSessionDescriptionSignalingType.Json; break;
                        case "offer": messageType = RTCSessionDescriptionSignalingType.SdpOffer; break;
                        case "answer": messageType = RTCSessionDescriptionSignalingType.SdpAnswer; break;
                        case "pranswer": messageType = RTCSessionDescriptionSignalingType.SdpPranswer; break;
                        default: Debug.Assert(false, type); break;
                    }
#else
                    RTCSdpType messageType = RTCSdpType.Offer;
                    switch (type)
                    {
                        case "offer": messageType = RTCSdpType.Offer; break;
                        case "answer": messageType = RTCSdpType.Answer; break;
                        case "pranswer": messageType = RTCSdpType.Pranswer; break;
                        default: Debug.Assert(false, type); break;
                    }
#endif
                    Debug.WriteLine("Conductor: Received session description: " + message);
                    await _peerConnection.SetRemoteDescription(new RTCSessionDescription(messageType, sdp));

#if ORTCLIB
                    if ((messageType == RTCSessionDescriptionSignalingType.SdpOffer) ||
                        ((created) && (messageType == RTCSessionDescriptionSignalingType.Json)))
#else
                    if (messageType == RTCSdpType.Offer)
#endif
                    {
                        var answer = await _peerConnection.CreateAnswer();
                        await _peerConnection.SetLocalDescription(answer);
                        // Send answer
                        SendSdp(answer);
#if ORTCLIB
                        OrtcStatsManager.Instance.StartCallWatch(SessionId, false);
#endif
                    }
                }
                else
                {
                    RTCIceCandidate candidate = null;
#if ORTCLIB
                    if (RTCPeerConnectionSignalingMode.Json != _signalingMode)
#endif
                    {
                        var sdpMid = jMessage.ContainsKey(kCandidateSdpMidName)
                            ? jMessage.GetNamedString(kCandidateSdpMidName)
                            : null;
                        var sdpMlineIndex = jMessage.ContainsKey(kCandidateSdpMlineIndexName)
                            ? jMessage.GetNamedNumber(kCandidateSdpMlineIndexName)
                            : -1;
                        var sdp = jMessage.ContainsKey(kCandidateSdpName)
                            ? jMessage.GetNamedString(kCandidateSdpName)
                            : null;
                        //TODO: Check is this proper condition ((String.IsNullOrEmpty(sdpMid) && (sdpMlineIndex == -1)) || String.IsNullOrEmpty(sdp))
                        if (IsNullOrEmpty(sdpMid) || sdpMlineIndex == -1 || IsNullOrEmpty(sdp))
                        {
                            Debug.WriteLine("[Error] Conductor: Can't parse received message.\n" + message);
                            return;
                        }
#if ORTCLIB
                        candidate = IsNullOrEmpty(sdpMid) ? RTCIceCandidate.FromSdpStringWithMLineIndex(sdp, (ushort)sdpMlineIndex) : RTCIceCandidate.FromSdpStringWithMid(sdp, sdpMid);
#else
                        candidate = new RTCIceCandidate(sdp, sdpMid, (ushort)sdpMlineIndex);
#endif
                    }
#if ORTCLIB
                    else
                    {
                        candidate = RTCIceCandidate.FromJsonString(message);
                    }
                    _peerConnection?.AddIceCandidate(candidate);
#else
                    await _peerConnection.AddIceCandidate(candidate);
#endif


                    Debug.WriteLine("Conductor: Received candidate : " + message);
                }
            }).Wait();
        }

        /// <summary>
        /// Handler for Signaller's OnDisconnected event handler.
        /// </summary>
        private void Signaller_OnDisconnected()
        {
            ClosePeerConnection();
        }

        /// <summary>
        /// Starts the login to server process.
        /// </summary>
        /// <param name="server">The host server.</param>
        /// <param name="port">The port to connect to.</param>
        public void StartLogin(string server, string port, string peerName = "")
        {
            if (_signaller.IsConnected())
            {
                return;
            }
            
            //_signaller.Connect(server, port, GetLocalPeerName());
            _signaller.Connect(server, port, peerName == String.Empty ? GetLocalPeerName() : peerName);
        }
       
        /// <summary>
        /// Calls to disconnect the user from the server.
        /// </summary>
        public async Task DisconnectFromServer()
        {
            if (_signaller.IsConnected())
            {
                await _signaller.SignOut();
            }
        }

        /// <summary>
        /// Calls to connect to the selected peer.
        /// </summary>
        /// <param name="peer">Peer to connect to.</param>
        public async void ConnectToPeer(Peer peer)
        {            
            Debug.Assert(peer != null);
            Debug.Assert(_peerId == -1);

            if (_peerConnection != null)
            {
                Debug.WriteLine("[Error] Conductor: We only support connecting to one peer at a time");
                return;
            }
#if ORTCLIB
            _signalingMode = Helper.SignalingModeForClientName(peer.Name);
#endif
            _connectToPeerCancelationTokenSource = new System.Threading.CancellationTokenSource();
            _connectToPeerTask = CreatePeerConnection(_connectToPeerCancelationTokenSource.Token);
            bool connectResult = await _connectToPeerTask;
            _connectToPeerTask = null;
            _connectToPeerCancelationTokenSource.Dispose();

            if (connectResult)
            {
                _peerId = peer.Id;
                var offer = await _peerConnection.CreateOffer();
#if !ORTCLIB
                // Alter sdp to force usage of selected codecs
                string newSdp = offer.Sdp;
                SdpUtils.SelectCodecs(ref newSdp, AudioCodec, VideoCodec);
                offer.Sdp = newSdp;
#endif
                await _peerConnection.SetLocalDescription(offer);
                Debug.WriteLine("Conductor: Sending offer.");
                SendSdp(offer);
#if ORTCLIB
                OrtcStatsManager.Instance.StartCallWatch(SessionId, true);
#endif
            }
        }

        /// <summary>
        /// Calls to disconnect from peer.
        /// </summary>
        public async Task DisconnectFromPeer()        
        {
            await SendHangupMessage();
            ClosePeerConnection();
        }

        /// <summary>
        /// Constructs and returns the local peer name.
        /// </summary>
        /// <returns>The local peer name.</returns>
        private string GetLocalPeerName()
        {
            var hostname = NetworkInformation.GetHostNames().FirstOrDefault(h => h.Type == HostNameType.DomainName);
            string ret = hostname?.CanonicalName ?? "<unknown host>";
#if ORTCLIB
            ret = ret + "-dual";
#endif
            return ret;
        }

        /// <summary>
        /// Sends SDP message.
        /// </summary>
        /// <param name="description">RTC session description.</param>
        private void SendSdp(RTCSessionDescription description)
        {
            JsonObject json = null;
#if ORTCLIB
            var type = description.Type.ToString().ToLower();
            string formattedDescription = description.FormattedDescription;

            if (description.Type == RTCSessionDescriptionSignalingType.Json)
            {
                if (IsNullOrEmpty(SessionId))
                {
                    var match = Regex.Match(formattedDescription, "{\"username\":\"-*[a-zA-Z0-9]*\",\"id\":\"([0-9]+)\"");
                    if (match.Success)
                    {
                        SessionId = match.Groups[1].Value;
                    }
                }
                var jsonDescription = JsonObject.Parse(formattedDescription);
                var sessionValue = jsonDescription.GetNamedObject(kSessionDescriptionJsonName);
                json = new JsonObject
                {
                    {kSessionDescriptionTypeName, JsonValue.CreateStringValue(type)},
                    {kSessionDescriptionJsonName,  sessionValue}
                };
            }
            else
            {
                var match = Regex.Match(formattedDescription, "o=[^ ]+ ([0-9]+) [0-9]+ [a-zA-Z]+ [a-zA-Z0-9]+ [0-9\\.]+");
                if (match.Success)
                {
                    SessionId = match.Groups[1].Value;
                }

                var prefix = type.Substring(0, "sdp".Length);
                if (prefix == "sdp")
                {
                    type = type.Substring("sdp".Length);
                }

                json = new JsonObject
                {
                    {kSessionDescriptionTypeName, JsonValue.CreateStringValue(type)},
                    {kSessionDescriptionSdpName, JsonValue.CreateStringValue(formattedDescription)}
                };
            }
#else
            json = new JsonObject();
            json.Add(kSessionDescriptionTypeName, JsonValue.CreateStringValue(description.Type.GetValueOrDefault().ToString().ToLower()));
            json.Add(kSessionDescriptionSdpName, JsonValue.CreateStringValue(description.Sdp));
#endif
            SendMessage(json);
        }

        /// <summary>
        /// Helper method to send a message to a peer.
        /// </summary>
        /// <param name="json">Message body.</param>
        private void SendMessage(IJsonValue json)
        {
            // Don't await, send it async.
            var task = _signaller.SendToPeer(_peerId, json);
        }

        /// <summary>
        /// Helper method to send a hangup message to a peer.
        /// </summary>
        private async Task SendHangupMessage()
        {
            await _signaller.SendToPeer(_peerId, "BYE");
        }

        /// <summary>
        /// Enables the local video stream.
        /// </summary>
        public void EnableLocalVideoStream()
        {
            lock (MediaLock)
            {
                if (_mediaStream != null)
                {
                    foreach (MediaVideoTrack videoTrack in _mediaStream.GetVideoTracks())
                    {                        
                        videoTrack.Enabled = true;
                    }
                }
                VideoEnabled = true;
            }
        }

        /// <summary>
        /// Disables the local video stream.
        /// </summary>
        public void DisableLocalVideoStream()
        {
            lock (MediaLock)
            {
                if (_mediaStream != null)
                {
                    foreach (MediaVideoTrack videoTrack in _mediaStream.GetVideoTracks())
                    {
                        videoTrack.Enabled = false;
                    }
                }
                VideoEnabled = false;
            }
        }

        /// <summary>
        /// Mutes the microphone.
        /// </summary>
        public void MuteMicrophone()
        {
            lock (MediaLock)
            {
                if (_mediaStream != null)
                {
                    foreach (MediaAudioTrack audioTrack in _mediaStream.GetAudioTracks())
                    {
                        audioTrack.Enabled = false;
                    }
                }
                AudioEnabled = false;
            }
        }

        /// <summary>
        /// Unmutes the microphone.
        /// </summary>
        public void UnmuteMicrophone()
        {
            lock (MediaLock)
            {
                if (_mediaStream != null)
                {
                    foreach (MediaAudioTrack audioTrack in _mediaStream.GetAudioTracks())
                    {
                        audioTrack.Enabled = true;
                    }
                }
                AudioEnabled = true;
            }
        }

        /// <summary>
        /// Receives a new list of Ice servers and updates the local list of servers.
        /// </summary>
        /// <param name="iceServers">List of Ice servers to configure.</param>
        public void ConfigureIceServers(Collection<IceServer> iceServers)
        {
            _iceServers.Clear();
            foreach(IceServer iceServer in iceServers)
            {
                //Url format: stun:stun.l.google.com:19302
                string url = "stun:";
                if (iceServer.Type == IceServer.ServerType.TURN)
                {
                    url = "turn:";
                }
                RTCIceServer server = null;
                url += iceServer.Host.Value;
#if ORTCLIB
                //url += iceServer.Host.Value;
                server = new RTCIceServer()
                {
                    Urls = new List<string>(),
                };
                server.Urls.Add(url);
#else
                //url += iceServer.Host.Value + ":" + iceServer.Port.Value;
                server = new RTCIceServer { Url = url };
#endif
                if (iceServer.Credential != null)
                {
                    server.Credential = iceServer.Credential;
                }
                if (iceServer.Username != null)
                {
                    server.Username = iceServer.Username;
                }
                _iceServers.Add(server);
            }
        }

        /// <summary>
        /// If a connection to a peer is establishing, requests it's
        /// cancelation and wait the operation to cancel (blocks curren thread).
        /// </summary>
        public void CancelConnectingToPeer()
        {
            if(_connectToPeerTask != null)
            {
                Debug.WriteLine("Conductor: Connecting to peer in progress, canceling");
                _connectToPeerCancelationTokenSource.Cancel();
                _connectToPeerTask.Wait();
                Debug.WriteLine("Conductor: Connecting to peer flow canceled");
            }
        }
    }
}
