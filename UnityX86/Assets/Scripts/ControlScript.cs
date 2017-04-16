using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
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
    public Renderer renderer;

    private Transform camTransform;
    private Vector3 prevPos;
    private Quaternion prevRot;
    private WebRtcUtils _webRtcUtils;
    private Texture2D sourceTexture2D;

    private int frameCounter = 0;

    private static readonly Queue<Action> _executionQueue = new Queue<Action>();

#if !UNITY_EDITOR
    public RawVideoSource rawVideo;
    private MediaVideoTrack _peerVideoTrack;
#endif

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

        sourceTexture2D = new Texture2D(640, 640, TextureFormat.ARGB32, false);
        renderer.material.mainTexture = sourceTexture2D;

        for (int x = 0; x < 640; x++)
        {
            for (int y = 0; y < 640; y++)
            {
                if (Mathf.Abs(x - y) < 50)
                {
                    sourceTexture2D.SetPixel(x, y, Color.yellow);
                }
                else
                {
                    sourceTexture2D.SetPixel(x, y, Color.blue);
                }
            }
        }
        sourceTexture2D.Apply();
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
        uint p0,
        uint p1,
        byte[] p2,
        uint p3,
        byte[] p4,
        uint p5,
        byte[] p6,
        uint p7)
    {       
//        int i = 0;
//        for (int x = 0; x < 640; x++)
//        {
//            for (int y = 0; y < 640; y++)
//            {
//                float yVal = p2[i] / 255f;
//                Color c = new Color(yVal, 0, 0, 1);
//                i++;
//                sourceTexture2D.SetPixel(x, y, c);
//            }
//        }        

        EnqueueAction(() => UpdateFrame(p0,p1,p2,p3,p4,p5,p6,p7));
    }

    private void UpdateFrame(
        uint p0,
        uint p1,
        byte[] p2,
        uint p3,
        byte[] p4,
        uint p5,
        byte[] p6,
        uint p7)
    {
        frameCounter++;
        int i = 0;
        for (int x = 0; x < 640; x++)
        {
            for (int y = 0; y < 640; y++)
            {
                float yVal = p2[i] / 255f;
                Color c = new Color(yVal, 0, 0, 1);
                i++;
                sourceTexture2D.SetPixel(x, y, c);
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

        MessageText.text = string.Format("Raw Frame: {0}", frameCounter);
        lock (_executionQueue)
        {
            while (_executionQueue.Count > 0)
            {
                _executionQueue.Dequeue().Invoke();
            }
        }

    }
}
