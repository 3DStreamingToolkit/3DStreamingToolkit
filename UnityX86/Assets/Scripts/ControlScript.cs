using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.UI;
using WebRtcWrapper;

#if !UNITY_EDITOR
using Org.WebRtc;
#endif

public class ControlScript : MonoBehaviour
{
    public Text StatusText;
    public Text MessageText;
    public InputField ServerInputTextField;
    public InputField PeerInputTextField;
    public InputField MessageInputField;
    public Renderer RenderTexture;

    private Transform camTransform;
    private Vector3 prevPos;
    private Quaternion prevRot;
    private WebRtcUtils _webRtcUtils;    

    private int frameCounter = 0;
    private int fpsCounter = 0;
    private float fpsCount = 0f;
    private float startTime = 0;
    private float endTime = 0;
    
    private static readonly Queue<Action> _executionQueue = new Queue<Action>();    
    private bool frame_ready_receive = true;

    #region DLL Setup
#if !UNITY_EDITOR
    public RawVideoSource rawVideo;
    public EncodedVideoSource encodedVideo;
    private MediaVideoTrack _peerVideoTrack;
#endif

#if UNITY_EDITOR
    [DllImport("TexturesWin32")]
#else
    [DllImport("TexturesUWP")]
#endif
    private static extern void SetTextureFromUnity(System.IntPtr texture, int w, int h);

#if UNITY_EDITOR
    [DllImport("TexturesWin32")]
#else
    [DllImport("TexturesUWP")]
#endif
    private static extern void ProcessRawFrame(uint w, uint h, IntPtr yPlane, uint yStride, IntPtr uPlane, uint uStride,
        IntPtr vPlane, uint vStride);

#if UNITY_EDITOR
    [DllImport("TexturesWin32")]
#else
    [DllImport("TexturesUWP")]
#endif
    private static extern void ProcessH264Frame(uint w, uint h, IntPtr data, uint dataSize);

#if UNITY_EDITOR
    [DllImport("TexturesWin32")]
#else
    [DllImport("TexturesUWP")]
#endif
    private static extern IntPtr GetRenderEventFunc();
    #endregion

    void Start()
    {
        camTransform = Camera.main.transform;
        prevPos = camTransform.position;
        prevRot = camTransform.rotation;

        _webRtcUtils = new WebRtcUtils();
        _webRtcUtils.OnInitialized += _webRtcUtils_OnInitialized;
        _webRtcUtils.OnPeerMessageDataReceived += _webRtcUtils_OnPeerMessageDataReceived;
        _webRtcUtils.OnStatusMessageUpdate += _webRtcUtils_OnStatusMessageUpdate;

#if !UNITY_EDITOR
        PeerConnectionClient.Signalling.Conductor.Instance.OnAddRemoteStream += Conductor_OnAddRemoteStream;
#endif
        _webRtcUtils.Initialize();


        // Setup Low-Level Graphics Plugin
        CreateTextureAndPassToPlugin();
        StartCoroutine(CallPluginAtEndOfFrames());

        ConnectToServer();
        ConnectToPeer();
    }

#if !UNITY_EDITOR
    private void Conductor_OnAddRemoteStream(MediaStreamEvent evt)
    {        
        _peerVideoTrack = evt.Stream.GetVideoTracks().FirstOrDefault();
        if (_peerVideoTrack != null)
        {
            // Raw Video from VP8 Encoded Sender
            // H264 Encoded Stream does not trigger this event
            
            // TODO:  Switch between RAW or ENCODED Frame
            rawVideo = Media.CreateMedia().CreateRawVideoSource(_peerVideoTrack);
            rawVideo.OnRawVideoFrame += Source_OnRawVideoFrame;

//            encodedVideo = Media.CreateMedia().CreateEncodedVideoSource(_peerVideoTrack);
//            encodedVideo.OnEncodedVideoFrame += EncodedVideo_OnEncodedVideoFrame;
        }        
    }
#endif

    private void EncodedVideo_OnEncodedVideoFrame(uint w, uint h, byte[] data)
    {
        frameCounter++;
        fpsCounter++;

        if (frame_ready_receive)
            frame_ready_receive = false;
        else
            return;

        GCHandle buf = GCHandle.Alloc(data, GCHandleType.Pinned);
        ProcessH264Frame(w, h, buf.AddrOfPinnedObject(), (uint)data.Length);
        buf.Free();
    }


    private void Source_OnRawVideoFrame(
        uint w,
        uint h,
        byte[] yPlane,
        uint yStride,
        byte[] uPlane,
        uint uStride,
        byte[] vPlane,
        uint vStride)
    {
        frameCounter++;
        fpsCounter++;
        
        if (frame_ready_receive)
            frame_ready_receive = false;
        else
            return;

        GCHandle yP = GCHandle.Alloc(yPlane, GCHandleType.Pinned);
        GCHandle uP = GCHandle.Alloc(uPlane, GCHandleType.Pinned);
        GCHandle vP = GCHandle.Alloc(vPlane, GCHandleType.Pinned);
        ProcessRawFrame(w, h, yP.AddrOfPinnedObject(), yStride, uP.AddrOfPinnedObject(), uStride,
            vP.AddrOfPinnedObject(), vStride);
        yP.Free();
        uP.Free();
        vP.Free();        
    }
   
    private void _webRtcUtils_OnInitialized()
    {
        EnqueueAction(OnInitialized);
    }

    private void OnInitialized()
    {
#if !UNITY_EDITOR
        _webRtcUtils.SelectedVideoCodec = _webRtcUtils.VideoCodecs.FirstOrDefault(x => x.Name.Contains("H264"));
        // _webRtcUtils.IsMicrophoneEnabled = false;
        PeerConnectionClient.Signalling.Conductor.Instance.MuteMicrophone();
#endif
        StatusText.text += "WebRTC Initialized\n";
    }

    private void _webRtcUtils_OnPeerMessageDataReceived(int peerId, string message)
    {        
        EnqueueAction(() => UpdateMessageText(string.Format("{0}-{1}", peerId, message)));
    }


    private void _webRtcUtils_OnStatusMessageUpdate(string msg)
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
        //RTCIceServer turnServer = new RTCIceServer();
#if !UNITY_EDITOR
        PeerConnectionClient.Model.IceServer turnServer = new PeerConnectionClient.Model.IceServer()
        {
            Host = new PeerConnectionClient.Utilities.ValidableNonEmptyString("13.65.204.45:3478"),
            Type = PeerConnectionClient.Model.IceServer.ServerType.TURN,
            Username = "anzoloch",
            Credential = "3Dstreaming0317"
        };
        _webRtcUtils.IceServers.Clear();
        _webRtcUtils.IceServers.Add(turnServer);
        _webRtcUtils.ConnectToServer("13.65.196.240", "8888", PeerInputTextField.text);
#endif
    }

    public void DisconnectFromServer()
    {
        _webRtcUtils.DisconnectFromServerExecute(null);
    }

    public void ConnectToPeer()
    {
        // TODO: Support Peer Selection
        //_webRtcUtils.SelectedPeer = _webRtcUtils.Peers[0];        
        _webRtcUtils.ConnectToPeerCommandExecute(null);

        endTime = (startTime = Time.time) + 10f;
    }

    public void DisconnectFromPeer()
    {
        _webRtcUtils.DisconnectFromPeerCommandExecute(null);
    }

    public void SendMessageToPeer()
    {
        _webRtcUtils.SendPeerMessageDataExecute(MessageInputField.text);
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
    
    void Update()
    {
        if (Vector3.Distance(prevPos, camTransform.position) > 0.05f ||
    Quaternion.Angle(prevRot, camTransform.rotation) > 2f)
        {
            prevPos = camTransform.position;
            prevRot = camTransform.rotation;
            var eulerRot = prevRot.eulerAngles;
            var camMsg = string.Format(
                @"{{""camera-transform"":""{0},{1},{2},{3},{4},{5}""}}",
                prevPos.x,
                prevPos.y,
                prevPos.z,
                eulerRot.x,
                eulerRot.y,
                eulerRot.z);

            _webRtcUtils.SendPeerMessageDataExecute(camMsg);
        }

        
        if (Time.time > endTime)
        {
            fpsCount = (float)fpsCounter / (Time.time - startTime);
            fpsCounter = 0;
            endTime = (startTime = Time.time) + 3;
        }

        MessageText.text = string.Format("Raw Frame: {0}\nFPS: {1}", frameCounter, fpsCount);
        lock (_executionQueue)
        {
            while (_executionQueue.Count > 0)
            {
                _executionQueue.Dequeue().Invoke();
            }
        }
    }

    private void CreateTextureAndPassToPlugin()
    {        
        Texture2D tex = new Texture2D(1280, 720, TextureFormat.ARGB32, false);
        tex.filterMode = FilterMode.Point;       
        tex.Apply();
        RenderTexture.material.mainTexture = tex;
        SetTextureFromUnity(tex.GetNativeTexturePtr(), tex.width, tex.height);
    }

    private IEnumerator CallPluginAtEndOfFrames()
    {
        while (true)
        {
            // Wait until all frame rendering is done
            yield return new WaitForEndOfFrame();

            // Issue a plugin event with arbitrary integer identifier.
            // The plugin can distinguish between different
            // things it needs to do based on this ID.
            // For our simple plugin, it does not matter which ID we pass here.

            if (!frame_ready_receive)
            {
                GL.IssuePluginEvent(GetRenderEventFunc(), 1);              
                frame_ready_receive = true;               
            }
        }
    }
}
