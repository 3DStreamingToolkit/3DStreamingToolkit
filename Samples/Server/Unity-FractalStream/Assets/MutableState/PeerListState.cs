using System;
using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// Represents a type of mutable object for storing a list of peers
/// </summary>
[CreateAssetMenu]
public class PeerListState : ScriptableObject
{
    /// <summary>
    /// Represents a peer
    /// </summary>
    [Serializable]
    public class Peer
    {
        public int Id;
        public string Name;

        public override bool Equals(object obj)
        {
            if (obj is Peer)
            {
                return (obj as Peer).Id == this.Id &&
                    (obj as Peer).Name == this.Name;
            }
            else
            {
                return base.Equals(obj);
            }
        }

        public override int GetHashCode()
        {
            return this.Id.GetHashCode() ^ this.Name.GetHashCode();
        }
    }

    /// <summary>
    /// Mutable list of peers
    /// </summary>
    public List<Peer> Peers;

    /// <summary>
    /// Mutable peer to indicate it is the selected peer from the <see cref="Peers"/> list
    /// </summary>
    public Peer SelectedPeer = null;
}