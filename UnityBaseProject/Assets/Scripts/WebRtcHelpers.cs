using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

#if !UNITY_EDITOR
using WebRtcUtils;

using System.Collections.ObjectModel;
using PeerConnectionClient.Model;
using PeerConnectionClient.MVVM;
using PeerConnectionClient.Signalling;
using PeerConnectionClient.Utilities;

#else

public class Peer
{
    public int Id { get; set; }
    public string Name { get; set; }
    public override string ToString()
    {
        return Id + ": " + Name;
    }
}
#endif



public class WebRtcHelpers : MonoBehaviour
{
    public Text statusText;
    public Text peerText;
    public Text inputText;
    public Text camText;

    private Transform camTransform;
    private Vector3 prevPos;
    private Quaternion prevRot;
    private string camMessage;

#if !UNITY_EDITOR
    private ObservableCollection<Peer> Peers;
#else
    private List<Peer> Peers;
#endif
    private Peer peerConnection = null;


    void Start()
    {
        statusText.text += "\nStarted...\n";
        camTransform = Camera.main.transform;
        prevPos = camTransform.position;
        prevRot = camTransform.rotation;


#if !UNITY_EDITOR
        Conductor.Instance.Signaller.OnSignedIn += Signaller_OnSignedIn;
        Conductor.Instance.Signaller.OnDisconnected += Signaller_OnDisconnected;
        Conductor.Instance.Signaller.OnPeerConnected += Signaller_OnPeerConnected;
        Conductor.Instance.Signaller.OnMessageFromPeer += Signaller_OnMessageFromPeer;
        Conductor.Instance.Signaller.OnPeerDisconnected += Signaller_OnPeerDisconnected;
        Conductor.Instance.Signaller.OnPeerHangup += Signaller_OnPeerHangup;
        Conductor.Instance.Signaller.OnServerConnectionFailure += Signaller_OnServerConnectionFailure;

        Conductor.Instance.OnPeerConnectionCreated += Instance_OnPeerConnectionCreated;
        Conductor.Instance.OnPeerConnectionClosed += Instance_OnPeerConnectionClosed;
        Conductor.Instance.OnReadyToConnect += Instance_OnReadyToConnect;
#endif
    }

    void Update()
    {        
        if (Vector3.Distance(prevPos, camTransform.position) > 0.05f ||
            Quaternion.Angle(prevRot, camTransform.rotation) > 2f)
        {
            prevPos = camTransform.position;
            prevRot = camTransform.rotation;
            var eulerRot = prevRot.eulerAngles;
            camText.text = camMessage = string.Format(
                "CAM:{0},{1},{2},{3},{4},{5}",
                prevPos.x,
                prevPos.y,
                prevPos.z,
                eulerRot.x,
                eulerRot.y,
                eulerRot.z);

            if (peerConnection != null)
            {
#if !UNITY_EDITOR
                Conductor.Instance.Signaller.SendToPeer(peerConnection.Id, camMessage);
#endif
            }
        }

    }

    public void VersionCheck()
    {
#if !UNITY_EDITOR
        statusText.text = "UWP 10 - Version: " + Utils.LibTestCall();        
#else
        statusText.text = "NET 3.5 Script";
#endif
    }

    private void Instance_OnReadyToConnect()
    {
        statusText.text += "Ready To Connect\n";
    }

    private void Instance_OnPeerConnectionClosed()
    {
        statusText.text += "Peer Connection Closed\n";
    }

    private void Instance_OnPeerConnectionCreated()
    {
        statusText.text += "Peer Connection Created\n";
    }

    private void Signaller_OnServerConnectionFailure()
    {
        statusText.text += "Peer Connection Connection Failure\n";
    }

    private void Signaller_OnPeerHangup(int peer_id)
    {
        statusText.text += "Peer Hangup: " + peer_id + "\n";
    }

    private void Signaller_OnPeerDisconnected(int peer_id)
    {
        statusText.text += string.Format("Peer Disconnected: {0}\n", peer_id);
        if (peerConnection.Id == peer_id)
        {
            peerConnection = null;
        }
    }

    private void Signaller_OnMessageFromPeer(int peer_id, string message)
    {        
        statusText.text += string.Format("MSG:{0}\n{1}\n", peer_id, message);
    }

    private void Signaller_OnPeerConnected(int id, string name)
    {
        if (Peers == null)
        {

#if !UNITY_EDITOR
            Peers = new ObservableCollection<Peer>();
            Conductor.Instance.Peers = Peers;
#else
            Peers = new List<Peer>();
#endif
        }
        // TODO:  Support Peer Selection instead of automatically switching latest connection
        Peers.Add(peerConnection = new Peer { Id = id, Name = name });
        statusText.text += string.Format("Peer Connected: {0}-{1}", id, name);
    }

    private void Signaller_OnDisconnected()
    {
        statusText.text += "Signaller Disconnected\n";
    }

    private void Signaller_OnSignedIn()
    {
        statusText.text += "Signaller Signed-In\n";
    }

    public void ConnectToSignaller()
    {        
        statusText.text += "Connect to Signaller\n";
        if (peerText.text == string.Empty)
        {
            peerText.text = "UnityClient";
        }

#if !UNITY_EDITOR
//Conductor.Instance.StartLogin(
//       server,
//       port,
//       peer);

        Conductor.Instance.StartLogin(
               "n3dtoolkit.southcentralus.cloudapp.azure.com",
               "8888",
               peerText.text);
#endif
    }

    public void DisconnectFromSignaller()
    {
#if !UNITY_EDITOR
        Conductor.Instance.DisconnectFromServer();
#endif
    }

    public void SendToPeer()
    {
#if !UNITY_EDITOR
        Conductor.Instance.Signaller.SendToPeer(peerConnection.Id, inputText.text);
#endif
    }

}
