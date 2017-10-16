using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.Events;
using UnityEngine.UI;

[RequireComponent(typeof(Dropdown))]
public class PeerListDropdownAdapter : MonoBehaviour
{
    public class PeerDropdownOption : Dropdown.OptionData
    {
        public PeerListState.Peer Peer
        {
            get;
            private set;
        }

        public PeerDropdownOption(PeerListState.Peer peer)
        {
            this.Peer = peer;
            this.text = peer.Name;
        }
    }

    public PeerListState PeerList;
    
    private Dropdown dropdown;

    private List<PeerListState.Peer> previousPeerList = new List<PeerListState.Peer>();
    
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
            this.GetComponentInParent<MenuStack.MenuRoot>().CloseAsync();
        });
    }

    private void Update()
    {
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