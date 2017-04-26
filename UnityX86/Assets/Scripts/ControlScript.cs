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
    private Texture2D sourceTexture2D;

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
    private static extern unsafe void ProcessRawFrame(uint w, uint h, IntPtr yPlane, uint yStride, IntPtr uPlane, uint uStride,
        IntPtr vPlane, uint vStride);

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
    }

#if !UNITY_EDITOR
    private void Conductor_OnAddRemoteStream(MediaStreamEvent evt)
    {        
        _peerVideoTrack = evt.Stream.GetVideoTracks().FirstOrDefault();
        if (_peerVideoTrack != null)
        {
            // Raw Video from VP8 Encoded Sender
            // H264 Encoded Stream does not trigger this event
            PeerConnectionClient.Signalling.Conductor.Instance.MuteMicrophone();
            rawVideo = Media.CreateMedia().CreateRawVideoSource(_peerVideoTrack);
            rawVideo.OnRawVideoFrame += Source_OnRawVideoFrame;
        }        
    }        
#endif


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

    private void ConvertYUVFrame(       // REFERENCE ONLY
        uint width,
        uint height,
        byte[] yPlane,
        uint yPitch,
        byte[] vPlane,
        uint vPitch,
        byte[] uPlane,
        uint uPitch)
    {
        frameCounter++;

        int i = 0;
        for (int x = 0; x < width; x++)
        {
            for (int y = 0; y < height; y++)
            {
                float yVal;
                float uVal;
                float vVal;

                // 4-pixel group sample for a 640 x 640 frame?
                // Format seems like a YUV 420 (YV12)?
                yVal = yPlane[i];
                var p = y /2 * vPitch + x / 2;      // Assume V,U to have the same index
                vVal = vPlane[p];
                uVal = uPlane[p];

                float r = Mathf.Clamp(yVal + (1.4065f * (vVal - 128)), 0f, 255f) / 255f;
                float g = Mathf.Clamp(yVal - (0.3455f * (uVal - 128)) - (0.7169f * (vVal - 128)), 0f, 255f) / 255f;
                float b = Mathf.Clamp(yVal + (1.7790f * (uVal - 128)), 0f, 255f) / 255f;

                Color c = new Color(r, g, b, 1.0f);
                sourceTexture2D.SetPixel(x, y, c);

                i++;
            }
        }
        sourceTexture2D.Apply();
    }

    private void _webRtcUtils_OnInitialized()
    {
        EnqueueAction(OnInitialized);
    }

    private void OnInitialized()
    {
#if !UNITY_EDITOR
        // _webRtcUtils.SelectedVideoCodec = _webRtcUtils.VideoCodecs.FirstOrDefault(x => x.Name.Contains("H264"));
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
        _webRtcUtils.ConnectToServer(ServerInputTextField.text, "8888", PeerInputTextField.text);
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
        Texture2D tex = new Texture2D(640, 640, TextureFormat.ARGB32, false);
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
