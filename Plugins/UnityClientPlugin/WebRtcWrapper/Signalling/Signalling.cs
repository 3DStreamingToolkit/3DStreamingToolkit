//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Windows.Data.Json;
using Windows.Networking;
using Windows.Networking.Sockets;
using Windows.Storage.Streams;

namespace PeerConnectionClient.Signalling
{
    public delegate void SignedInDelegate();
    public delegate void DisconnectedDelegate();
    public delegate void PeerConnectedDelegate(int id, string name);
    public delegate void PeerDisonnectedDelegate(int peer_id);
    public delegate void PeerHangupDelegate(int peer_id);
    public delegate void MessageFromPeerDelegate(int peer_id, string message);
    public delegate void MessageSentDelegate(int err);
    public delegate void ServerConnectionFailureDelegate(Exception error);

    /// <summary>
    /// Signaller instance is used to fire connection events.
    /// </summary>
    public class Signaller
    {
        // Connection events
        public event SignedInDelegate OnSignedIn;
        public event ServerConnectionFailureDelegate OnServerConnectionFailure;
		public event DisconnectedDelegate OnDisconnected;
        public event PeerConnectedDelegate OnPeerConnected;
        public event PeerDisonnectedDelegate OnPeerDisconnected;
        public event PeerHangupDelegate OnPeerHangup;
        public event MessageFromPeerDelegate OnMessageFromPeer;

		/// <summary>
		/// The connection state.
		/// </summary>
		public enum State
		{
			NOT_CONNECTED,
			CONNECTED
		};
		private State _state;
		
		private Uri _connectUri;
		private string _clientName;
		private int _myId;
		private int _heartbeatTickMs;
		private Timer _heartbeatTimer;
		private HttpClient _httpClient;
		private AuthenticationHeaderValue _authHeader;
		private Dictionary<int, string> _peers = new Dictionary<int, string>();

		/// <summary>
		/// Creates an instance of a Signaller.
		/// </summary>
		public Signaller()
        {
            _state = State.NOT_CONNECTED;
            _myId = -1;
            _heartbeatTickMs = -1;
        }

        /// <summary>
        /// Checks if connected to the server.
        /// </summary>
        /// <returns>True if connected to the server.</returns>
        public bool IsConnected()
        {
            return _myId != -1;
        }

        /// <summary>
        /// The interval at which heartbeats will be sent
        /// </summary>
        /// <returns>time in ms</returns>
        public int HeartbeatMs()
        {
            return _heartbeatTickMs;
        }

        /// <summary>
        /// Set the interval at which heartbeats will be sent
        /// </summary>
        /// <param name="tickMs">time in ms</param>
        /// <remarks>
        /// -1 will disable the heartbeats entirely
        /// </remarks>
        public void SetHeartbeatMs(int tickMs)
        {
            _heartbeatTickMs = tickMs;
            if (_heartbeatTimer != null)
            {
                _heartbeatTimer.Change(
                   TimeSpan.FromMilliseconds(0),
                   TimeSpan.FromMilliseconds(_heartbeatTickMs));
            }
        }

		/// <summary>
		/// Set the authentication header that will be set for each request
		/// </summary>
		/// <param name="authHeader">the value of the header</param>
		public void SetAuthenticationHeader(AuthenticationHeaderValue authHeader)
		{
			_authHeader = authHeader;
		}

        /// <summary>
        /// Connects to the server.
        /// </summary>
        /// <param name="uri">the server to connect to</param>
        /// <param name="client_name">Client name.</param>
        public async void Connect(string uri, string client_name)
        {
            try
            {
                if (_state != State.NOT_CONNECTED)
                {
					OnServerConnectionFailure?.Invoke(new Exception("_state is invalid at start"));
                    return;
                }

				_connectUri = new Uri(uri);
                _clientName = client_name;

				if (_httpClient != null)
				{
					_httpClient.Dispose();
				}
				_httpClient = new HttpClient();

				_httpClient.DefaultRequestHeaders.Host = _connectUri.Host;

				if (_authHeader != null)
				{
					_httpClient.DefaultRequestHeaders.Authorization = _authHeader;
				}

				_httpClient.BaseAddress = _connectUri;

				// set this to 2 minutes, which is the azure max
				// the value here is really fairly arbitrary, since
				// our long polling endpoint will just reconnect if it times out
				_httpClient.Timeout = TimeSpan.FromMinutes(2);

				var res = await this._httpClient.GetAsync($"/sign_in?peer_name={client_name}");

				if (res.StatusCode != HttpStatusCode.OK)
				{
					OnServerConnectionFailure?.Invoke(new Exception("invalid response status " + res.StatusCode));
					return;
				}

				// set the id, from the request
				_myId = int.Parse(res.Headers.Pragma.ToString());

				var body = await res.Content.ReadAsStringAsync();

				// we pass _myId as pragmaId, since they're the same value in this case
				if (!ParseServerResponse(body, _myId))
				{
					OnServerConnectionFailure?.Invoke(new Exception("unable to parse response"));
					return;
				}

				_state = State.CONNECTED;

				OnSignedIn?.Invoke();

                // Start the long polling loop without await
                StartHangingGetReadLoop();

				// start our heartbeat timer
				if (_heartbeatTimer == null)
                {
                    _heartbeatTimer = new Timer(
                        this.HeartbeatLoopAsync, null, 0, _heartbeatTickMs);
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine("[Error] Signaling: Failed to connect to server: " + ex.Message);
				OnServerConnectionFailure?.Invoke(ex);
			}
        }
		
        /// <summary>
        /// triggered at an interval to send heartbeats
        /// </summary>
        private async void HeartbeatLoopAsync(object s)
        {
            if (_state != State.CONNECTED)
            {
                return;
            }

			var res = await this._httpClient.GetAsync($"/heartbeat?peer_id={_myId}");

			if (res.StatusCode != HttpStatusCode.OK)
			{
				Debug.WriteLine("[Info] Signaling: heartbeat failed: {0}", res.StatusCode);
			}
        }

		private void StartHangingGetReadLoop()
		{
			if (_state != State.CONNECTED)
			{
				return;
			}

			Task.Run(async () =>
			{
				await HangingGetReadLoopAsync();
				StartHangingGetReadLoop();
			});
		}

        /// <summary>
        /// Long lasting loop to get notified about connected/disconnected peers.
        /// </summary>
        private async Task HangingGetReadLoopAsync()
        {
            while (_state != State.NOT_CONNECTED)
            {
				try
				{
					var res = await this._httpClient.GetAsync($"/wait?peer_id={_myId}");

					if (res.StatusCode != HttpStatusCode.OK)
					{
						Debug.WriteLine("[Info] Signaling: wait failed: {0}", res.StatusCode);
						return;
					}

					var body = await res.Content.ReadAsStringAsync();
					var pragmaId = int.Parse(res.Headers.Pragma.ToString());

					if (!ParseServerResponse(body, pragmaId))
					{
						Debug.WriteLine("[Info] Signaling: wait parsing failed");
						return;
					}
				}
				catch (Exception ex)
				{
					Debug.WriteLine("[ERROR] Signalling: wait threw: " + ex.Message + "\r\n" + ex.StackTrace);
				}
            }
        }

        /// <summary>
        /// Disconnects the user from the server.
        /// </summary>
        /// <returns>True if the user is disconnected from the server.</returns>
        public async Task<bool> SignOut()
        {
			if (_state == State.NOT_CONNECTED)
			{
				return true;
			}

			var res = await this._httpClient.GetAsync($"/sign_out?peer_id={_myId}");

			if (res.StatusCode != HttpStatusCode.OK)
			{
				Debug.WriteLine("[Info] Signaling: sign_out failed: {0}", res.StatusCode);
				return false;
			}

            _myId = -1;
            _state = State.NOT_CONNECTED;

			Close();

			OnDisconnected?.Invoke();

            return true;
        }

        /// <summary>
        /// Resets the states after connection is closed.
        /// </summary>
        private void Close()
        {
			if (_heartbeatTimer != null)
			{
				_heartbeatTimer.Dispose();
				_heartbeatTimer = null;
			}

			if (_httpClient != null)
            {
				_httpClient.Dispose();
				_httpClient = null;
            }

            _peers.Clear();
            _state = State.NOT_CONNECTED;
        }

        /// <summary>
        /// Updates the capacity data on the signaling server
        /// </summary>
        /// <remarks>
        /// Once capacity reaches 0, we'll be removed from the signaling server listing of available peers
        /// see https://github.com/bengreenier/webrtc-signal-http-capacity for more info
        /// </remarks>
        /// <param name="newCapacity">the new remaining capacity</param>
        /// <returns>success flag</returns>
        public async Task<bool> UpdateCapacity(int newCapacity)
        {
            if (!IsConnected())
            {
                return false;
            }


            var res = await this._httpClient.PutAsync($"/capacity?peer_id={_myId}&value={newCapacity}", new StringContent(""));

            if (res.StatusCode != HttpStatusCode.OK)
            {
                Debug.WriteLine("[Info] Signaling: capacity update failed: {0}", res.StatusCode);
                return false;
            }

            return true;
        }

        /// <summary>
        /// Sends a message to a peer.
        /// </summary>
        /// <param name="peerId">ID of the peer to send a message to.</param>
        /// <param name="message">Message to send.</param>
        /// <returns>True if the message was sent.</returns>
        public async Task<bool> SendToPeer(int peerId, string message)
        {
            if (_state != State.CONNECTED)
            {
                return false;
            }

            Debug.Assert(IsConnected());

            if (!IsConnected() || peerId == -1)
            {
                return false;
            }

			var res = await this._httpClient.PostAsync($"/message?peer_id={_myId}&to={peerId}", new StringContent(message));

			if (res.StatusCode != HttpStatusCode.OK)
			{
				Debug.WriteLine("[Info] Signaling: message failed: {0}", res.StatusCode);
				return false;
			}

			return true;
        }

        /// <summary>
        /// Sends a message to a peer.
        /// </summary>
        /// <param name="peerId">ID of the peer to send a message to.</param>
        /// <param name="json">The json message.</param>
        /// <returns>True if the message is sent.</returns>
        public async Task<bool> SendToPeer(int peerId, IJsonValue json)
        {
            string message = json.Stringify();
			return await SendToPeer(peerId, message);
        }

		/// <summary>
		/// Parses a body and updates peers, emitting events as needed
		/// </summary>
		/// <param name="body">the body to parse</param>
		/// <returns>success indicator</returns>
		private bool ParseServerResponse(string body, int pragmaId)
		{
			// if pragmaId isn't us, it's not a notification it's a message
			if (pragmaId != _myId)
			{
				// if the entire message is bye, they gone
				if (body == "BYE")
				{
					this.OnPeerHangup?.Invoke(pragmaId);
				}
				// otherwise, we share the message
				else
				{
					this.OnMessageFromPeer?.Invoke(pragmaId, body);
				}

				// and then we're done
				return true;
			}

			// oh boy, new peers
			var oldPeers = new Dictionary<int, string>(_peers);

			// peer updates aren't incremental
			_peers.Clear();

			foreach (var line in body.Split('\n'))
			{
				var parts = line.Split(',');

				if (parts.Length != 3)
				{
					continue;
				}

				var id = int.Parse(parts[1]);

				// indicates the peer is connected
				// and isn't us
				if (int.Parse(parts[2]) == 1 && id != _myId)
				{
					_peers[id] = parts[0];
				}
			}

			foreach (var peer in _peers)
			{
				if (!oldPeers.ContainsKey(peer.Key))
				{
					this.OnPeerConnected?.Invoke(peer.Key, peer.Value);
				}
				oldPeers.Remove(peer.Key);
			}

			// handles disconnects that aren't respectful (and don't send /sign_out)
			foreach (var oldPeer in oldPeers)
			{
				this.OnPeerDisconnected?.Invoke(oldPeer.Key);
			}

			// and we're all good
			return true;
		}

		/// <summary>
		/// Parses the given entry information (peer).
		/// </summary>
		/// <param name="entry">Entry info.</param>
		/// <param name="name">Peer name.</param>
		/// <param name="id">Peer ID.</param>
		/// <param name="connected">Connected status of the entry (peer).</param>
		/// <returns>False if fails to parse the entry information.</returns>
		private bool ParseEntry(string entry, ref string name, ref int id, ref bool connected)
		{
			connected = false;
			int separator = entry.IndexOf(',');
			if (separator != -1)
			{
				id = entry.Substring(separator + 1).ParseLeadingInt();
				name = entry.Substring(0, separator);
				separator = entry.IndexOf(',', separator + 1);
				if (separator != -1)
				{
					connected = entry.Substring(separator + 1).ParseLeadingInt() > 0 ? true : false;
				}
			}
			return name.Length > 0;
		}
	}

    /// <summary>
    /// Class providing helper functions for parsing responses and messages.
    /// </summary>
    public static class Extensions
    {
        public static int ParseLeadingInt(this string str)
        {
            return int.Parse(Regex.Match(str, "\\d+").Value);
        }
    }
}
