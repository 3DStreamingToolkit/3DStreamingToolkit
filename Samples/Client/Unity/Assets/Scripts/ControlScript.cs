using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Linq;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.UI;

#if !UNITY_EDITOR
using Org.WebRtc;
using WebRtcWrapper;
using PeerConnectionClient.Signalling;
using Windows.Media.Playback;
using Windows.Media.Core;
#endif

public class ControlScript : MonoBehaviour
{
    private const int textureWidth = 2560;
    private const int textureHeight = 720;
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
    public float TextureScale = 1f;
    public int PluginMode = 0;

    private Transform camTransform;
    private Vector3 prevPos;
    private Quaternion prevRot;

    private int frameCounter = 0;
    private int fpsCounter = 0;
    private float fpsCount = 0f;
    private float startTime = 0;
    private float endTime = 0;
    private bool enabledStereo = false;

    private string toyStoryH264 = @"http://www.html5videoplayer.net/videos/toystory.mp4";


#if !UNITY_EDITOR
    private WebRtcControl _webRtcControl;
    private static readonly ConcurrentQueue<Action> _executionQueue = new ConcurrentQueue<Action>();
#else
    private static readonly Queue<Action> _executionQueue = new Queue<Action>();
#endif
    private bool frame_ready_receive = true;
    private string messageText;

    #region Graphics Low-Level Plugin DLL Setup
#if !UNITY_EDITOR
    private MediaVideoTrack _peerVideoTrack;
#endif
    #endregion

    void Awake()
    {
        // Azure Host Details
        // ServerInputTextField.text = "signalingserver.centralus.cloudapp.azure.com:3000";
        
        // Local Dev Setup - Define Host Workstation IP Address
        ServerInputTextField.text = "10.0.0.88:8888";
    }

#if UNITY_EDITOR
    void Start()
#else
    IEnumerator Start()
#endif
    {
        camTransform = Camera.main.transform;
        prevPos = camTransform.position;
        prevRot = camTransform.rotation;

#if !UNITY_EDITOR
        _webRtcControl = new WebRtcControl();
        _webRtcControl.OnInitialized += WebRtcControlOnInitialized;
        _webRtcControl.OnPeerMessageDataReceived += WebRtcControlOnPeerMessageDataReceived;
        _webRtcControl.OnStatusMessageUpdate += WebRtcControlOnStatusMessageUpdate;

        Conductor.Instance.OnAddRemoteStream += Conductor_OnAddRemoteStream;
        _webRtcControl.Initialize();

        yield return StartCoroutine("CallPluginAtEndOfFrames");
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

            var media = Media.CreateMedia().CreateMediaStreamSource(_peerVideoTrack, 30, "media");
            
           Plugin.LoadMediaStreamSource((MediaStreamSource)media);
           Plugin.Play();

           // Wait one second after connection to set the stereo mode for the server.
           EnqueueAction(() => { endTime = (startTime = Time.time) + 1.0f; });
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

        ConnectToServer();
    }

    private void OnInitialized()
    {
#if !UNITY_EDITOR
        // _webRtcUtils.SelectedVideoCodec = _webRtcUtils.VideoCodecs.FirstOrDefault(x => x.Name.Contains("H264"));
        // _webRtcUtils.IsMicrophoneEnabled = false;
        //      //  PeerConnectionClient.Signalling.Conductor.Instance.MuteMicrophone();
#if VP8_ENCODING
        _webRtcUtils.SelectedVideoCodec = _webRtcUtils.VideoCodecs.FirstOrDefault(x => x.Name.Contains("VP8"));
#else
        _webRtcControl.SelectedVideoCodec = _webRtcControl.VideoCodecs.FirstOrDefault(x => x.Name.Contains("H264"));
#endif
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
        StatusText.text += msg;
    }

    public void ConnectToServer()
    {
        var signalhost = ServerInputTextField.text.Split(':');
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
#if !UNITY_EDITOR
        _webRtcControl.ConnectToServer(host, port, PeerInputTextField.text);
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
        // TODO: Support Peer Selection        
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
        lock (_executionQueue)
        {
            _executionQueue.Enqueue(action);
        }
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
        if(enabledStereo)
        {
            var leftViewProjection = LeftCamera.cullingMatrix;
            var rightViewProjection = RightCamera.cullingMatrix;

            // Builds the camera transform message.
            var leftCameraTransform = "";
            var rightCameraTransform = "";
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    leftCameraTransform += leftViewProjection[i, j] + ",";
                    rightCameraTransform += rightViewProjection[i, j];
                    if (i != 3 || j != 3)
                    {
                        rightCameraTransform += ",";
                    }
                }
            }

            var cameraTransformBody = leftCameraTransform + rightCameraTransform;
            var msg =
                "{" +
                "  \"type\":\"camera-transform-stereo\"," +
                "  \"body\":\"" + cameraTransformBody + "\"" +
                "}";

            // Send the left and right eye transform
            _webRtcControl.SendPeerDataChannelMessage(msg);
        }

        if (endTime != 0 && Time.time > endTime && !enabledStereo)
        {
            enabledStereo = true;
            // Enables the stereo output mode.
            var msg = "{" +
            "  \"type\":\"stereo-rendering\"," +
            "  \"body\":\"1\"" +
            "}";
            _webRtcControl.SendPeerDataChannelMessage(msg);
        }
        
        lock (_executionQueue)
        {
            while (!_executionQueue.IsEmpty)
            {
                Action qa;
                if (_executionQueue.TryDequeue(out qa))
                {
                    if (qa != null)
                        qa.Invoke();
                }
            }
        }
#endif
    }

    private void GetPlaybackTextureFromPlugin()
    {
        IntPtr nativeTex = IntPtr.Zero;
        Plugin.GetPrimaryTexture(textureWidth, textureHeight, out nativeTex);
        var primaryPlaybackTexture = Texture2D.CreateExternalTexture(textureWidth, textureHeight, TextureFormat.BGRA32, false, false, nativeTex);

        LeftCanvas.texture = primaryPlaybackTexture;
        RightCanvas.texture = primaryPlaybackTexture;
    }

    private IEnumerator CallPluginAtEndOfFrames()
    {
        while (true)
        {
            // Wait until all frame rendering is done
            yield return new WaitForEndOfFrame();

            // Set time for the plugin
            Plugin.SetTimeFromUnity(Time.timeSinceLevelLoad);

            // Issue a plugin event with arbitrary integer identifier.
            // The plugin can distinguish between different
            // things it needs to do based on this ID.
            // For our simple plugin, it does not matter which ID we pass here.
            GL.IssuePluginEvent(Plugin.GetRenderEventFunc(), 1);
        }
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

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "LoadAdaptiveContent")]
        internal static extern void LoadAdaptiveContent([MarshalAs(UnmanagedType.BStr)] string manifestURL);

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "Play")]
        internal static extern void Play();

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "Pause")]
        internal static extern void Pause();

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "Stop")]
        internal static extern void Stop();

        // Unity plugin
        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "SetTimeFromUnity")]
        internal static extern void SetTimeFromUnity(float t);

        [DllImport("MediaEngineUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetRenderEventFunc")]
        internal static extern IntPtr GetRenderEventFunc();
    }
}
