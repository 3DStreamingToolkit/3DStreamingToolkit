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
using Windows.UI.Core;


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

#if LOCAL
        private string signalHost = "n3dtoolkit.southcentralus.cloudapp.azure.com";
#else
        private string signalHost = "localhost";
#endif
        private string signalPort = "8888";

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

        public async void RunOnUi(DispatchedHandler handler)
        {
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                handler
            );
        }

        private void Instance_OnReadyToConnect()
        {
            RunOnUi(
                () => {
                    MessagesTextBlock.Text +=
                    "Ready To Connect";
                }
            );
        }

        private void Instance_OnPeerConnectionClosed()
        {
            RunOnUi(
                () => {
                    MessagesTextBlock.Text +=
                    "Peer Connection Closed";
                }
            );
        }

        private void Instance_OnPeerConnectionCreated()
        {
            RunOnUi(
                () => {
                    MessagesTextBlock.Text +=
                    "Peer Connection Created";
                }
            );
        }

        private void Signaller_OnServerConnectionFailure()
        {
            RunOnUi(
                () => {
                    MessagesTextBlock.Text +=
                    "Server Connection Failure";
                }
            );
        }

        private void Signaller_OnPeerHangup(int peer_id)
        {
            RunOnUi(
                () => {
                    MessagesTextBlock.Text +=
                    "Peer Hangup: " + peer_id;
                }
            );
        }

        private void Signaller_OnPeerDisconnected(int peer_id)
        {
            RunOnUi(
                () => {
                    MessagesTextBlock.Text +=
                    "Peer Disconnected: " + peer_id;
                }
            );
        }

        private void Signaller_OnMessageFromPeer(int peer_id, string message)
        {
            RunOnUi(
                () => {
                    MessagesTextBlock2.Text += String.Format("{0}-{1}\n", peer_id, message);
                }
            );
        }

        private void Signaller_OnPeerConnected(int id, string name)
        {
            if (Peers == null)
            {
                Peers = new ObservableCollection<Peer>();
                Conductor.Instance.Peers = Peers;
            }
            Peers.Add(peerConnection = new Peer { Id = id, Name = name });

            RunOnUi(
                () => {
                    MessagesTextBlock.Text += String.Format("Peer Connected: {0}-{1}\n", id, name);                    
                }
            );            
        }

        private void Signaller_OnDisconnected()
        {
            RunOnUi(
                () => {
                    MessagesTextBlock.Text += "\nSignaller Disconnected\n";
                }
            );
        }

        private void Signaller_OnSignedIn()
        {
            RunOnUi(
                () => {
                    MessagesTextBlock.Text += "\nSignaller Signed In\n";
                }
            );
        }

        private async void SendButton_Click(object sender, RoutedEventArgs e)
        {                 
            if (peerConnection != null)
            {
                MessagesTextBlock.Text += string.Format("Attempt Send: {0}-{1}\n", peerConnection.Id, MessageTextBox.Text);
                await Conductor.Instance.Signaller.SendToPeer(peerConnection.Id, MessageTextBox.Text);               
            }
        }

        private void ConnectButton_Click(object sender, RoutedEventArgs e)
        {
            string name = PeerTextBox.Text;

            new Task(() =>
            {
                Conductor.Instance.StartLogin(
                signalHost,
                signalPort,
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
                new Task(() =>
                {
                    Conductor.Instance.ConnectToPeer(peerConnection);
                }).Start();
            }
            else
            {
                MessagesTextBlock.Text += "No Peer Setup\n";
            }
        }                
    }
}
