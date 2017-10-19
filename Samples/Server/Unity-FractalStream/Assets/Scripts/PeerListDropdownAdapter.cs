using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.UI;

/// <summary>
/// Allows a dropdown to be used to represent a <see cref="PeerListState"/> object
/// </summary>
[RequireComponent(typeof(Dropdown))]
public class PeerListDropdownAdapter : MonoBehaviour
{
    /// <summary>
    /// Represents an option in a dropdown that contains <see cref="PeerListState.Peer"/> information
    /// </summary>
    public class PeerDropdownOption : Dropdown.OptionData
    {
        /// <summary>
        /// The peer
        /// </summary>
        public PeerListState.Peer Peer
        {
            get;
            private set;
        }

        /// <summary>
        /// Default ctor
        /// </summary>
        /// <param name="peer">the peer</param>
        public PeerDropdownOption(PeerListState.Peer peer)
        {
            this.Peer = peer;
            this.text = peer.Name;
        }
    }

    /// <summary>
    /// The peer list to operate on
    /// </summary>
    public PeerListState PeerList;
    
    /// <summary>
    /// An internal reference to our dropdown component
    /// </summary>
    private Dropdown dropdown;

    /// <summary>
    /// A cached copy of the peer list used to diff and raise events on changes
    /// </summary>
    private List<PeerListState.Peer> previousPeerList = new List<PeerListState.Peer>();

    /// <summary>
    /// Unity engine object Start() hook
    /// </summary>
    private void Start()
    {
        this.dropdown = this.GetComponent<Dropdown>();
        this.dropdown.onValueChanged.AddListener((int index) =>
        {
            // must actually contain the index to proceed
            if (this.dropdown.options.Count <= index)
            {
                return;
            }

            var opt = this.dropdown.options.ElementAt(index) as PeerDropdownOption;

            this.PeerList.SelectedPeer = this.PeerList.Peers.First(p => p.Equals(opt.Peer));

            // close the open menu (the one we sit inside of)
            // NOTE: this depends on Unity-MenuStack, if you don't want to take that dependency
            // you can change this to some logic that hides and disables the menu
            this.GetComponentInParent<MenuStack.MenuRoot>().CloseAsync();
        });
    }

    /// <summary>
    /// Unity engine object Update() hook
    /// </summary>
    private void Update()
    {
        if (this.PeerList.SelectedPeer != null)
        {
            // close the open menu (the one we sit inside of)
            this.GetComponentInParent<MenuStack.MenuRoot>().CloseAsync();
            return;
        }

        bool isDirty = false;
        foreach (var peer in this.PeerList.Peers)
        {
            if (!this.previousPeerList.Contains(peer))
            {
                this.previousPeerList.Add(peer);
                isDirty = true;
            }
        }

        if (!isDirty)
        {
            return;
        }

        this.dropdown.ClearOptions();
        this.dropdown.AddOptions(this.previousPeerList.Select(p => new PeerDropdownOption(p) as Dropdown.OptionData).ToList());
    }
}