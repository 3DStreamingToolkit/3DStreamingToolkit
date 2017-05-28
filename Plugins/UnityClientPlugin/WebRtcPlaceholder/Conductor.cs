using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;


namespace PeerConnectionClient.Signalling
{
    public class Conductor
    {
        //public event Action<MediaStreamEvent> OnAddRemoteStream;
        private static readonly object InstanceLock = new object();
        public static Conductor Instance;
    }
}

        
        