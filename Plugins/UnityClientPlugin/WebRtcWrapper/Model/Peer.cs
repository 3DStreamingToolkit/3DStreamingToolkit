using System;
using System.Collections.Generic;
using System.Text;

namespace PeerConnectionClient.Model
{
    public class Peer
    {
        public int Id { get; set; }
        public string Name { get; set; }

        public override string ToString()
        {
            return Id + ": " + Name;
        }
    }
}
