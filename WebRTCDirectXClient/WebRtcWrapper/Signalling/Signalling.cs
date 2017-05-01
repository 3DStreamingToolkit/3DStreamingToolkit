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
using System.Text.RegularExpressions;
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
    public delegate void ServerConnectionFailureDelegate();

    /// <summary>
    /// Signaller instance is used to fire connection events.
    /// </summary>
    public class Signaller
    {
        // Connection events
        public event SignedInDelegate OnSignedIn;
        public event DisconnectedDelegate OnDisconnected;
        public event PeerConnectedDelegate OnPeerConnected;
        public event PeerDisonnectedDelegate OnPeerDisconnected;
        public event PeerHangupDelegate OnPeerHangup;
        public event MessageFromPeerDelegate OnMessageFromPeer;
        public event ServerConnectionFailureDelegate OnServerConnectionFailure;

        /// <summary>
        /// Creates an instance of a Signaller.
        /// </summary>
        public Signaller()
        {
            _state = State.NOT_CONNECTED;
            _myId = -1;

            // Annoying but register empty handlers
            // so we don't have to check for null everywhere
            OnSignedIn += () => { };
            OnDisconnected += () => { };
            OnPeerConnected += (a, b) => { };
            OnPeerDisconnected += (a) => { };
            OnMessageFromPeer += (a, b) => { };
            OnServerConnectionFailure += () => { };
        }

        /// <summary>
        /// The connection state.
        /// </summary>
        public enum State
        {
            NOT_CONNECTED,
            RESOLVING, // Note: State not used
            SIGNING_IN,
            CONNECTED,
            SIGNING_OUT_WAITING, // Note: State not used
            SIGNING_OUT,
        };
        private State _state;

        private HostName _server;
        private string _port;
        private string _clientName;
        private int _myId;
        private Dictionary<int, string> _peers = new Dictionary<int, string>();

        /// <summary>
        /// Checks if connected to the server.
        /// </summary>
        /// <returns>True if connected to the server.</returns>
        public bool IsConnected()
        {
            return _myId != -1;
        }

        /// <summary>
        /// Connects to the server.
        /// </summary>
        /// <param name="server">Host name/IP.</param>
        /// <param name="port">Port to connect.</param>
        /// <param name="client_name">Client name.</param>
        public async void Connect(string server, string port, string client_name)
        {
            try
            {
                if (_state != State.NOT_CONNECTED)
                {
                    OnServerConnectionFailure();
                    return;
                }

                _server = new HostName(server);
                _port = port;
                _clientName = client_name;

                _state = State.SIGNING_IN;
                await ControlSocketRequestAsync(string.Format("GET /sign_in?{0} HTTP/1.0\r\n\r\n", client_name));
                if (_state == State.CONNECTED)
                {
                    // Start the long polling loop without await
                    var task = HangingGetReadLoopAsync();
                }
                else
                {
                    _state = State.NOT_CONNECTED;
                    OnServerConnectionFailure();
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine("[Error] Signaling: Failed to connect to server: " + ex.Message);
            }
        }

        private StreamSocket _hangingGetSocket;

        #region Parsing
        /// <summary>
        /// Gets the integer value from the message header.
        /// </summary>
        /// <param name="buffer">The message.</param>
        /// <param name="optional">Lack of an optional header is not considered an error.</param>
        /// <param name="header">Header name.</param>
        /// <param name="value">Header value.</param>
        /// <returns>False if fails to find header in the message and header is not optional.</returns>
        private static bool GetHeaderValue(string buffer, bool optional, string header, out int value)
        {
            try
            {
                int index = buffer.IndexOf(header);
                if(index == -1)
                {
                    if (optional)
                    {
                        value = -1;
                        return true;
                    }
                    throw new KeyNotFoundException();
                }
                index += header.Length;
                value = buffer.Substring(index).ParseLeadingInt();
                return true;
            }
            catch
            {
                value = -1;
                if (!optional)
                {
                    Debug.WriteLine("[Error] Failed to find header <" + header + "> in buffer(" + buffer.Length + ")=<" + buffer + ">");
                    return false;
                }
                else
                {
                    return true;
                }
            }
        }
       
        /// <summary>
        /// Gets the string value from the message header.
        /// </summary>
        /// <param name="buffer">The message.</param>
        /// <param name="header">Header name.</param>
        /// <param name="value">Header value.</param>
        /// <returns>False if fails to find header in the message.</returns>
        private static bool GetHeaderValue(string buffer, string header, out string value)
        {
            try
            {
                int startIndex = buffer.IndexOf(header);
                if(startIndex == -1)
                {
                    value = null;
                    return false;
                }
                startIndex += header.Length;
                int endIndex = buffer.IndexOf("\r\n", startIndex);
                value = buffer.Substring(startIndex, endIndex - startIndex);
                return true;
            }
            catch
            {
                value = null;
                return false;
            }
        }

        /// <summary>
        /// Parses the response from the server.
        /// </summary>
        /// <param name="buffer">The message.</param>
        /// <param name="peer_id">Peer ID.</param>
        /// <param name="eoh">End of header name position.</param>
        /// <returns>False if fails to parse the server response.</returns>
        private bool ParseServerResponse(string buffer, out int peer_id, out int eoh)
        {
            peer_id = -1;
            eoh = -1;
            try
            {
                int index = buffer.IndexOf(' ') + 1;
                int status = int.Parse(buffer.Substring(index, 3));
                if (status != 200)
                {
                    if (status == 500 && buffer.Contains("Peer most likely gone."))
                    {
                        Debug.WriteLine("Peer most likely gone. Closing peer connection.");
                        //As Peer Id doesn't exist in buffer using 0
                        OnPeerDisconnected(0);
                        return false;
                    }
                    Close();
                    OnDisconnected();
                    _myId = -1;
                    return false;
                }

                eoh = buffer.IndexOf("\r\n\r\n");
                if (eoh == -1)
                {
                    Debug.WriteLine("[Error] Failed to parse server response (end of header not found)! Buffer(" + buffer.Length + ")=<" + buffer + ">");
                    return false;
                }

                GetHeaderValue(buffer, true, "\r\nPragma: ", out peer_id);
                return true;
            }
            catch (Exception ex)
            {
                Debug.WriteLine("[Error] Failed to parse server response (ex=" + ex.Message + ")! Buffer(" + buffer.Length + ")=<" + buffer + ">");
                return false;
            }
        }

        /// <summary>
        /// Parses the given entry information (peer).
        /// </summary>
        /// <param name="entry">Entry info.</param>
        /// <param name="name">Peer name.</param>
        /// <param name="id">Peer ID.</param>
        /// <param name="connected">Connected status of the entry (peer).</param>
        /// <returns>False if fails to parse the entry information.</returns>
        private static bool ParseEntry(string entry, ref string name, ref int id, ref bool connected)
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
        #endregion

#pragma warning disable 1998
        /// <summary>
        /// Helper to read the information into a buffer.
        /// </summary>
        private async Task<Tuple<string, int>> ReadIntoBufferAsync(StreamSocket socket)
        {
            DataReaderLoadOperation loadTask = null;
            string data;
            try
            {
                var reader = new DataReader(socket.InputStream);
                // Set the DataReader to only wait for available data
                reader.InputStreamOptions = InputStreamOptions.Partial;

                loadTask = reader.LoadAsync(0xffff);
                bool succeeded = loadTask.AsTask().Wait(20000);
                if (!succeeded)
                {
                    throw new TimeoutException("Timed out long polling, re-trying.");
                }

                var count = loadTask.GetResults();
                if (count == 0)
                {
                    throw new Exception("No results loaded from reader.");
                }

                data = reader.ReadString(count);
                if (data == null)
                {
                    throw new Exception("ReadString operation failed.");
                }
            }
            catch (TimeoutException ex)
            {
                Debug.WriteLine(ex.Message);
                if (loadTask != null && loadTask.Status == Windows.Foundation.AsyncStatus.Started)
                {
                    loadTask.Cancel();
                }
                return null;
            }
            catch (Exception ex)
            {
                Debug.WriteLine("[Error] Signaling: Failed to read from socket. " + ex.Message);
                if (loadTask != null && loadTask.Status == Windows.Foundation.AsyncStatus.Started)
                {
                    loadTask.Cancel();
                }
                return null;
            }

            int content_length = 0;
            bool ret = false;
            int i = data.IndexOf("\r\n\r\n");
            if (i != -1)
            {
                Debug.WriteLine("Signaling: Headers received [i=" + i + " data(" + data.Length + ")"/*=" + data*/ + "]");
                if (GetHeaderValue(data, false, "\r\nContent-Length: ", out content_length))
                {
                    int total_response_size = (i + 4) + content_length;
                    if (data.Length >= total_response_size)
                    {
                        ret = true;
                    }
                    else
                    {
                        // We haven't received everything.  Just continue to accept data.
                        Debug.WriteLine("[Error] Singaling: Incomplete response; expected to receive " + total_response_size + ", received" + data.Length);
                    }
                }
                else
                {
                    Debug.WriteLine("[Error] Signaling: No content length field specified by the server.");
                }
            }
            return ret ? Tuple.Create(data, content_length) : null;
        }
#pragma warning restore 1998

        /// <summary>
        /// Sends a request to the server, waits for response and parses it.
        /// </summary>
        /// <param name="sendBuffer">Information to send.</param>
        /// <returns>False if there is a failure. Otherwise returns true.</returns>
        private async Task<bool> ControlSocketRequestAsync(string sendBuffer)
        {
            using (var socket = new StreamSocket())
            {
                // Connect to the server
                try
                {
                    await socket.ConnectAsync(_server, _port);
                }
                catch (Exception e)
                {
                    // This could be a connection failure like a timeout
                    Debug.WriteLine("[Error] Signaling: Failed to connect to " + _server + ":" + _port + " : " + e.Message);
                    return false;
                }
                // Send the request
                socket.WriteStringAsync(sendBuffer);

                // Read the response
                var readResult = await ReadIntoBufferAsync(socket);
                if (readResult == null)
                {
                    return false;
                }

                string buffer = readResult.Item1;
                int content_length = readResult.Item2;

                int peer_id, eoh;
                if (!ParseServerResponse(buffer, out peer_id, out eoh))
                {
                    return false;
                }

                if (_myId == -1)
                {
                    Debug.Assert(_state == State.SIGNING_IN);
                    _myId = peer_id;
                    Debug.Assert(_myId != -1);

                    // The body of the response will be a list of already connected peers
                    if (content_length > 0)
                    {
                        int pos = eoh + 4; // Start after the header.
                        while (pos < buffer.Length)
                        {
                            int eol = buffer.IndexOf('\n', pos);
                            if (eol == -1)
                            {
                                break;
                            }
                            int id = 0;
                            string name = "";
                            bool connected = false;
                            if (ParseEntry(buffer.Substring(pos, eol - pos), ref name, ref id, ref connected) && id != _myId)
                            {
                                _peers[id] = name;
                                OnPeerConnected(id, name);
                            }
                            pos = eol + 1;
                        }
                        OnSignedIn();
                    }
                }
                else if (_state == State.SIGNING_OUT)
                {
                    Close();
                    OnDisconnected();
                }
                else if (_state == State.SIGNING_OUT_WAITING)
                {
                    await SignOut();
                }

                if (_state == State.SIGNING_IN)
                {
                    _state = State.CONNECTED;
                }
            }

            return true;
        }

        /// <summary>
        /// Long lasting loop to get notified about connected/disconnected peers.
        /// </summary>
        private async Task HangingGetReadLoopAsync()
        {
            while (_state != State.NOT_CONNECTED)
            {
                using (_hangingGetSocket = new StreamSocket())
                {
                    try
                    {
                        // Connect to the server
                        await _hangingGetSocket.ConnectAsync(_server, _port);
                        if (_hangingGetSocket == null)
                        {
                            return;
                        }

                        // Send the request
                        _hangingGetSocket.WriteStringAsync(String.Format("GET /wait?peer_id={0} HTTP/1.0\r\n\r\n", _myId));

                        // Read the response.
                        var readResult = await ReadIntoBufferAsync(_hangingGetSocket);
                        if (readResult == null)
                        {
                            continue;
                        }

                        string buffer = readResult.Item1;
                        int content_length = readResult.Item2;

                        int peer_id, eoh;
                        if (!ParseServerResponse(buffer, out peer_id, out eoh))
                        {
                            continue;
                        }

                        // Store the position where the body begins
                        int pos = eoh + 4;

                        if (_myId == peer_id)
                        {
                            // A notification about a new member or a member that just
                            // disconnected
                            int id = 0;
                            string name = "";
                            bool connected = false;
                            if (ParseEntry(buffer.Substring(pos), ref name, ref id, ref connected))
                            {
                                if (connected)
                                {
                                    _peers[id] = name;
                                    OnPeerConnected(id, name);
                                }
                                else
                                {
                                    _peers.Remove(id);
                                    OnPeerDisconnected(id);
                                }
                            }
                        }
                        else
                        {
                            string message = buffer.Substring(pos);
                            if (message == "BYE")
                            {
                                OnPeerHangup(peer_id);
                            }
                            else
                            {
                                OnMessageFromPeer(peer_id, message);
                            }
                        }
                    }
                    catch (Exception e)
                    {
                        Debug.WriteLine("[Error] Signaling: Long-polling exception: {0}", e.Message);
                    }
                }
            }
        }

        /// <summary>
        /// Disconnects the user from the server.
        /// </summary>
        /// <returns>True if the user is disconnected from the server.</returns>
        public async Task<bool> SignOut()
        {
            if (_state == State.NOT_CONNECTED || _state == State.SIGNING_OUT)
                return true;

            if (_hangingGetSocket != null)
            {
                _hangingGetSocket.Dispose();
                _hangingGetSocket = null;
            }

            _state = State.SIGNING_OUT;

            if (_myId != -1)
            {
                await ControlSocketRequestAsync(String.Format("GET /sign_out?peer_id={0} HTTP/1.0\r\n\r\n", _myId));
            }
            else
            {
                // Can occur if the app is closed before we finish connecting
                return true;
            }

            _myId = -1;
            _state = State.NOT_CONNECTED;
            return true;
        }

        /// <summary>
        /// Resets the states after connection is closed.
        /// </summary>
        private void Close()
        {
            if (_hangingGetSocket != null)
            {
                _hangingGetSocket.Dispose();
                _hangingGetSocket = null;
            }

            _peers.Clear();
            _state = State.NOT_CONNECTED;
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

            string buffer = String.Format(
                "POST /message?peer_id={0}&to={1} HTTP/1.0\r\n" +
                "Content-Length: {2}\r\n" +
                "Content-Type: text/plain\r\n" +
                "\r\n" +
                "{3}",
                _myId, peerId, message.Length, message);
            return await ControlSocketRequestAsync(buffer);
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
    }

    /// <summary>
    /// Class providing helper functions for parsing responses and messages.
    /// </summary>
    public static class Extensions
    {
        public static async void WriteStringAsync(this StreamSocket socket, string str)
        {
            try
            {
                var writer = new DataWriter(socket.OutputStream);
                writer.WriteString(str);
                await writer.StoreAsync();
            }
            catch (Exception ex)
            {
                Debug.WriteLine("[Error] Singnaling: Couldn't write to socket : " + ex.Message);
            }
        }

        public static int ParseLeadingInt(this string str)
        {
            return int.Parse(Regex.Match(str, "\\d+").Value);
        }
    }
}
