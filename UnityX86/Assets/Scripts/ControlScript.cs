using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.UI;
using WebRtcWrapper;

public class ControlScript : MonoBehaviour
{
    public Text StatusText;
    public Text MessageText;
    public InputField PeerInputTextField;
    public InputField MessageInputField;

    private Transform camTransform;
    private Vector3 prevPos;
    private Quaternion prevRot;
    private WebRtcUtils _webRtcUtils;

    void Start()
    {
        camTransform = Camera.main.transform;
        prevPos = camTransform.position;
        prevRot = camTransform.rotation;

        _webRtcUtils = new WebRtcUtils();
        _webRtcUtils.OnInitialized += _webRtcUtils_OnInitialized;
        _webRtcUtils.OnPeerMessageDataReceived += _webRtcUtils_OnPeerMessageDataReceived;
        _webRtcUtils.OnStatusMessageUpdate += _webRtcUtils_OnStatusMessageUpdate;
        _webRtcUtils.Initialize();
    }

    private void _webRtcUtils_OnInitialized()
    {
        //_webRtcUtils.SelectedVideoCodec = _webRtcUtils.VideoCodecs.FirstOrDefault(x => x.Name.Contains("H264"));
        _webRtcUtils.IsMicrophoneEnabled = false;
    }

    private void _webRtcUtils_OnPeerMessageDataReceived(int peerId, string message)
    {
        MessageText.text += string.Format("{0}-{1}", peerId, message);
    }

    private void _webRtcUtils_OnStatusMessageUpdate(string msg)
    {
        StatusText.text += string.Format("{0}\n", msg);
    }

    public void ConnectToServer()
    {
        _webRtcUtils.ConnectCommandExecute(PeerInputTextField.text);
    }

    public void DisconnectFromServer()
    {
        _webRtcUtils.DisconnectFromServerExecute(null);
    }

    public void ConnectToPeer()
    {
        //_webRtcUtils.SelectedPeer = _webRtcUtils.Peers[0];
        _webRtcUtils.ConnectToPeerCommandExecute(null);   
    }

    public void DisconnectFromPeer()
    {
        _webRtcUtils.DisconnectFromPeerCommandExecute(null);
    }

    public void SendMessageToPeer()
    {
        _webRtcUtils.SendPeerMessageDataExecute(MessageText.text);
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

    }
}
