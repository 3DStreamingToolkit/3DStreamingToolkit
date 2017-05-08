//#define HACK_VP8

using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Linq;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.UI;
using WebRtcWrapper;

#if !UNITY_EDITOR
using Org.WebRtc;
using PeerConnectionClient.Model;
using PeerConnectionClient.Signalling;
using PeerConnectionClient.Utilities;
#endif

public class ControlScript : MonoBehaviour
{
    private const int textureWidth = 1280;
    private const int textureHeight = 720;
    public Text StatusText;
    public Text MessageText;
    public InputField ServerInputTextField;
    public InputField PeerInputTextField;
    public InputField MessageInputField;
    public Renderer RenderTexture;
    public Transform VirtualCamera;

    private Transform camTransform;
    private Vector3 prevPos;
    private Quaternion prevRot;    

    private WebRtcControl _webRtcControl;

    private int frameCounter = 0;
    private int fpsCounter = 0;
    private float fpsCount = 0f;
    private float startTime = 0;
    private float endTime = 0;
    
    private static readonly ConcurrentQueue<Action> _executionQueue = new ConcurrentQueue<Action>();    
    private bool frame_ready_receive = true;
    private string messageText;

    #region Graphics Low-Level Plugin DLL Setup
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

    void Awake()
    {
        // Azure Host Details
        //ServerInputTextField.text = "signalingserver.centralus.cloudapp.azure.com:3000";
        
        // Local Dev Setup
        ServerInputTextField.text = "127.0.0.1:8888";
    }

    void Start()
    {
        camTransform = Camera.main.transform;
        prevPos = camTransform.position;
        prevRot = camTransform.rotation;

        //_webRtcUtils = new WebRtcUtils();
        _webRtcControl = new WebRtcControl();
        _webRtcControl.OnInitialized += WebRtcControlOnInitialized;
        _webRtcControl.OnPeerMessageDataReceived += WebRtcControlOnPeerMessageDataReceived;
        _webRtcControl.OnStatusMessageUpdate += WebRtcControlOnStatusMessageUpdate;

#if !UNITY_EDITOR
        Conductor.Instance.OnAddRemoteStream += Conductor_OnAddRemoteStream;
#endif
        _webRtcControl.Initialize();

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

            // TODO:  Switch between RAW or ENCODED Frame
#if HACK_VP8
            rawVideo = Media.CreateMedia().CreateRawVideoSource(_peerVideoTrack);
            rawVideo.OnRawVideoFrame += Source_OnRawVideoFrame;
#else            
            encodedVideo = Media.CreateMedia().CreateEncodedVideoSource(_peerVideoTrack);
            encodedVideo.OnEncodedVideoFrame += EncodedVideo_OnEncodedVideoFrame;
#endif
        }
        _webRtcControl.IsReadyToDisconnect = true;
    }

    private void EncodedVideo_OnEncodedVideoFrame(uint w, uint h, byte[] data)
    {
        frameCounter++;
        fpsCounter++;

        messageText = data.Length.ToString();

        if (data.Length == 0)
            return;

        if (frame_ready_receive)
            frame_ready_receive = false;
        else
            return;

        GCHandle buf = GCHandle.Alloc(data, GCHandleType.Pinned);
        ProcessH264Frame(w, h, buf.AddrOfPinnedObject(), (uint)data.Length);
        buf.Free();
    }

    private void Source_OnRawVideoFrame(uint w, uint h, byte[] yPlane, uint yStride, byte[] vPlane, uint vStride, byte[] uPlane, uint uStride)
    {
        frameCounter++;
        fpsCounter++;

        messageText = string.Format("{0}-{1}\n{2}-{3}\n{4}-{5}", 
            yPlane != null ? yPlane.Length.ToString() : "X", yStride,
            vPlane != null ? vPlane.Length.ToString() : "X", vStride,
            uPlane != null ? uPlane.Length.ToString() : "X", uStride);

        if ((yPlane == null) || (uPlane == null) || (vPlane == null))
            return;

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
#endif
    
    private void WebRtcControlOnInitialized()
    {
        EnqueueAction(OnInitialized);
    }


    private void OnInitialized()
    {
#if !UNITY_EDITOR
        // _webRtcUtils.SelectedVideoCodec = _webRtcUtils.VideoCodecs.FirstOrDefault(x => x.Name.Contains("H264"));
        // _webRtcUtils.IsMicrophoneEnabled = false;
//      //  PeerConnectionClient.Signalling.Conductor.Instance.MuteMicrophone();
#if HACK_VP8
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
        _webRtcControl.ConnectToServer(host, port, PeerInputTextField.text);
    }

    public void DisconnectFromServer()
    {        
        _webRtcControl.DisconnectFromServer();
    }

    public void ConnectToPeer()
    {
        // TODO: Support Peer Selection        
#if !UNITY_EDITOR
        if(_webRtcControl.Peers.Count > 0)
        {
            _webRtcControl.SelectedPeer = _webRtcControl.Peers[0];
            _webRtcControl.ConnectToPeer();
            endTime = (startTime = Time.time) + 10f;
        }
#endif
    }

    public void DisconnectFromPeer()
    {
#if !UNITY_EDITOR
        if(encodedVideo != null)
        {
            encodedVideo.OnEncodedVideoFrame -= EncodedVideo_OnEncodedVideoFrame;            
        }
#endif                
        _webRtcControl.DisconnectFromPeer();
    }

    public void SendMessageToPeer()
    {        
        _webRtcControl.SendPeerMessageData(MessageInputField.text);
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
#region Main Camera Control
//        if (Vector3.Distance(prevPos, camTransform.position) > 0.05f ||
//    Quaternion.Angle(prevRot, camTransform.rotation) > 2f)
//        {
//            prevPos = camTransform.position;
//            prevRot = camTransform.rotation;
//            var eulerRot = prevRot.eulerAngles;
//            var camMsg = string.Format(
//                @"{{""camera-transform"":""{0},{1},{2},{3},{4},{5}""}}",
//                prevPos.x,
//                prevPos.y,
//                prevPos.z,
//                eulerRot.x,
//                eulerRot.y,
//                eulerRot.z);
//
//            _webRtcUtils.SendPeerMessageDataExecute(camMsg);
//        }
#endregion


#region Virtual Camera Control
        if (Vector3.Distance(prevPos, VirtualCamera.position) > 0.05f ||
            Quaternion.Angle(prevRot, VirtualCamera.rotation) > 2f)
        {
            prevPos = VirtualCamera.position;
            prevRot = VirtualCamera.rotation;
            var eulerRot = prevRot.eulerAngles;
            var camMsg = string.Format(
                @"{{""camera-transform"":""{0},{1},{2},{3},{4},{5}""}}",
                prevPos.x,
                prevPos.y,
                prevPos.z,
                eulerRot.x,
                eulerRot.y,
                eulerRot.z);

            //_webRtcUtils.SendPeerMessageDataExecute(camMsg);
            _webRtcControl.SendPeerMessageData(camMsg);
        }
#endregion


        if (Time.time > endTime)
        {
            fpsCount = (float)fpsCounter / (Time.time - startTime);
            fpsCounter = 0;
            endTime = (startTime = Time.time) + 3;
        }

        MessageText.text = string.Format("Raw Frame: {0}\nFPS: {1}\n{2}", frameCounter, fpsCount, messageText);

        lock (_executionQueue)            
        {
            while (!_executionQueue.IsEmpty)
            {
                Action qa;
                if (_executionQueue.TryDequeue(out qa))
                {
                    if(qa != null)
                        qa.Invoke();
                }
            }
        }
    }

    private void CreateTextureAndPassToPlugin()
    {
        RenderTexture.transform.localScale = new Vector3(-1f, (float) textureHeight / textureWidth, 1f) * 2f;

        Texture2D tex = new Texture2D(textureWidth, textureHeight, TextureFormat.ARGB32, false);
        // Workaround for Unity Color Space Shift issue
        //Texture2D tex = new Texture2D(textureWidth, textureHeight, TextureFormat.BGRA32, false);
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
