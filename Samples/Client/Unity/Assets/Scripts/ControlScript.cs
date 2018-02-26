using Microsoft.Toolkit.ThreeD;
using System;
using System.Linq;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.UI;

#if !UNITY_EDITOR
using Org.WebRtc;
using WebRtcWrapper;
using PeerConnectionClient.Signalling;
using Windows.Media.Core;
#endif

public class ControlScript : MonoBehaviour
{
    public uint TextureWidth = 2560;
    public uint TextureHeight = 720;
    public uint FrameRate = 30;

    // Heartbeat interval in ms (-1 will disable)
    public int HeartbeatInputText = 5000;
    public Text StatusText;
    public Text MessageText;
    public InputField ServerInputTextField;
    public InputField PeerInputTextField;
    public InputField MessageInputField;
    public Transform VirtualCamera;
    
    public RawImage LeftCanvas;
    public RawImage RightCanvas;

    public Camera LeftCamera;
    public Camera RightCamera;

	public HolographicStatus HoloStatus;

#if !UNITY_EDITOR
    private Matrix4x4 leftProjection;
    private Matrix4x4 leftView;
    private Matrix4x4 rightProjection;
    private Matrix4x4 rightView;
    private string cameraTransformMsg;
    private WebRtcControl _webRtcControl;
    
    private bool enabledStereo = false;
    private long timestamp = 0;
#endif

    #region Graphics Low-Level Plugin DLL Setup
#if !UNITY_EDITOR
    private MediaVideoTrack _peerVideoTrack;
#endif
    #endregion

    void Awake()
    {
    }
    
    void Start()
    {
#if !UNITY_EDITOR
        _webRtcControl = new WebRtcControl();
        _webRtcControl.OnInitialized += WebRtcControlOnInitialized;
        _webRtcControl.OnPeerMessageDataReceived += WebRtcControlOnPeerMessageDataReceived;
        _webRtcControl.OnStatusMessageUpdate += WebRtcControlOnStatusMessageUpdate;

        Conductor.Instance.OnAddRemoteStream += Conductor_OnAddRemoteStream;

		_webRtcControl.Initialize();
#endif
	}

#if !UNITY_EDITOR
    private void Conductor_OnAddRemoteStream(MediaStreamEvent evt)
    {
        System.Diagnostics.Debug.WriteLine("Conductor_OnAddRemoteStream()");
        _peerVideoTrack = evt.Stream.GetVideoTracks().FirstOrDefault();
        if (_peerVideoTrack != null)
        {
            System.Diagnostics.Debug.WriteLine(
                "Conductor_OnAddRemoteStream() - GetVideoTracks: {0}",
                evt.Stream.GetVideoTracks().Count);
        }
        else
        {
            System.Diagnostics.Debug.WriteLine("Conductor_OnAddRemoteStream() - peerVideoTrack NULL");
        }
        _webRtcControl.IsReadyToDisconnect = true;
    }
#endif

	private void WebRtcControlOnInitialized()
    {
        EnqueueAction(OnInitialized);
		
		// note: this will start the work of connecting to a server
		// note: this work MAY include authentication
		ConnectToServer();
    }

    private void OnInitialized()
    {
#if !UNITY_EDITOR
        _webRtcControl.SelectedVideoCodec = _webRtcControl.VideoCodecs.FirstOrDefault(x => x.Name.Contains("H264"));
#endif
        StatusText.text += "WebRTC Initialized\n";
    }

    private void WebRtcControlOnPeerMessageDataReceived(int peerId, string message)
    {
        EnqueueAction(() => UpdateMessageText(string.Format("{0}-{1}", peerId, message)));
    }
    
    private void WebRtcControlOnStatusMessageUpdate(string msg)
    {
        EnqueueAction(() => UpdateStatusText(string.Format("{0}\n", msg)));
    }

    private void UpdateMessageText(string msg)
    {
        MessageText.text += msg;
    }

    private void UpdateStatusText(string msg)
    {
		if (HoloStatus)
		{
			HoloStatus.ShowStatus(msg);
		}

        StatusText.text += msg;
		Debug.Log(msg);
    }

    public void ConnectToServer()
    {
#if !UNITY_EDITOR
        _webRtcControl.ConnectToServer(ServerInputTextField.text, PeerInputTextField.text, HeartbeatInputText);
#endif
	}

	public void DisconnectFromServer()
    {
#if !UNITY_EDITOR
        _webRtcControl.DisconnectFromServer();
#endif
    }

    public void ConnectToPeer()
    {    
#if !UNITY_EDITOR
        if (_webRtcControl.Peers.Count > 0)
        {
            _webRtcControl.SelectedPeer = _webRtcControl.Peers[0];
            _webRtcControl.ConnectToPeer();
        }
#endif
    }

    public void DisconnectFromPeer()
    {
#if !UNITY_EDITOR
        _webRtcControl.DisconnectFromPeer();
#endif
    }

    public void SendMessageToPeer()
    {
#if !UNITY_EDITOR
        _webRtcControl.SendPeerMessageData(MessageInputField.text);
#endif
    }

    public void ClearStatusText()
    {
        StatusText.text = string.Empty;
    }

    public void ClearMessageText()
    {
        MessageText.text = string.Empty;
    }

    public void EnqueueAction(Action action)
    {
		UIThreadSingleton.Dispatch(action);
    }

    private void OnEnable()
    {
        Plugin.CreateMediaPlayback();
        GetPlaybackTextureFromPlugin();
    }

    private void OnDisable()
    {
        Plugin.ReleaseMediaPlayback();
    }

    void Update()
    {
#if !UNITY_EDITOR
        leftProjection = LeftCamera.projectionMatrix;
        leftView = LeftCamera.worldToCameraMatrix;
        rightProjection = RightCamera.projectionMatrix;
        rightView = RightCamera.worldToCameraMatrix;

        // Builds the camera transform message.
        var leftProjectionCameraTransform = "";
        var leftViewCameraTransform = "";
        var rightProjectionCameraTransform = "";
        var rightViewCameraTransform = "";
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                leftProjectionCameraTransform += leftProjection[i, j] + ",";
                leftViewCameraTransform += leftView[i, j] + ",";
                rightProjectionCameraTransform += rightProjection[i, j] + ",";
                rightViewCameraTransform += rightView[i, j] + ",";
            }
        }

        var cameraTransformBody = leftProjectionCameraTransform + leftViewCameraTransform + rightProjectionCameraTransform + rightViewCameraTransform;

        // Adds dummy prediction timestamp.
        cameraTransformBody += timestamp++;

        cameraTransformBody =
           "{" +
           "  \"type\":\"camera-transform-stereo-prediction\"," +
           "  \"body\":\"" + cameraTransformBody + "\"" +
           "}";

        _webRtcControl.SendPeerDataChannelMessage(cameraTransformBody);
        
        if (!enabledStereo && _peerVideoTrack != null)
        {
            // Enables the stereo output mode.
            var msg = "{" +
            "  \"type\":\"stereo-rendering\"," +
            "  \"body\":\"1\"" +
            "}";

            if (_webRtcControl.SendPeerDataChannelMessage(msg))
            {
                // Start the stream when the server is in stero mode to avoid corrupt frames at startup.
                var source = Media.CreateMedia().CreateMediaStreamSource(
                    _peerVideoTrack,
                    "media",
                    TextureWidth,
                    TextureHeight);

                Plugin.LoadMediaStreamSource((MediaStreamSource)source);
                Plugin.Play();
                enabledStereo = true;
            }
        }
#endif
    }

    private void GetPlaybackTextureFromPlugin()
    {
        IntPtr nativeTex = IntPtr.Zero;
        Plugin.GetPrimaryTexture(TextureWidth, TextureHeight, out nativeTex);
        var primaryPlaybackTexture = Texture2D.CreateExternalTexture((int)TextureWidth, (int)TextureHeight, TextureFormat.BGRA32, false, false, nativeTex);

        LeftCanvas.texture = primaryPlaybackTexture;
        RightCanvas.texture = primaryPlaybackTexture;
    }

	private static class Plugin
    {
        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "CreateMediaPlayback")]
        internal static extern void CreateMediaPlayback();

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "ReleaseMediaPlayback")]
        internal static extern void ReleaseMediaPlayback();

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetPrimaryTexture")]
        internal static extern void GetPrimaryTexture(UInt32 width, UInt32 height, out System.IntPtr playbackTexture);

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "LoadContent")]
        internal static extern void LoadContent([MarshalAs(UnmanagedType.BStr)] string sourceURL);
#if !UNITY_EDITOR

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "LoadMediaSource")]
        internal static extern void LoadMediaSource(IMediaSource IMediaSourceHandler);

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "LoadMediaStreamSource")]
        internal static extern void LoadMediaStreamSource(MediaStreamSource IMediaSourceHandler);
#endif
    
        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "Play")]
        internal static extern void Play();

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "Pause")]
        internal static extern void Pause();

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "Stop")]
        internal static extern void Stop();
	}
}
