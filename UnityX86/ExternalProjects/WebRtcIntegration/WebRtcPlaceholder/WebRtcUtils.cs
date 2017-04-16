using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
//using Org.WebRtc;

// PLACEHOLDER Object for UWP Class
namespace WebRtcWrapper
{
    public class WebRtcUtils
    {
        public delegate void OnRawVideoFrameSendDelegate(uint w, uint h, byte[] yPlane, uint yPitch, byte[] vPlane, uint vPitch, byte[] uPlane, uint uPitch);

        public event Action OnInitialized;
        public event Action<int, string> OnPeerMessageDataReceived;
        public event Action<string> OnStatusMessageUpdate;        
        public event Action<uint, uint, byte[]> OnEncodedFrameReceived;
        //public event Action<byte[], byte[], byte[]> OnRawFrameReceived2;
        public event Action OnRawFrameReceived2;
        public OnRawVideoFrameSendDelegate OnRawVideoFrameSend;
                

        public void Initialize()
        {
            OnStatusMessageUpdate?.Invoke("Initialized");            
        }

        public void ConnectCommandExecute(object obj)
        {
            OnStatusMessageUpdate?.Invoke("Connect Server");
        }

        public void ConnectToServer(string host, string port, string peerName)
        {
            OnStatusMessageUpdate?.Invoke("ConnectToServer()");            
        }

        public void DisconnectFromServerExecute(object obj)
        {
            OnStatusMessageUpdate?.Invoke("Disconnect Server");
        }

        public void ConnectToPeerCommandExecute(object obj)
        {
            OnStatusMessageUpdate?.Invoke("Connect Peer");
            OnRawFrameReceived2?.Invoke();

        }

        public void SendPeerMessageDataExecute(object obj)
        {
            OnStatusMessageUpdate?.Invoke("Send Peer Message");
        }

        public void DisconnectFromPeerCommandExecute(object obj)
        {
            OnStatusMessageUpdate?.Invoke("Disconnect Peer");
        }

        #region Properties

        public bool IsMicrophoneEnabled { get; set; }
        public Peer SelectedPeer { get; set; }

        //public CodecInfo SelectedVideoCodec { get; set; }
        //public List<CodecInfo> VideoCodecs { get; set; }
        public List<Peer> Peers = new List<Peer>(){ new Peer(){ Id = 1, Name = "peer1" }};

        #endregion
    }

    public class Peer
    {
        public uint Id;
        public string Name;
    }    
}
