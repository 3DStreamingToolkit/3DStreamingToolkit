using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.ApplicationModel.Core;
using Windows.Data.Json;
using Windows.Media.Core;
using Windows.Storage;
using Windows.UI.Core;
using Windows.UI.Popups;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Org.WebRtc;
using PeerConnectionClient.Model;
using PeerConnectionClient.Signalling;
using PeerConnectionClient.MVVM;
using PeerConnectionClient.Utilities;

namespace WebRtcWrapper
{
    public class WebRtcUtils : DispatcherBindableBase
    {
        public delegate void OnRawVideoFrameSendDelegate(uint w, uint h, byte[] yPlane, uint yPitch, byte[] vPlane, uint vPitch, byte[] uPlane, uint uPitch);

        public event Action OnInitialized;
        public event Action<int, string> OnPeerMessageDataReceived;
        public event Action<string> OnStatusMessageUpdate;
        
        //public event Action<uint, uint, byte[], uint, byte[], uint, byte[], uint> OnRawVideoFrameSend;
        public OnRawVideoFrameSendDelegate OnRawVideoFrameSend;
        public event Action<uint, uint, byte[]> OnEncodedFrameReceived;        

        public MediaElement SelfVideo = null;
        public MediaElement PeerVideo = null;        
        public RawVideoSource rawVideo;
        public EncodedVideoSource encodedVideoSource;        

        // Message Data Type
        private static readonly string kMessageDataType = "message";

        private MediaVideoTrack _peerVideoTrack;
        private MediaVideoTrack _selfVideoTrack;
        private readonly NtpService _ntpService;
        private readonly CoreDispatcher _uiDispatcher;

        public WebRtcUtils() : base(CoreApplication.MainView.CoreWindow.Dispatcher)
        {
            _uiDispatcher = CoreApplication.MainView.CoreWindow.Dispatcher;            
        }
        
        public void Initialize()
        {
            WebRTC.Initialize(_uiDispatcher);
            Conductor.Instance.ETWStatsEnabled = false;

            ConnectCommand = new ActionCommand(ConnectCommandExecute, ConnectCommandCanExecute);
            ConnectToPeerCommand = new ActionCommand(ConnectToPeerCommandExecute, ConnectToPeerCommandCanExecute);
            DisconnectFromPeerCommand = new ActionCommand(DisconnectFromPeerCommandExecute, DisconnectFromPeerCommandCanExecute);
            DisconnectFromServerCommand = new ActionCommand(DisconnectFromServerExecute, DisconnectFromServerCanExecute);
            AddIceServerCommand = new ActionCommand(AddIceServerExecute, AddIceServerCanExecute);
            RemoveSelectedIceServerCommand = new ActionCommand(RemoveSelectedIceServerExecute, RemoveSelectedIceServerCanExecute);
            SendPeerMessageDataCommand = new ActionCommand(SendPeerMessageDataExecute, SendPeerMessageDataCanExecute);            

            Cameras = new ObservableCollection<MediaDevice>();
            Microphones = new ObservableCollection<MediaDevice>();
            AudioPlayoutDevices = new ObservableCollection<MediaDevice>();

            foreach (MediaDevice videoCaptureDevice in Conductor.Instance.Media.GetVideoCaptureDevices())
            {
                Cameras.Add(videoCaptureDevice);
            }
            foreach (MediaDevice audioCaptureDevice in Conductor.Instance.Media.GetAudioCaptureDevices())
            {
                Microphones.Add(audioCaptureDevice);
            }
            foreach (MediaDevice audioPlayoutDevice in Conductor.Instance.Media.GetAudioPlayoutDevices())
            {
                AudioPlayoutDevices.Add(audioPlayoutDevice);
            }
            if (SelectedCamera == null && Cameras.Count > 0)
            {
                SelectedCamera = Cameras.First();
            }
            if (SelectedMicrophone == null && Microphones.Count > 0)
            {
                SelectedMicrophone = Microphones.First();
            }
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
                    Peers.Add(SelectedPeer = new Peer { Id = peerId, Name = peerName });
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
                    IsMicrophoneEnabled = true;
                    IsCameraEnabled = true;
                    IsConnecting = false;

                    OnStatusMessageUpdate?.Invoke("Signed-In");
                });
            };

            Conductor.Instance.Signaller.OnServerConnectionFailure += () =>
            {
                RunOnUiThread(async () =>
                {
                    IsConnecting = false;
                    MessageDialog msgDialog = new MessageDialog("Failed to connect to server!");
                    await msgDialog.ShowAsync();
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
                    // TODO: Handle All Peer Messages
                });
            };

            // TODO: Restore Event Handler in Utility Wrapper
            // Implemented in Unity Consumer due to Event Handling Issue
//            Conductor.Instance.OnAddRemoteStream += Conductor_OnAddRemoteStream;

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
                    PeerVideo.Source = null;
                    SelfVideo.Source = null;
                    _peerVideoTrack = null;
                    _selfVideoTrack = null;
                    GC.Collect(); // Ensure all references are truly dropped.
                    IsMicrophoneEnabled = false;
                    IsCameraEnabled = true;
                    OnStatusMessageUpdate?.Invoke("Peer Connection Closed");
                });
            };

            Conductor.Instance.OnPeerMessageDataReceived += (peerId, message) =>
            {
                OnPeerMessageDataReceived?.Invoke(peerId, message);
            };

            Conductor.Instance.OnReadyToConnect += () => { RunOnUiThread(() => { IsReadyToConnect = true; }); };

            IceServers = new ObservableCollection<IceServer>();
            NewIceServer = new IceServer();

            AudioCodecs = new ObservableCollection<CodecInfo>();
            var audioCodecList = WebRTC.GetAudioCodecs();

            string[] incompatibleAudioCodecs = new string[] { "CN32000", "CN16000", "CN8000", "red8000", "telephone-event8000" };
            VideoCodecs = new ObservableCollection<CodecInfo>();
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
                    SelectedVideoCodec = VideoCodecs.FirstOrDefault(x => x.Name.Contains("VP8"));
                }
            });

            LoadSettings();

            RunOnUiThread(() =>
            {
                OnInitialized?.Invoke();
            });
        }

        void LoadSettings()
        {
            // Default values:
            var configTraceServerIp = "127.0.0.1";
            var configTraceServerPort = "55000";

            var ntpServerAddress = new ValidableNonEmptyString("time.windows.com");
            var peerCcServerIp = new ValidableNonEmptyString("127.0.0.1");
            var peerCcPortInt = 8888;

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

            RunOnUiThread(() =>
            {
                IceServers = configIceServers;                
                Ip = peerCcServerIp;
                NtpServer = ntpServerAddress;
                Port = new ValidableIntegerString(peerCcPortInt, 0, 65535);
                ReevaluateHasServer();
            });

            Conductor.Instance.ConfigureIceServers(configIceServers);
        }

        #region ActionCommands

        private ActionCommand _connectCommand;
        public ActionCommand ConnectCommand
        {
            get { return _connectCommand; }
            set { SetProperty(ref _connectCommand, value); }
        }

        private ActionCommand _connectToPeerCommand;
        public ActionCommand ConnectToPeerCommand
        {
            get { return _connectToPeerCommand; }
            set { SetProperty(ref _connectToPeerCommand, value); }
        }

        private ActionCommand _disconnectFromPeerCommand;
        public ActionCommand DisconnectFromPeerCommand
        {
            get { return _disconnectFromPeerCommand; }
            set { SetProperty(ref _disconnectFromPeerCommand, value); }
        }

        private ActionCommand _disconnectFromServerCommand;
        public ActionCommand DisconnectFromServerCommand
        {
            get { return _disconnectFromServerCommand; }
            set { SetProperty(ref _disconnectFromServerCommand, value); }
        }

        private ActionCommand _addIceServerCommand;
        public ActionCommand AddIceServerCommand
        {
            get { return _addIceServerCommand; }
            set { SetProperty(ref _addIceServerCommand, value); }
        }

        private ActionCommand _removeSelectedIceServerCommand;
        public ActionCommand RemoveSelectedIceServerCommand
        {
            get { return _removeSelectedIceServerCommand; }
            set { SetProperty(ref _removeSelectedIceServerCommand, value); }
        }

        private ActionCommand _settingsButtonCommand;
        public ActionCommand SettingsButtonCommand
        {
            get { return _settingsButtonCommand; }
            set { SetProperty(ref _settingsButtonCommand, value); }
        }

        private ActionCommand _sendPeerMessageDataCommand;
        public ActionCommand SendPeerMessageDataCommand
        {
            get { return _settingsButtonCommand; }
            set { SetProperty(ref _settingsButtonCommand, value); }
        }
        #endregion

        #region Property Bindings
        private ValidableNonEmptyString _ntpServer;
        public ValidableNonEmptyString NtpServer
        {
            get { return _ntpServer; }
            set
            {
                SetProperty(ref _ntpServer, value);
                _ntpServer.PropertyChanged += NtpServer_PropertyChanged;
                NtpSyncEnabled = false;
            }
        }

        private ValidableNonEmptyString _ip;
        public ValidableNonEmptyString Ip
        {
            get { return _ip; }
            set
            {
                SetProperty(ref _ip, value);
                _ip.PropertyChanged += Ip_PropertyChanged;
            }
        }

        private ValidableIntegerString _port;
        public ValidableIntegerString Port
        {
            get { return _port; }
            set
            {
                SetProperty(ref _port, value);
                _port.PropertyChanged += Port_PropertyChanged;
            }
        }

        private ObservableCollection<Peer> _peers;
        public ObservableCollection<Peer> Peers
        {
            get { return _peers; }
            set { SetProperty(ref _peers, value); }
        }

        private Peer _selectedPeer;
        public Peer SelectedPeer
        {
            get { return _selectedPeer; }
            set
            {
                SetProperty(ref _selectedPeer, value);
                ConnectToPeerCommand.RaiseCanExecuteChanged();
            }
        }

        private String _peerWidth;
        public String PeerWidth
        {
            get { return _peerWidth; }
            set { SetProperty(ref _peerWidth, value); }
        }

        private String _peerHeight;
        public String PeerHeight
        {
            get { return _peerHeight; }
            set { SetProperty(ref _peerHeight, value); }
        }

        private String _selfWidth;
        public String SelfWidth
        {
            get { return _selfWidth; }
            set { SetProperty(ref _selfWidth, value); }
        }

        private String _selfHeight;
        public String SelfHeight
        {
            get { return _selfHeight; }
            set { SetProperty(ref _selfHeight, value); }
        }

        private String _peerVideoFps;
        public String PeerVideoFps
        {
            get { return _peerVideoFps; }
            set { SetProperty(ref _peerVideoFps, value); }
        }

        private String _selfVideoFps;
        public String SelfVideoFps
        {
            get { return _selfVideoFps; }
            set { SetProperty(ref _selfVideoFps, value); }
        }

        private bool _hasServer;
        public bool HasServer
        {
            get { return _hasServer; }
            set { SetProperty(ref _hasServer, value); }
        }

        private bool _isConnected;
        public bool IsConnected
        {
            get { return _isConnected; }
            set
            {
                SetProperty(ref _isConnected, value);
                ConnectCommand.RaiseCanExecuteChanged();
                DisconnectFromServerCommand.RaiseCanExecuteChanged();
            }
        }

        private bool _isConnecting;
        public bool IsConnecting
        {
            get { return _isConnecting; }
            set
            {
                SetProperty(ref _isConnecting, value);
                ConnectCommand.RaiseCanExecuteChanged();
            }
        }

        private bool _isDisconnecting;
        public bool IsDisconnecting
        {
            get { return _isDisconnecting; }
            set
            {
                SetProperty(ref _isDisconnecting, value);
                DisconnectFromServerCommand.RaiseCanExecuteChanged();
            }
        }

        private bool _isConnectedToPeer;
        public bool IsConnectedToPeer
        {
            get { return _isConnectedToPeer; }
            set
            {
                SetProperty(ref _isConnectedToPeer, value);
                ConnectToPeerCommand.RaiseCanExecuteChanged();
                DisconnectFromPeerCommand.RaiseCanExecuteChanged();

                PeerConnectionHealthStats = null;
                UpdatePeerConnHealthStatsVisibilityHelper();
                UpdateLoopbackVideoVisibilityHelper();
            }
        }

        private bool _isReadyToConnect;
        public bool IsReadyToConnect
        {
            get { return _isReadyToConnect; }
            set
            {
                SetProperty(ref _isReadyToConnect, value);
                ConnectToPeerCommand.RaiseCanExecuteChanged();
                DisconnectFromPeerCommand.RaiseCanExecuteChanged();
            }
        }

        private bool _isReadyToDisconnect;
        public bool IsReadyToDisconnect
        {
            get { return _isReadyToDisconnect; }
            set
            {
                SetProperty(ref _isReadyToDisconnect, value);
                DisconnectFromPeerCommand.RaiseCanExecuteChanged();
            }
        }

        private bool _cameraEnabled = true;
        public bool CameraEnabled
        {
            get { return _cameraEnabled; }
            set
            {
                if (!SetProperty(ref _cameraEnabled, value))
                {
                    return;
                }

                if (IsConnectedToPeer)
                {
                    if (_cameraEnabled)
                    {
                        Conductor.Instance.EnableLocalVideoStream();
                    }
                    else
                    {
                        Conductor.Instance.DisableLocalVideoStream();
                    }
                }
            }
        }

        private bool _microphoneIsOn = true;
        public bool MicrophoneIsOn
        {
            get { return _microphoneIsOn; }
            set
            {
                if (!SetProperty(ref _microphoneIsOn, value))
                {
                    return;
                }

                if (IsConnectedToPeer)
                {
                    if (_microphoneIsOn)
                    {
                        Conductor.Instance.UnmuteMicrophone();
                    }
                    else
                    {
                        Conductor.Instance.MuteMicrophone();
                    }
                }
            }
        }

        private bool _isMicrophoneEnabled = true;
        public bool IsMicrophoneEnabled
        {
            get { return _isMicrophoneEnabled; }
            set { SetProperty(ref _isMicrophoneEnabled, value); }
        }

        private bool _isCameraEnabled = true;
        public bool IsCameraEnabled
        {
            get { return _isCameraEnabled; }
            set { SetProperty(ref _isCameraEnabled, value); }
        }

        private ObservableCollection<MediaDevice> _cameras;
        public ObservableCollection<MediaDevice> Cameras
        {
            get { return _cameras; }
            set { SetProperty(ref _cameras, value); }
        }

        private MediaDevice _selectedCamera;
        public MediaDevice SelectedCamera
        {
            get { return _selectedCamera; }
            set
            {
                SetProperty(ref _selectedCamera, value);

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
                    RunOnUiThread(async () =>
                    {
                        if (resolutions.IsFaulted)
                        {
                            Exception ex = resolutions.Exception;
                            while (ex is AggregateException && ex.InnerException != null)
                                ex = ex.InnerException;
                            String errorMsg = "SetSelectedCamera: Failed to GetVideoCaptureCapabilities (Error: " + ex.Message + ")";
                            Debug.WriteLine("[Error] " + errorMsg);
                            var msgDialog = new MessageDialog(errorMsg);
                            await msgDialog.ShowAsync();
                            return;
                        }
                        if (resolutions.Result == null)
                        {
                            String errorMsg = "SetSelectedCamera: Failed to GetVideoCaptureCapabilities (Result is null)";
                            Debug.WriteLine("[Error] " + errorMsg);
                            var msgDialog = new MessageDialog(errorMsg);
                            await msgDialog.ShowAsync();
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
                        var settings = ApplicationData.Current.LocalSettings;
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
                    OnPropertyChanged("AllCapRes");
                });
            }
        }

        private ObservableCollection<MediaDevice> _microphones;
        public ObservableCollection<MediaDevice> Microphones
        {
            get { return _microphones; }
            set { SetProperty(ref _microphones, value); }
        }

        private MediaDevice _selectedMicrophone;
        public MediaDevice SelectedMicrophone
        {
            get { return _selectedMicrophone; }
            set
            {
                if (SetProperty(ref _selectedMicrophone, value) && value != null)
                {
                    Conductor.Instance.Media.SelectAudioCaptureDevice(_selectedMicrophone);
                }
            }
        }

        private ObservableCollection<MediaDevice> _audioPlayoutDevices;
        public ObservableCollection<MediaDevice> AudioPlayoutDevices
        {
            get { return _audioPlayoutDevices; }
            set { SetProperty(ref _audioPlayoutDevices, value); }
        }

        private MediaDevice _selectedAudioPlayoutDevice;
        public MediaDevice SelectedAudioPlayoutDevice
        {
            get { return _selectedAudioPlayoutDevice; }
            set
            {
                if (SetProperty(ref _selectedAudioPlayoutDevice, value) && value != null)
                {
                    Conductor.Instance.Media.SelectAudioPlayoutDevice(_selectedAudioPlayoutDevice);
                }
            }
        }

        private bool _videoLoopbackEnabled = false;
        public bool VideoLoopbackEnabled
        {
            get { return _videoLoopbackEnabled; }
            set
            {
                if (SetProperty(ref _videoLoopbackEnabled, value))
                {
                    if (value)
                    {
                        if (_selfVideoTrack != null)
                        {
                            Debug.WriteLine("Enabling video loopback");

                            var source = Media.CreateMedia().CreateMediaSource(_selfVideoTrack, "SELF");
                            RunOnUiThread(() =>
                            {
                                if(SelfVideo != null)
                                {
                                    SelfVideo.SetMediaStreamSource(source);
                                    Debug.WriteLine("Video loopback enabled");
                                }
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
                        SelfVideo.Source = null;
                        SelfVideo.Stop();
                        SelfVideo.Source = null;
                        GC.Collect(); // Ensure all references are truly dropped.
                    }
                }
                UpdateLoopbackVideoVisibilityHelper();
            }
        }

        private ObservableCollection<IceServer> _iceServers;
        public ObservableCollection<IceServer> IceServers
        {
            get { return _iceServers; }
            set { SetProperty(ref _iceServers, value); }
        }

        private IceServer _selectedIceServer;
        public IceServer SelectedIceServer
        {
            get { return _selectedIceServer; }
            set
            {
                SetProperty(ref _selectedIceServer, value);
                RemoveSelectedIceServerCommand.RaiseCanExecuteChanged();
            }
        }

        private IceServer _newIceServer;
        public IceServer NewIceServer
        {
            get { return _newIceServer; }
            set
            {
                if (SetProperty(ref _newIceServer, value))
                {
                    _newIceServer.PropertyChanged += NewIceServer_PropertyChanged;
                }
            }
        }

        private ObservableCollection<CodecInfo> _audioCodecs;
        public ObservableCollection<CodecInfo> AudioCodecs
        {
            get { return _audioCodecs; }
            set { SetProperty(ref _audioCodecs, value); }
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
                OnPropertyChanged(() => SelectedAudioCodec);
            }
        }

        private ObservableCollection<String> _allCapRes;
        public ObservableCollection<String> AllCapRes
        {
            get { return _allCapRes ?? (_allCapRes = new ObservableCollection<String>()); }
            set { SetProperty(ref _allCapRes, value); }
        }

        private String _selectedCapResItem;
        public String SelectedCapResItem
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
                var opCap = SelectedCamera.GetVideoCaptureCapabilities();
                opCap.AsTask().ContinueWith(caps =>
                {
                    var fpsList = from cap in caps.Result where cap.ResolutionDescription == value select cap;
                    var t = CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
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
                    OnPropertyChanged("AllCapFps");
                });
                SetProperty(ref _selectedCapResItem, value);
            }
        }

        private ObservableCollection<CaptureCapability> _allCapFps;
        public ObservableCollection<CaptureCapability> AllCapFps
        {
            get { return _allCapFps ?? (_allCapFps = new ObservableCollection<CaptureCapability>()); }
            set { SetProperty(ref _allCapFps, value); }
        }

        private CaptureCapability _selectedCapFpsItem;
        public CaptureCapability SelectedCapFpsItem
        {
            get { return _selectedCapFpsItem; }
            set
            {
                if (SetProperty(ref _selectedCapFpsItem, value))
                {
                    Conductor.Instance.VideoCaptureProfile = value;
                    Conductor.Instance.UpdatePreferredFrameFormat();
                }
            }
        }

        private ObservableCollection<CodecInfo> _videoCodecs;
        public ObservableCollection<CodecInfo> VideoCodecs
        {
            get { return _videoCodecs; }
            set { SetProperty(ref _videoCodecs, value); }
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
                OnPropertyChanged(() => SelectedVideoCodec);
            }
        }

        private string _appVersion = "N/A";
        public string AppVersion
        {
            get { return _appVersion; }
            set { SetProperty(ref _appVersion, value); }
        }

        private RTCPeerConnectionHealthStats _peerConnectionHealthStats;
        public RTCPeerConnectionHealthStats PeerConnectionHealthStats
        {
            get { return _peerConnectionHealthStats; }
            set
            {
                if (SetProperty(ref _peerConnectionHealthStats, value))
                {
                    UpdatePeerConnHealthStatsVisibilityHelper();
                }
            }
        }

        private bool _peerConnectioneHealthStatsEnabled;
        public bool PeerConnectionHealthStatsEnabled
        {
            get { return _peerConnectioneHealthStatsEnabled; }
            set
            {
                if (SetProperty(ref _peerConnectioneHealthStatsEnabled, value))
                {
                    Conductor.Instance.PeerConnectionStatsEnabled = value;
                    UpdatePeerConnHealthStatsVisibilityHelper();
                }
            }
        }

        private bool _showPeerConnectionHealthStats;
        public bool ShowPeerConnectionHealthStats
        {
            get { return _showPeerConnectionHealthStats; }
            set { SetProperty(ref _showPeerConnectionHealthStats, value); }
        }

        private bool _showLoopbackVideo;
        public bool ShowLoopbackVideo
        {
            get { return _showLoopbackVideo; }
            set { SetProperty(ref _showLoopbackVideo, value); }
        }
        #endregion

        #region Action Handlers
        private bool SendPeerMessageDataCanExecute(object obj)
        {
            if (!IsConnectedToPeer)
            {
                return false;
            }

            return IsConnected;
        }

        public void SendPeerMessageDataExecute(object obj)
        {
            new Task(async () =>
            {
                string msg = (string) obj;
                JsonObject jsonMessage;

                if (!JsonObject.TryParse(msg, out jsonMessage))
                {
                    jsonMessage = new JsonObject()
                    {
                        {"data", JsonValue.CreateStringValue(msg) }
                    };
                }

                var jsonPackage = new JsonObject
                {
                    {"type", JsonValue.CreateStringValue(kMessageDataType)}
                };
                foreach (var o in jsonMessage)
                {
                    jsonPackage.Add(o.Key, o.Value);
                }
                await Conductor.Instance.Signaller.SendToPeer(SelectedPeer.Id, jsonPackage);
                
            }).Start();            
        }

        private bool ConnectCommandCanExecute(object obj)
        {
            return !IsConnected && !IsConnecting && Ip.Valid && Port.Valid;
        }

        public void ConnectCommandExecute(object obj)
        {
            new Task(() =>
            {
                IsConnecting = true;
                //Conductor.Instance.StartLogin(Ip.Value, Port.Value, peerName);
                //Conductor.Instance.StartLogin(host, port, peerName);
                Conductor.Instance.StartLogin(Ip.Value, Port.Value, (string)obj);
            }).Start();
        }

        public void ConnectToServer(string host, string port, string peerName)
        {
            new Task(() =>
            {
                IsConnecting = true;                
                Conductor.Instance.StartLogin(host, port, peerName);
            }).Start();
        }

        private bool ConnectToPeerCommandCanExecute(object obj)
        {
            return SelectedPeer != null && Peers.Contains(SelectedPeer) && !IsConnectedToPeer && IsReadyToConnect;
        }

        public void ConnectToPeerCommandExecute(object obj)
        {
            new Task(() => { Conductor.Instance.ConnectToPeer(SelectedPeer); }).Start();
        }

        private bool DisconnectFromPeerCommandCanExecute(object obj)
        {
            return IsConnectedToPeer && IsReadyToDisconnect;
        }

        public void DisconnectFromPeerCommandExecute(object obj)
        {
            new Task(() => { var task = Conductor.Instance.DisconnectFromPeer(); }).Start();
        }

        private bool DisconnectFromServerCanExecute(object obj)
        {
            if (IsDisconnecting)
            {
                return false;
            }

            return IsConnected;
        }

        public void DisconnectFromServerExecute(object obj)
        {
            new Task(() =>
            {
                IsDisconnecting = true;
                var task = Conductor.Instance.DisconnectFromServer();
            }).Start();

            Peers?.Clear();
        }
        private bool AddIceServerCanExecute(object obj)
        {
            return NewIceServer.Valid;
        }

        private void AddIceServerExecute(object obj)
        {
            IceServers.Add(_newIceServer);
            OnPropertyChanged(() => IceServers);
            Conductor.Instance.ConfigureIceServers(IceServers);
            SaveIceServerList();
            NewIceServer = new IceServer();
        }

        private bool RemoveSelectedIceServerCanExecute(object obj)
        {
            return SelectedIceServer != null;
        }

        private void RemoveSelectedIceServerExecute(object obj)
        {
            IceServers.Remove(_selectedIceServer);
            OnPropertyChanged(() => IceServers);
            SaveIceServerList();
            Conductor.Instance.ConfigureIceServers(IceServers);
        }

        void SaveIceServerList()
        {
        }

        void NewIceServer_PropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == "Valid")
            {
                AddIceServerCommand.RaiseCanExecuteChanged();
            }
        }

        void NtpServer_PropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == "Valid")
            {
                ConnectCommand.RaiseCanExecuteChanged();
            }
        }

        void Ip_PropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == "Valid")
            {
                ConnectCommand.RaiseCanExecuteChanged();
            }
            ReevaluateHasServer();
        }

        void Port_PropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == "Valid")
            {
                ConnectCommand.RaiseCanExecuteChanged();
            }
            ReevaluateHasServer();
        }

        private Boolean _ntpSyncInProgress;
        public Boolean NtpSyncInProgress
        {
            get { return _ntpSyncInProgress; }
            set { SetProperty(ref _ntpSyncInProgress, value); }
        }

        private bool _ntpSyncEnabled;

        /// <summary>
        /// Indicator and control of NTP syncronization.
        /// </summary>
        public bool NtpSyncEnabled
        {
            get { return _ntpSyncEnabled; }
            set
            {
                if (!SetProperty(ref _ntpSyncEnabled, value))
                {
                    return;
                }

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

        private void HandleNtpTimeSync(long ntpTime)
        {
            Debug.WriteLine("New NTP time: {0}", ntpTime);
            WebRTC.SynNTPTime(ntpTime);
            NtpSyncInProgress = false;
        }

        private void HandleNtpSynFailed()
        {
            NtpSyncInProgress = false;
            NtpSyncEnabled = false;
        }

        public async Task OnAppSuspending()
        {
            Conductor.Instance.CancelConnectingToPeer();

            if (IsConnectedToPeer)
            {
                await Conductor.Instance.DisconnectFromPeer();
            }
            if (IsConnected)
            {
                IsDisconnecting = true;
                await Conductor.Instance.DisconnectFromServer();
            }
            Media.OnAppSuspending();
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

        private DispatcherTimer _appPerfTimer;


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
            RunOnUiThread(() => {
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

                if (SelectedCamera == null)
                {
                    SelectedCamera = Cameras.FirstOrDefault();
                }
            });
        }

        private void RefreshAudioCaptureDevices(IList<MediaDevice> audioCaptureDevices)
        {
            RunOnUiThread(() => {
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
            RunOnUiThread(() => {
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


        private void Conductor_OnAddRemoteStream(MediaStreamEvent evt)
        {
            if (PeerVideo == null)
            {
                return;
            }

            _peerVideoTrack = evt.Stream.GetVideoTracks().FirstOrDefault();
            if (_peerVideoTrack != null)
            {

                // XAML MediaElement Setup
                //var source = Media.CreateMedia().CreateMediaSource(_peerVideoTrack, "PEER");
                //RunOnUiThread(() =>
                //{
                //    PeerVideo.SetMediaStreamSource(source);
                //});


                // Raw Video from VP8 Sender Setup
                rawVideo = Media.CreateMedia().CreateRawVideoSource(_peerVideoTrack);
                rawVideo.OnRawVideoFrame += Source_OnRawVideoFrame;

                //// Get H264 Encoded Frame
                //encodedVideoSource = Media.CreateMedia().CreateEncodedVideoSource(_peerVideoTrack);
                //encodedVideoSource.OnEncodedVideoFrame += Source_OnEncodedVideoFrame;
            }

            IsReadyToDisconnect = true;
        }

        private void Source_OnEncodedVideoFrame(uint width, uint height, byte[] frameData)
        {
            RunOnUiThread(() =>
            {
                // Pass the Event to Consumer
                OnEncodedFrameReceived?.Invoke(width, height, frameData);
            });
        }

        private void Source_OnRawVideoFrame(
            uint p0, 
            uint p1, 
            byte[] p2, 
            uint p3, 
            byte[] p4, 
            uint p5, 
            byte[] p6, 
            uint p7)
        {

            RunOnUiThread(() =>
            {
                //OnStatusMessageUpdate?.Invoke(string.Format(
                //    "{0}-{1}\n{2}\n{3}\n{4}\n{5}\n{6}\n{7}\n{8}",
                //    p0, // Width
                //    p1, // Height
                //    p2 != null ? p2.Length.ToString() : "null",     // yPlane
                //    p3,                                             // yPitch
                //    p4 != null ? p4.Length.ToString() : "null",     // vPlane
                //    p5,                                             // vPitch
                //    p6 != null ? p6.Length.ToString() : "null",     // uPlane
                //    p7,                                             // uPitch
                //    rawVideoCounter
                //));
                OnRawVideoFrameSend?.Invoke(p0, p1, p2, p3, p4, p5, p6, p7);
            });
        }

        private void Conductor_OnRemoveRemoteStream(MediaStreamEvent evt)
        {
            RunOnUiThread(() =>
            {
                PeerVideo.SetMediaStreamSource(null);
            });
        }

        private void Conductor_OnAddLocalStream(MediaStreamEvent evt)
        {
            _selfVideoTrack = evt.Stream.GetVideoTracks().FirstOrDefault();
            if (_selfVideoTrack != null)
            {
             
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
                    if ((VideoLoopbackEnabled) && (SelfVideo != null))
                    {
                        var source = Media.CreateMedia().CreateMediaSource(_selfVideoTrack, "SELF");
                        SelfVideo.SetMediaStreamSource(source);
                    }
                });
            }
        }

        private void Conductor_OnPeerConnectionHealthStats(RTCPeerConnectionHealthStats stats)
        {
            PeerConnectionHealthStats = stats;
        }

        #endregion

        #region Conductor Event Handlers
      
        #endregion

        #region Utility Methods
        private void ReevaluateHasServer()
        {
            HasServer = Ip != null && Ip.Valid && Port != null && Port.Valid;
        }
        #endregion
    }
}
