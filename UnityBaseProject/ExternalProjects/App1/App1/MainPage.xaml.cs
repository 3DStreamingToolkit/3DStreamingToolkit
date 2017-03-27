using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using System.Threading.Tasks;

using PeerConnectionClient.Model;
using PeerConnectionClient.MVVM;
using PeerConnectionClient.Signalling;
using PeerConnectionClient.Utilities;
using System.Collections.ObjectModel;
using System.Windows;



// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace App1
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        private ObservableCollection<Peer> Peers;
        private Peer peerConnection;

        public MainPage()
        {
            this.InitializeComponent();

            MessagesTextBlock.Text += "\nStarted...\n";
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


        }

        private async void Instance_OnReadyToConnect()
        {
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                () => {
                    MessagesTextBlock.Text +=
                    "Ready To Connect";
                }
            );
        }

        private async void Instance_OnPeerConnectionClosed()
        {
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                () => {
                    MessagesTextBlock.Text +=
                    "Peer Connection Closed";
                }
            );
        }

        private async void Instance_OnPeerConnectionCreated()
        {
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                () => {
                    MessagesTextBlock.Text +=
                    "Peer Connection Created";
                }
            );
        }

        private async void Signaller_OnServerConnectionFailure()
        {
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                () => {
                    MessagesTextBlock.Text +=
                    "Server Connection Failure";
                }
            );
        }

        private async void Signaller_OnPeerHangup(int peer_id)
        {
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                () => {
                    MessagesTextBlock.Text +=
                    "Peer Hangup: " + peer_id;
                }
            );
        }

        private async void Signaller_OnPeerDisconnected(int peer_id)
        {
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                () => {
                    MessagesTextBlock.Text +=
                    "Peer Disconnected: " + peer_id;
                }
            );
        }

        private async void Signaller_OnMessageFromPeer(int peer_id, string message)
        {
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                () => {
                    MessagesTextBlock2.Text += String.Format("{0}-{1}\n", peer_id, message);
                }
            );
        }

        private async void Signaller_OnPeerConnected(int id, string name)
        {
            if (Peers == null)
            {
                Peers = new ObservableCollection<Peer>();
                Conductor.Instance.Peers = Peers;
            }
            Peers.Add(peerConnection = new Peer { Id = id, Name = name });

            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                () => {
                    MessagesTextBlock.Text += String.Format("Peer Connected: {0}-{1}\n", id, name);                    
                }
            );            
        }

        private async void Signaller_OnDisconnected()
        {            
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                () => {
                    MessagesTextBlock.Text += "\nSignaller Disconnected\n";
                }
            );
        }

        private async void Signaller_OnSignedIn()
        {
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                () => {
                    MessagesTextBlock.Text += "\nSignaller Signed In\n";
                }
            );
        }

        private async void SendButton_Click(object sender, RoutedEventArgs e)
        {
            //MessagesTextBlock.Text += MessageTextBox.Text + "\n";
            if (peerConnection != null)
            {
                await Conductor.Instance.Signaller.SendToPeer(peerConnection.Id, MessageTextBox.Text);
            }
        }

        private void ConnectButton_Click(object sender, RoutedEventArgs e)
        {
            string name = PeerTextBox.Text;

            new Task(() =>
            {
                Conductor.Instance.StartLogin(
                "n3dtoolkit.southcentralus.cloudapp.azure.com",
                "8888",
                name);
            }).Start();
        }

        private void DisconnectButton_Click(object sender, RoutedEventArgs e)
        {
            //Conductor.Instance.StartLogin(
            //    "n3dtoolkit.southcentralus.cloudapp.azure.com",
            //    "8888");            

            new Task(() =>
            {                
                var task = Conductor.Instance.DisconnectFromServer();
            }).Start();
        }

        private void ClearButton_Click(object sender, RoutedEventArgs e)
        {
            MessagesTextBlock.Text = String.Empty;
        }

        private void ClearButton2_Click(object sender, RoutedEventArgs e)
        {
            MessagesTextBlock2.Text = String.Empty;
        }

        private void ConnectPeerButton_Click(object sender, RoutedEventArgs e)
        {
            if (peerConnection != null)
            {
                MessagesTextBlock.Text += string.Format("Connect to Peer: {0}\n", peerConnection.Id);
                new Task(() => { Conductor.Instance.ConnectToPeer(peerConnection); }).Start();
            }
            else
            {
                MessagesTextBlock.Text += "No Peer Setup\n";
            }
        }
                
    }
}
