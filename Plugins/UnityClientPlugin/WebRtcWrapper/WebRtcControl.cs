// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Org.WebRtc;
using PeerConnectionClient.Model;
using PeerConnectionClient.Signalling;
using PeerConnectionClient.Utilities;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Threading.Tasks;
using Windows.ApplicationModel.Core;
using Windows.Data.Json;
using Windows.Storage;
using Windows.UI.Core;

namespace WebRtcWrapper
{
    public class WebRtcControl
    {
        public event Action OnInitialized;
        public event Action<int, string> OnPeerMessageDataReceived;        
        public event Action<string> OnStatusMessageUpdate;
        public event Action<int, IDataChannelMessage> OnPeerDataChannelReceived;

        public RawVideoSource rawVideo;

        // Message Data Type
        private static readonly string kMessageDataType = "message";

        private MediaVideoTrack _peerVideoTrack;
        private MediaVideoTrack _selfVideoTrack;
        private readonly NtpService _ntpService;

        private readonly CoreDispatcher _uiDispatcher;

        public WebRtcControl()
        {
            _uiDispatcher = CoreApplication.MainView.CoreWindow.Dispatcher;
        }

        #region SETUP ROUTINES
        public void Initialize()
        {
            WebRTC.Initialize(_uiDispatcher);
            Conductor.Instance.ETWStatsEnabled = false;

            Cameras = new ObservableCollection<MediaDevice>();
            Microphones = new ObservableCollection<MediaDevice>();
            AudioPlayoutDevices = new ObservableCollection<MediaDevice>();

            foreach (MediaDevice audioCaptureDevice in Conductor.Instance.Media.GetAudioCaptureDevices())
            {
                Microphones.Add(audioCaptureDevice);
            }

            foreach (MediaDevice audioPlayoutDevice in Conductor.Instance.Media.GetAudioPlayoutDevices())
            {
                AudioPlayoutDevices.Add(audioPlayoutDevice);
            }

            // HACK Remove Automatic Device Assignment
            if (SelectedCamera == null && Cameras.Count > 0)
            {
                SelectedCamera = Cameras.First();
            }

            if (SelectedMicrophone == null && Microphones.Count > 0)
            {
                SelectedMicrophone = Microphones.First();
            }

            Debug.WriteLine("Device Status: SelectedCamera: {0} - SelectedMic: {1}", SelectedCamera == null ? "NULL" : "OK", SelectedMicrophone == null ? "NULL" : "OK");
            if (SelectedAudioPlayoutDevice == null && AudioPlayoutDevices.Count > 0)
            {
                SelectedAudioPlayoutDevice = AudioPlayoutDevices.First();
            }

            Conductor.Instance.Media.OnMediaDevicesChanged += OnMediaDevicesChanged;
            Conductor.Instance.Signaller.OnPeerConnected += (peerId, peerName) =>
            {
                RunOnUiThread(() =>
                {
                    if (Peers == null)
                    {
                        Peers = new ObservableCollection<Peer>();
                        Conductor.Instance.Peers = Peers;
                    }

                    Peers.Add(new Peer { Id = peerId, Name = peerName });
                });
            };

            Conductor.Instance.Signaller.OnPeerDisconnected += peerId =>
            {
                RunOnUiThread(() =>
                {
                    var peerToRemove = Peers?.FirstOrDefault(p => p.Id == peerId);
                    if (peerToRemove != null)
                        Peers.Remove(peerToRemove);
                });
            };

            Conductor.Instance.Signaller.OnSignedIn += () =>
            {
                RunOnUiThread(() =>
                {
                    IsConnected = true;
                    IsMicrophoneEnabled = false;
                    IsCameraEnabled = false;
                    IsConnecting = false;

                    OnStatusMessageUpdate?.Invoke("Signed-In");
                });
            };

            Conductor.Instance.Signaller.OnServerConnectionFailure += () =>
            {
                RunOnUiThread(() =>
                {
                    IsConnecting = false;
                    OnStatusMessageUpdate?.Invoke("Server Connection Failure");
                });
            };

            Conductor.Instance.Signaller.OnDisconnected += () =>
            {
                RunOnUiThread(() =>
                {
                    IsConnected = false;
                    IsMicrophoneEnabled = false;
                    IsCameraEnabled = false;
                    IsDisconnecting = false;
                    Peers?.Clear();
                    OnStatusMessageUpdate?.Invoke("Disconnected");
                });
            };

            Conductor.Instance.Signaller.OnMessageFromPeer += (id, message) =>
            {
                RunOnUiThread(() =>
                {
                    // TODO: Handles All Peer Messages (Signal Channel)
                });
            };

            Conductor.Instance.Signaller.OnPeerConnected += (id, name) =>
            {
                RunOnUiThread(() =>
                {
                    SelectedPeer = Peers.First(x => x.Id == id);
                    OnStatusMessageUpdate?.Invoke(string.Format("Connected Peer: {0}-{1}", SelectedPeer.Id, SelectedPeer.Name));
                });
            };

            // TODO: Restore Event Handler in Utility Wrapper
            // Implemented in Unity Consumer due to Event Handling Issue
            // Conductor.Instance.OnAddRemoteStream += Conductor_OnAddRemoteStream does not propagate
          
            Conductor.Instance.OnRemoveRemoteStream += Conductor_OnRemoveRemoteStream;
            Conductor.Instance.OnAddLocalStream += Conductor_OnAddLocalStream;
            Conductor.Instance.OnConnectionHealthStats += Conductor_OnPeerConnectionHealthStats;
            Conductor.Instance.OnPeerConnectionCreated += () =>
            {
                RunOnUiThread(() =>
                {
                    IsReadyToConnect = false;
                    IsConnectedToPeer = true;
                    IsReadyToDisconnect = false;
                    IsMicrophoneEnabled = false;
                    OnStatusMessageUpdate?.Invoke("Peer Connection Created");                    
                });
            };

            Conductor.Instance.OnPeerConnectionClosed += () =>
            {
                RunOnUiThread(() =>
                {
                    IsConnectedToPeer = false;
                    _peerVideoTrack = null;
                    _selfVideoTrack = null;
                    IsMicrophoneEnabled = false;
                    IsCameraEnabled = false;

                    // TODO: Clean-up References
                    //GC.Collect(); // Ensure all references are truly dropped.

                    OnStatusMessageUpdate?.Invoke("Peer Connection Closed");
                });
            };

            Conductor.Instance.OnPeerMessageDataReceived += (peerId, message) =>
            {
                OnPeerMessageDataReceived?.Invoke(peerId, message);
            };

            // DATA Channel Setup
            Conductor.Instance.OnPeerMessageDataReceived += (i, s) =>
            {
                
            };
            
            Conductor.Instance.OnReadyToConnect += () => { RunOnUiThread(() => { IsReadyToConnect = true; }); };

            IceServers = new ObservableCollection<IceServer>();
            NewIceServer = new IceServer();
            AudioCodecs = new ObservableCollection<CodecInfo>();
            var audioCodecList = WebRTC.GetAudioCodecs();

            string[] incompatibleAudioCodecs = new string[] { "CN32000", "CN16000", "CN8000", "red8000", "telephone-event8000" };
            VideoCodecs = new ObservableCollection<CodecInfo>();

            // TODO: REMOVE DISPLAY LIST SUPPORT
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

            RunOnUiThread(() =>
            {
                foreach (var audioCodec in audioCodecList)
                {
                    if (!incompatibleAudioCodecs.Contains(audioCodec.Name + audioCodec.ClockRate))
                    {
                        AudioCodecs.Add(audioCodec);
                    }
                }

                if (AudioCodecs.Count > 0)
                {
                    SelectedAudioCodec = AudioCodecs.FirstOrDefault(x => x.Name.Contains("PCMU"));
                }

                foreach (var videoCodec in videoCodecList)
                {
                    VideoCodecs.Add(videoCodec);
                }

                if (VideoCodecs.Count > 0)
                {
                    SelectedVideoCodec = VideoCodecs.FirstOrDefault(x => x.Name.Contains("H264"));
                }
            });

            RunOnUiThread(() =>
            {
                OnInitialized?.Invoke();
            });
        }

        async Task LoadSettings()
        {
            // Default values:
            var configTraceServerIp = new ValidableNonEmptyString("127.0.0.1");
            var configTraceServerPort = 8888;

            StorageFile configFile = await StorageFile.GetFileFromApplicationUriAsync(
                new Uri("ms-appx:///Data/StreamingAssets/webrtcConfig.json"))
                .AsTask()
                .ConfigureAwait(false);

            string content = await FileIO.ReadTextAsync(configFile)
                .AsTask()
                .ConfigureAwait(false);

            JsonValue json = JsonValue.Parse(content);

            // Parses server info.
            _server = json.GetObject().GetNamedString("server");
            int startId = 0;
            if (_server.StartsWith("http://"))
            {
                startId = 7;
            }
            else if (_server.StartsWith("https://"))
            {
                startId = 8;

                // TODO: Supports SSL
            }

            _server = _server.Substring(startId);
            _port = new ValidableIntegerString(
                (int)json.GetObject().GetNamedNumber("port"), 0, 65535);

            _heartbeat = new ValidableIntegerString(
                (int)json.GetObject().GetNamedNumber("heartbeat"), 0, 65535);

            var configIceServers = new ObservableCollection<IceServer>();
            bool useDefaultIceServers = true;
            if (useDefaultIceServers)
            {
                // Clears all values.
                configIceServers.Clear();

                // Parses turn server.
                if (json.GetObject().ContainsKey("turnServer"))
                {
                    JsonValue turnServer = json.GetObject().GetNamedValue("turnServer");
                    string uri = turnServer.GetObject().GetNamedString("uri");
                    IceServer iceServer = new IceServer(
                        uri.Substring(uri.IndexOf("turn:") + 5),
                        IceServer.ServerType.TURN);

                    iceServer.Credential = turnServer.GetObject().GetNamedString("username");
                    iceServer.Username = turnServer.GetObject().GetNamedString("password");
                    configIceServers.Add(iceServer);
                }

                // Parses stun server.
                if (json.GetObject().ContainsKey("stunServer"))
                {
                    JsonValue stunServer = json.GetObject().GetNamedValue("stunServer");
                    string uri = stunServer.GetObject().GetNamedString("uri");
                    IceServer iceServer = new IceServer(
                        uri.Substring(uri.IndexOf("stun:") + 5),
                        IceServer.ServerType.STUN);

                    iceServer.Credential = stunServer.GetObject().GetNamedString("username");
                    iceServer.Username = stunServer.GetObject().GetNamedString("password");
                    configIceServers.Add(iceServer);
                }

                // Default ones.
                configIceServers.Add(new IceServer("stun.l.google.com:19302", IceServer.ServerType.STUN));
                configIceServers.Add(new IceServer("stun1.l.google.com:19302", IceServer.ServerType.STUN));
                configIceServers.Add(new IceServer("stun2.l.google.com:19302", IceServer.ServerType.STUN));
                configIceServers.Add(new IceServer("stun3.l.google.com:19302", IceServer.ServerType.STUN));
                configIceServers.Add(new IceServer("stun4.l.google.com:19302", IceServer.ServerType.STUN));
            }

            Conductor.Instance.ConfigureIceServers(configIceServers);
            var ntpServerAddress = new ValidableNonEmptyString("time.windows.com");
            RunOnUiThread(() =>
            {
                IceServers = configIceServers;
                NtpServer = ntpServerAddress;
                Port = new ValidableIntegerString(configTraceServerPort, 0, 65535);
                Ip = new ValidableNonEmptyString(_server);
                NtpServer = ntpServerAddress;
                Port = _port;
                HeartBeat = _heartbeat;
                ReevaluateHasServer();
            });

            Conductor.Instance.ConfigureIceServers(configIceServers);
        }
        #endregion

        #region COMMANDS

        public void SendPeerDataChannelMessage(string msg)
        {
            Conductor.Instance.SendPeerDataChannelMessage(msg);
        }

        public void SendPeerMessageData(string msg)
        {
            Task.Run(async () =>
            {
                if (IsConnectedToPeer)
                {
                    JsonObject jsonMessage;

                    if (!JsonObject.TryParse(msg, out jsonMessage))
                    {
                        jsonMessage = new JsonObject()
                        {
                            { "data", JsonValue.CreateStringValue(msg) }
                        };
                    }

                    var jsonPackage = new JsonObject
                    {
                        { "type", JsonValue.CreateStringValue(kMessageDataType) }
                    };

                    foreach (var o in jsonMessage)
                    {
                        jsonPackage.Add(o.Key, o.Value);
                    }

                    await Conductor.Instance.Signaller.SendToPeer(
                        SelectedPeer.Id, jsonPackage)
                        .ConfigureAwait(false);
                }
            });
        }

        public void ConnectToServer(string peerName, int heartbeatMs)
        {
            Task.Run(async () =>
            {
                IsConnecting = true;
                await LoadSettings().ConfigureAwait(false);
				Conductor.Instance.Signaller.SetHeartbeatMs(Convert.ToInt32(HeartBeat.Value));
                Conductor.Instance.StartLogin(Ip.Value, Port.Value, peerName);
            });
        }

        public void ConnectToServer(string host, string port, string peerName, int heartbeatMs)
        {
            Task.Run(() =>
            {
                IsConnecting = true;
                Ip = new ValidableNonEmptyString(host);
                Port = new ValidableIntegerString(port);
                ConnectToServer(peerName, heartbeatMs);                
            });
        }

        public void ConnectToPeer()
        {
            Debug.WriteLine("Device Status: SelectedCamera: {0} - SelectedMic: {1}", SelectedCamera == null ? "NULL" : "OK", SelectedMicrophone == null ? "NULL" : "OK");
            if (SelectedPeer != null)
            {
                Task.Run(() => 
                {
                    Conductor.Instance.ConnectToPeer(SelectedPeer);
                });
            }
            else
            {
                OnStatusMessageUpdate?.Invoke("SelectedPeer not set");
            }
        }

        public async void DisconnectFromPeer()
        {
            await Conductor.Instance.DisconnectFromPeer().ConfigureAwait(false);
        }

        public async void DisconnectFromServer()
        {
            IsDisconnecting = true;
            await Conductor.Instance.DisconnectFromServer().ConfigureAwait(false);
            Peers?.Clear();
        }
        #endregion

        #region EVENT HANDLERS
        private void OnMediaDevicesChanged(MediaDeviceType mediaType)
        {
            switch (mediaType)
            {
                case MediaDeviceType.MediaDeviceType_VideoCapture:
                    RefreshVideoCaptureDevices(Conductor.Instance.Media.GetVideoCaptureDevices());
                    break;
                case MediaDeviceType.MediaDeviceType_AudioCapture:
                    RefreshAudioCaptureDevices(Conductor.Instance.Media.GetAudioCaptureDevices());
                    break;
                case MediaDeviceType.MediaDeviceType_AudioPlayout:
                    RefreshAudioPlayoutDevices(Conductor.Instance.Media.GetAudioPlayoutDevices());
                    break;
            }
        }

        private void RefreshVideoCaptureDevices(IList<MediaDevice> videoCaptureDevices)
        {
            RunOnUiThread(() => 
            {
                Collection<MediaDevice> videoCaptureDevicesToRemove = new Collection<MediaDevice>();
                foreach (MediaDevice videoCaptureDevice in Cameras)
                {
                    if (videoCaptureDevices.FirstOrDefault(x => x.Id == videoCaptureDevice.Id) == null)
                    {
                        videoCaptureDevicesToRemove.Add(videoCaptureDevice);
                    }
                }

                foreach (MediaDevice removedVideoCaptureDevices in videoCaptureDevicesToRemove)
                {
                    if (SelectedCamera != null && SelectedCamera.Id == removedVideoCaptureDevices.Id)
                    {
                        SelectedCamera = null;
                    }

                    Cameras.Remove(removedVideoCaptureDevices);
                }

                foreach (MediaDevice videoCaptureDevice in videoCaptureDevices)
                {
                    if (Cameras.FirstOrDefault(x => x.Id == videoCaptureDevice.Id) == null)
                    {
                        Cameras.Add(videoCaptureDevice);
                    }
                }

                if ((SelectedCamera == null) && Cameras.Count > 0)
                {
                    Debug.WriteLine("SelectedCamera RefreshVideoCaptureDevices() Update");                    
                    SelectedCamera = Cameras.FirstOrDefault();
                }
            });
        }

        private void RefreshAudioCaptureDevices(IList<MediaDevice> audioCaptureDevices)
        {
            RunOnUiThread(() => 
            {
                var selectedMicrophoneId = SelectedMicrophone?.Id;
                SelectedMicrophone = null;
                Microphones.Clear();
                foreach (MediaDevice audioCaptureDevice in audioCaptureDevices)
                {
                    Microphones.Add(audioCaptureDevice);
                    if (audioCaptureDevice.Id == selectedMicrophoneId)
                    {
                        SelectedMicrophone = Microphones.Last();
                    }
                }

                if (SelectedMicrophone == null)
                {
                    SelectedMicrophone = Microphones.FirstOrDefault();
                }

                if (SelectedMicrophone == null)
                {
                    SelectedMicrophone = Microphones.FirstOrDefault();
                }
            });
        }

        private void RefreshAudioPlayoutDevices(IList<MediaDevice> audioPlayoutDevices)
        {
            RunOnUiThread(() => 
            {
                var selectedPlayoutDeviceId = SelectedAudioPlayoutDevice?.Id;
                SelectedAudioPlayoutDevice = null;
                AudioPlayoutDevices.Clear();
                foreach (MediaDevice audioPlayoutDevice in audioPlayoutDevices)
                {
                    AudioPlayoutDevices.Add(audioPlayoutDevice);
                    if (audioPlayoutDevice.Id == selectedPlayoutDeviceId)
                    {
                        SelectedAudioPlayoutDevice = audioPlayoutDevice;
                    }
                }

                if (SelectedAudioPlayoutDevice == null)
                {
                    SelectedAudioPlayoutDevice = AudioPlayoutDevices.FirstOrDefault();
                }
            });
        }

        private void Conductor_OnRemoveRemoteStream(MediaStreamEvent evt)
        {
            RunOnUiThread(() =>
            {
                // TODO: Setup Remote Video Stream         
            });
        }

        private void Conductor_OnAddLocalStream(MediaStreamEvent evt)
        {
            if (evt == null)
            {
                var msg = "Conductor_OnAddLocalStream--media stream NULL";
                Debug.WriteLine(msg);
                OnStatusMessageUpdate?.Invoke(msg);
            }

            _selfVideoTrack = evt.Stream.GetVideoTracks().FirstOrDefault();
            //if ((_selfVideoTrack != null) && (SelectedCamera != null))
            if (_selfVideoTrack != null)
            {
                Debug.WriteLine("selfVideoTrack Setup-IsCameraEnabled:{0}-IsMicrophoneEnabled:{1}", IsCameraEnabled, IsMicrophoneEnabled);
                RunOnUiThread(() =>
                {                                        
                    if (IsCameraEnabled)
                    {
                        
                        Conductor.Instance.EnableLocalVideoStream();
                    }
                    else
                    {
                        Conductor.Instance.DisableLocalVideoStream();
                    }

                    if (IsMicrophoneEnabled)
                    {
                        Conductor.Instance.UnmuteMicrophone();
                    }
                    else
                    {
                        Conductor.Instance.MuteMicrophone();
                    }
                });
            }
            else
            {
                Debug.WriteLine("selfVideoTrack NULL");
            }
        }

        private void Conductor_OnPeerConnectionHealthStats(RTCPeerConnectionHealthStats stats)
        {
            PeerConnectionHealthStats = stats;
        }

        void NewIceServer_PropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == "Valid")
            {
                // TODO: AddIceServer
            }
        }
        #endregion

        #region PROPERTIES
        private ObservableCollection<Peer> _peers;
        public ObservableCollection<Peer> Peers
        {
            get { return _peers; }
            set { _peers = value; }
        }

        private bool _isConnected;
        public bool IsConnected
        {
            get { return _isConnected; }
            set
            {
                _isConnected = value;
                // TODO: Notify Connect Change / Disconnect Enable                
            }
        }

        private bool _isMicrophoneEnabled = false;
        public bool IsMicrophoneEnabled
        {
            get { return _isMicrophoneEnabled; }
            set { _isMicrophoneEnabled = value; }
        }

        private bool _isCameraEnabled = false;
        public bool IsCameraEnabled
        {
            get { return _isCameraEnabled; }
            set { _isCameraEnabled = value; }
        }

        private bool _isConnecting;
        public bool IsConnecting
        {
            get { return _isConnecting; }
            set
            {
                _isConnecting = value;
                // TODO: Notify Connect Status                
            }
        }

        private bool _isDisconnecting;
        public bool IsDisconnecting
        {
            get { return _isDisconnecting; }
            set
            {
                _isDisconnecting = value;
                // TODO: Notify Disconnect                
            }
        }

        private RTCPeerConnectionHealthStats _peerConnectionHealthStats;
        public RTCPeerConnectionHealthStats PeerConnectionHealthStats
        {
            get { return _peerConnectionHealthStats; }
            set
            {
                if (_peerConnectionHealthStats != value)
                {
                    _peerConnectionHealthStats = value;                   
                    UpdatePeerConnHealthStatsVisibilityHelper();                    
                }
            }
        }

        public void UpdatePeerConnHealthStatsVisibilityHelper()
        {
            if (IsConnectedToPeer && PeerConnectionHealthStatsEnabled && PeerConnectionHealthStats != null)
            {
                ShowPeerConnectionHealthStats = true;
            }
            else
            {
                ShowPeerConnectionHealthStats = false;
            }
        }

        private bool _isConnectedToPeer;
        public bool IsConnectedToPeer
        {
            get { return _isConnectedToPeer; }
            set
            {
                _isConnectedToPeer = value;

                // TODO: Notify ConnecToPeer, DisconnectFromPeer
                
                PeerConnectionHealthStats = null;
                UpdatePeerConnHealthStatsVisibilityHelper();
                UpdateLoopbackVideoVisibilityHelper();
            }
        }

        public void UpdateLoopbackVideoVisibilityHelper()
        {
            if (IsConnectedToPeer && VideoLoopbackEnabled)
            {
                ShowLoopbackVideo = true;
            }
            else
            {
                ShowLoopbackVideo = false;
            }
        }

        private bool _videoLoopbackEnabled = false;
        public bool VideoLoopbackEnabled
        {
            get { return _videoLoopbackEnabled; }
            set
            {
                if (_videoLoopbackEnabled != value)
                {
                    _videoLoopbackEnabled = value;
                    if (value)
                    {
                        if (_selfVideoTrack != null)
                        {                            
                            OnStatusMessageUpdate?.Invoke("Enabling video loopback");
                            var source = Media.CreateMedia().CreateMediaSource(_selfVideoTrack, "SELF");
                            RunOnUiThread(() =>
                            {
                                // TODO: Setup Local Video Display
                            });
                        }
                    }
                    else
                    {
                        // This is a hack/workaround for destroying the internal stream source (RTMediaStreamSource)
                        // instance inside webrtc winrt api when loopback is disabled.
                        // For some reason, the RTMediaStreamSource instance is not destroyed when only SelfVideo.Source
                        // is set to null.
                        // For unknown reasons, when executing the above sequence (set to null, stop, set to null), the
                        // internal stream source is destroyed.
                        // Apparently, with webrtc package version < 1.1.175, the internal stream source was destroyed
                        // corectly, only by setting SelfVideo.Source to null.
                        //                        SelfVideo.Source = null;
                        //                        SelfVideo.Stop();
                        //                        SelfVideo.Source = null;
                        GC.Collect(); // Ensure all references are truly dropped.
                    }
                }

                UpdateLoopbackVideoVisibilityHelper();
            }
        }

        private bool _peerConnectioneHealthStatsEnabled;
        public bool PeerConnectionHealthStatsEnabled
        {
            get { return _peerConnectioneHealthStatsEnabled; }
            set
            {
                if (_peerConnectioneHealthStatsEnabled != value)
                {
                    _peerConnectioneHealthStatsEnabled = value;
                    Conductor.Instance.PeerConnectionStatsEnabled = value;
                    UpdatePeerConnHealthStatsVisibilityHelper();
                }
            }
        }

        private bool _showLoopbackVideo = false;
        public bool ShowLoopbackVideo
        {
            get { return _showLoopbackVideo; }
            set { _showLoopbackVideo = value; }
        }

        private bool _showPeerConnectionHealthStats;
        public bool ShowPeerConnectionHealthStats
        {
            get { return _showPeerConnectionHealthStats; }
            set { _showPeerConnectionHealthStats = value; }
        }

        private bool _isReadyToConnect;
        public bool IsReadyToConnect
        {
            get { return _isReadyToConnect; }
            set
            {
                _isReadyToConnect = value;
                // TODO: Notify ConnectToPeer / DisconnectFromPeer                
            }
        }

        private bool _isReadyToDisconnect;
        public bool IsReadyToDisconnect
        {
            get { return _isReadyToDisconnect; }
            set
            {
                _isReadyToDisconnect = value;
                // TODO: Notify DisconnectFromPeer                
            }
        }

        private ObservableCollection<IceServer> _iceServers;
        public ObservableCollection<IceServer> IceServers
        {
            get { return _iceServers; }
            set { _iceServers = value; }
        }

        private IceServer _newIceServer;
        public IceServer NewIceServer
        {
            get { return _newIceServer; }
            set
            {
                if (_newIceServer != value)
                {
                    _newIceServer = value;
                    // TODO: Validate Entry                    
                }
            }
        }

        private ObservableCollection<CodecInfo> _audioCodecs;
        public ObservableCollection<CodecInfo> AudioCodecs
        {
            get { return _audioCodecs; }
            set { _audioCodecs = value; }
        }

        private ObservableCollection<CodecInfo> _videoCodecs;
        public ObservableCollection<CodecInfo> VideoCodecs
        {
            get { return _videoCodecs; }
            set { _videoCodecs = value; }
        }

        public CodecInfo SelectedAudioCodec
        {
            get { return Conductor.Instance.AudioCodec; }
            set
            {
                if (Conductor.Instance.AudioCodec == value)
                {
                    return;
                }

                Conductor.Instance.AudioCodec = value;
                // TODO: Notify SelectedAudioCodec
            }
        }

        public CodecInfo SelectedVideoCodec
        {
            get { return Conductor.Instance.VideoCodec; }
            set
            {
                if (Conductor.Instance.VideoCodec == value)
                {
                    return;
                }

                Conductor.Instance.VideoCodec = value;
                // TODO: Notify SelectedVideoCodec                
            }
        }

        private ValidableNonEmptyString _ip;
        public ValidableNonEmptyString Ip
        {
            get { return _ip; }
            set
            {
                _ip = value;            
            }
        }

        private string _server;
        public string Server
        {
            get { return _server; }
            set
            {
                _server = value;            
            }
        }

        private ValidableIntegerString _port;
        public ValidableIntegerString Port
        {
            get { return _port; }
            set
            {
                _port = value;            
            }
        }

        private ValidableIntegerString _heartbeat;
        public ValidableIntegerString HeartBeat
        {
            get { return _heartbeat; }
            set
            {
                _heartbeat = value;             
            }
        }

        private bool _hasServer;
        public bool HasServer
        {
            get { return _hasServer; }
            set { _hasServer = value; }
        }

        private ValidableNonEmptyString _ntpServer;
        public ValidableNonEmptyString NtpServer
        {
            get { return _ntpServer; }
            set
            {
                _ntpServer = value;
                // TODO: Notify NTP Server Status
                NtpSyncEnabled = false;
            }
        }

        private bool _ntpSyncEnabled;
        
        public bool NtpSyncEnabled
        {
            get { return _ntpSyncEnabled; }
            set
            {
                if (_ntpSyncEnabled == value)
                {
                    return;
                }

                _ntpSyncEnabled = value;

                if (_ntpSyncEnabled)
                {
                    NtpSyncInProgress = true;
                    _ntpService.GetNetworkTime(NtpServer.Value);
                }
                else
                {
                    if (NtpSyncInProgress)
                    {
                        NtpSyncInProgress = false;
                        _ntpService.AbortSync();
                    }
                }
            }
        }

        private Boolean _ntpSyncInProgress;
        public Boolean NtpSyncInProgress
        {
            get { return _ntpSyncInProgress; }
            set { _ntpSyncInProgress = value; }
        }

        private Peer _selectedPeer;
        public Peer SelectedPeer
        {
            get { return _selectedPeer; }
            set
            {
                _selectedPeer = value;
                // TODO: Notify ConnectToPeer                
            }
        }

        private ObservableCollection<MediaDevice> _cameras;
        public ObservableCollection<MediaDevice> Cameras
        {
            get { return _cameras; }
            set { _cameras =value; }
        }

        private ObservableCollection<MediaDevice> _microphones;
        public ObservableCollection<MediaDevice> Microphones
        {
            get { return _microphones; }
            set { _microphones = value; }
        }

        private ObservableCollection<MediaDevice> _audioPlayoutDevices;
        public ObservableCollection<MediaDevice> AudioPlayoutDevices
        {
            get { return _audioPlayoutDevices; }
            set { _audioPlayoutDevices = value; }
        }

        private MediaDevice _selectedCamera;
        public MediaDevice SelectedCamera
        {
            get { return _selectedCamera; }
            set
            {
                _selectedCamera = value;
                if (value == null)
                {
                    return;
                }

                Conductor.Instance.Media.SelectVideoDevice(_selectedCamera);
                if (_allCapRes == null)
                {
                    _allCapRes = new ObservableCollection<String>();
                }
                else
                {
                    _allCapRes.Clear();
                }

                var opRes = value.GetVideoCaptureCapabilities();
                opRes.AsTask().ContinueWith(resolutions =>
                {
                    RunOnUiThread(() =>
                    {
                        if (resolutions.IsFaulted)
                        {
                            Exception ex = resolutions.Exception;
                            while (ex is AggregateException && ex.InnerException != null)
                                ex = ex.InnerException;
                            String errorMsg = "SetSelectedCamera: Failed to GetVideoCaptureCapabilities (Error: " + ex.Message + ")";
                            OnStatusMessageUpdate?.Invoke(errorMsg);                            
                            return;
                        }

                        if (resolutions.Result == null)
                        {
                            String errorMsg = "SetSelectedCamera: Failed to GetVideoCaptureCapabilities (Result is null)";
                            OnStatusMessageUpdate?.Invoke(errorMsg);                            
                            return;
                        }

                        var uniqueRes = resolutions.Result.GroupBy(test => test.ResolutionDescription).Select(grp => grp.First()).ToList();
                        CaptureCapability defaultResolution = null;
                        foreach (var resolution in uniqueRes)
                        {
                            if (defaultResolution == null)
                            {
                                defaultResolution = resolution;
                            }
                            _allCapRes.Add(resolution.ResolutionDescription);
                            if ((resolution.Width == 640) && (resolution.Height == 480))
                            {
                                defaultResolution = resolution;
                            }
                        }
                        
                        string selectedCapResItem = string.Empty;
                        if (!string.IsNullOrEmpty(selectedCapResItem) && _allCapRes.Contains(selectedCapResItem))
                        {
                            SelectedCapResItem = selectedCapResItem;
                        }
                        else
                        {
                            SelectedCapResItem = defaultResolution?.ResolutionDescription;
                        }
                    });                    
                });
            }
        }

        private ObservableCollection<String> _allCapRes;
        public ObservableCollection<String> AllCapRes
        {
            get
            {
                if (_allCapRes != null)
                    return _allCapRes;
                else
                    return _allCapRes = new ObservableCollection<String>();
            }
            set
            {
                _allCapRes = value;
            }
        }

        private string _selectedCapResItem = null;
        public string SelectedCapResItem
        {
            get { return _selectedCapResItem; }
            set
            {
                if (AllCapFps == null)
                {
                    AllCapFps = new ObservableCollection<CaptureCapability>();
                }
                else
                {
                    AllCapFps.Clear();
                }

                if (SelectedCamera != null)
                {
                    var opCap = SelectedCamera.GetVideoCaptureCapabilities();
                    opCap.AsTask().ContinueWith(caps =>
                    {
                        var fpsList = from cap in caps.Result where cap.ResolutionDescription == value select cap;
                        var t = CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal,
                            () =>
                            {
                                CaptureCapability defaultFps = null;
                                uint selectedCapFpsFrameRate = 0;

                                foreach (var fps in fpsList)
                                {
                                    if (selectedCapFpsFrameRate != 0 && fps.FrameRate == selectedCapFpsFrameRate)
                                    {
                                        defaultFps = fps;
                                    }

                                    AllCapFps.Add(fps);
                                    if (defaultFps == null)
                                    {
                                        defaultFps = fps;
                                    }
                                }

                                SelectedCapFpsItem = defaultFps;
                            });                        
                    });

                    _selectedCapResItem = value;
                }
            }
        }

        private ObservableCollection<CaptureCapability> _allCapFps;
        public ObservableCollection<CaptureCapability> AllCapFps
        {
            get { return _allCapFps ?? (_allCapFps = new ObservableCollection<CaptureCapability>()); }
            set { _allCapFps = value; }
        }

        private CaptureCapability _selectedCapFpsItem;
        public CaptureCapability SelectedCapFpsItem
        {
            get { return _selectedCapFpsItem; }
            set
            {
                if(_selectedCapFpsItem != value)
                {
                    _selectedCapFpsItem = value;
                    Conductor.Instance.VideoCaptureProfile = value;
                    Conductor.Instance.UpdatePreferredFrameFormat();
                }
            }
        }

        private MediaDevice _selectedMicrophone;
        public MediaDevice SelectedMicrophone
        {
            get { return _selectedMicrophone; }
            set
            {
                if (_selectedMicrophone != value)
                {
                    _selectedMicrophone = value;
                    if (value != null)
                    {
                        Conductor.Instance.Media.SelectAudioCaptureDevice(_selectedMicrophone);
                    }
                }
            }
        }

        private MediaDevice _selectedAudioPlayoutDevice;
        public MediaDevice SelectedAudioPlayoutDevice
        {
            get { return _selectedAudioPlayoutDevice; }
            set
            {
                if (_selectedAudioPlayoutDevice != value)
                {
                    _selectedAudioPlayoutDevice = value;
                    if (value != null)
                    {
                        Conductor.Instance.Media.SelectAudioPlayoutDevice(_selectedAudioPlayoutDevice);
                    }
                }
            }
        }

        #endregion

        #region UTILITY METHODS
        private void ReevaluateHasServer()
        {
            HasServer = Ip != null && Ip.Valid && Port != null && Port.Valid;
        }

        protected void RunOnUiThread(Action fn)
        {
            var asyncOp = _uiDispatcher.RunAsync(CoreDispatcherPriority.Normal, new DispatchedHandler(fn));
        }
        #endregion
    }
}
