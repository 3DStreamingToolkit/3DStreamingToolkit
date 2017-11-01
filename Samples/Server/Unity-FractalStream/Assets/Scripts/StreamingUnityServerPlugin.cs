using System;
using System.Collections;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

namespace Microsoft.Toolkit.ThreeD
{
    /// <summary>
    /// Represents the streaming unity server plugin
    /// </summary>
    public class StreamingUnityServerPlugin : IDisposable
    {
        /// <summary>
        /// The name of the plugin from which we dllimport
        /// </summary>
        private const string PluginName = "StreamingUnityServerPlugin";

        #region Dll Imports

        private static class Native
        {
#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
            [DllImport ("__Internal")]
#else
            [DllImport(PluginName)]
#endif
            public static extern IntPtr GetRenderEventFunc();

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
            [DllImport ("__Internal")]
#else
            [DllImport(PluginName)]
            public static extern void Close();
#endif

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
            [DllImport ("__Internal")]
#else
            [DllImport(PluginName)]
            public static extern void SetEncodingStereo(bool encodingStereo);
#endif

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
            [DllImport ("__Internal")]
#else
            [DllImport(PluginName)]
            public static extern void ConnectToPeer(int peerId);
#endif

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
            [DllImport ("__Internal")]
#else
            [DllImport(PluginName)]
            public static extern void DisconnectFromPeer();
#endif

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
            [DllImport ("__Internal")]
#else
            [DllImport(PluginName)]
            public static extern void SetCallbackMap(GenericDelegate<string>.Handler onInputUpdate,
                GenericDelegate<int, string>.Handler onLog,
                GenericDelegate<int, string>.Handler onPeerConnect,
                GenericDelegate<int>.Handler onPeerDisconnect,
                GenericDelegate.Handler onSignIn,
                GenericDelegate.Handler onDisconnect,
                GenericDelegate<int, string>.Handler onMessageFromPeer,
                GenericDelegate<int>.Handler onMessageSent,
                GenericDelegate.Handler onServerConnectionFailure,
                GenericDelegate<int>.Handler onSignalingChange,
                GenericDelegate<string>.Handler onAddStream,
                GenericDelegate<string>.Handler onRemoveStream,
                GenericDelegate<string>.Handler onDataChannel,
                GenericDelegate.Handler onRenegotiationNeeded,
                GenericDelegate<int>.Handler onIceConnectionChange,
                GenericDelegate<int>.Handler onIceGatheringChange,
                GenericDelegate<string>.Handler onIceCandidate,
                GenericDelegate<bool>.Handler onIceConnectionReceivingChange);
#endif
        }

        #endregion

        #region Marshalled events

        public event GenericDelegate<string>.Handler InputUpdate;
        public event GenericDelegate<int, string>.Handler Log;
        public event GenericDelegate<int, string>.Handler PeerConnect;
        public event GenericDelegate<int>.Handler PeerDisconnect;
        public event GenericDelegate.Handler SignIn;
        public event GenericDelegate.Handler Disconnect;
        public event GenericDelegate<int, string>.Handler MessageFromPeer;
        public event GenericDelegate<int>.Handler MessageSent;
        public event GenericDelegate.Handler ServerConnectionFailure;
        public event GenericDelegate<int>.Handler SignalingChange;
        public event GenericDelegate<string>.Handler AddStream;
        public event GenericDelegate<string>.Handler RemoveStream;
        public event GenericDelegate<string>.Handler DataChannel;
        public event GenericDelegate.Handler RenegotiationNeeded;
        public event GenericDelegate<int>.Handler IceConnectionChange;
        public event GenericDelegate<int>.Handler IceGatheringChange;
        public event GenericDelegate<string>.Handler IceCandidate;
        public event GenericDelegate<bool>.Handler IceConnectionReceivingChange;

        private GenericDelegate<string>.Handler pinnedInputUpdate;
        private GenericDelegate<int, string>.Handler pinnedLog;
        private GenericDelegate<int, string>.Handler pinnedPeerConnect;
        private GenericDelegate<int>.Handler pinnedPeerDisconnect;
        private GenericDelegate.Handler pinnedSignIn;
        private GenericDelegate.Handler pinnedDisconnect;
        private GenericDelegate<int, string>.Handler pinnedMessageFromPeer;
        private GenericDelegate<int>.Handler pinnedMessageSent;
        private GenericDelegate.Handler pinnedServerConnectionFailure;
        private GenericDelegate<int>.Handler pinnedSignalingChange;
        private GenericDelegate<string>.Handler pinnedAddStream;
        private GenericDelegate<string>.Handler pinnedRemoveStream;
        private GenericDelegate<string>.Handler pinnedDataChannel;
        private GenericDelegate.Handler pinnedRenegotiationNeeded;
        private GenericDelegate<int>.Handler pinnedIceConnectionChange;
        private GenericDelegate<int>.Handler pinnedIceGatheringChange;
        private GenericDelegate<string>.Handler pinnedIceCandidate;
        private GenericDelegate<bool>.Handler pinnedIceConnectionReceivingChange;

        private void OnInputUpdate(string val0) { ErrorOnFailure(() => { if (this.InputUpdate != null) this.InputUpdate(val0); }); }
        private void OnLog(int val0, string val1) { ErrorOnFailure(() => { if (Log != null) this.Log(val0, val1); }); }
        private void OnPeerConnect(int val0, string val1) { ErrorOnFailure(() => { if (PeerConnect != null) this.PeerConnect(val0, val1); }); }
        private void OnPeerDisconnect(int val0) { ErrorOnFailure(() => { if (this.PeerDisconnect != null) this.PeerDisconnect(val0); }); }
        private void OnSignIn() { ErrorOnFailure(() => { if (this.SignIn != null) this.SignIn(); }); }
        private void OnDisconnect() { ErrorOnFailure(() => { if (this.Disconnect != null) this.Disconnect(); }); }
        private void OnMessageFromPeer(int val0, string val1){ ErrorOnFailure(() => { if (MessageFromPeer != null) this.MessageFromPeer(val0, val1); }); }
        private void OnMessageSent(int val0) { ErrorOnFailure(() => { if (this.MessageSent != null) this.MessageSent(val0); }); }
        private void OnServerConnectionFailure() { ErrorOnFailure(() => { if (this.ServerConnectionFailure != null) this.ServerConnectionFailure(); }); }
        private void OnSignalingChange(int val0) { ErrorOnFailure(() => { if (this.SignalingChange != null) this.SignalingChange(val0); }); }
        private void OnAddStream(string val0) { ErrorOnFailure(() => { if (this.AddStream != null) this.AddStream(val0); }); }
        private void OnRemoveStream(string val0) { ErrorOnFailure(() => { if (this.RemoveStream != null) this.RemoveStream(val0); }); }
        private void OnDataChannel(string val0) { ErrorOnFailure(() => { if (this.DataChannel != null) this.DataChannel(val0); }); }
        private void OnRenegotiationNeeded(){ ErrorOnFailure(() => { if (this.RenegotiationNeeded != null) this.RenegotiationNeeded(); }); }
        private void OnIceConnectionChange(int val0) { ErrorOnFailure(() => { if (this.IceConnectionChange != null) this.IceConnectionChange(val0); }); }
        private void OnIceGatheringChange(int val0) { ErrorOnFailure(() => { if (this.IceGatheringChange != null) this.IceGatheringChange(val0); }); }
        private void OnIceCandidate(string val0) { ErrorOnFailure(() => { if (this.IceCandidate != null) this.IceCandidate(val0); }); }
        private void OnIceConnectionReceivingChange(bool val0) { ErrorOnFailure(() => { if (this.IceConnectionReceivingChange != null) this.IceConnectionReceivingChange(val0); }); }

        #endregion

        /// <summary>
        /// Fired when a managed error occurs but is triggered by native code
        /// </summary>
        /// <remarks>
        /// This occurs, for example, when an event handler throws, but the event was 
        /// raised by native code
        /// </remarks>
        public event GenericDelegate<Exception>.Handler Error;

        /// <summary>
        /// Executes a chunk of work and emitted an <see cref="Error"/> event on failure
        /// </summary>
        /// <param name="action">chunk of work</param>
        private void ErrorOnFailure(Action action)
        {
            try
            {
                action();
            }
            catch(Exception ex)
            {
                if (this.Error != null)
                {
                    this.Error(ex);
                }
            }
        }

        /// <summary>
        /// Default ctor
        /// </summary>
        public StreamingUnityServerPlugin()
        {
            // set up the command buffer
            EncodeAndTransmitCommand = new CommandBuffer();
            EncodeAndTransmitCommand.name = PluginName + " Encoding";
            EncodeAndTransmitCommand.IssuePluginEvent(Native.GetRenderEventFunc(), 0);

            // create null checking wrappers that we can always call on the native side
            this.pinnedInputUpdate = new GenericDelegate<string>.Handler(this.OnInputUpdate);
            this.pinnedLog = new GenericDelegate<int, string>.Handler(this.OnLog);
            this.pinnedPeerConnect = new GenericDelegate<int, string>.Handler(this.OnPeerConnect);
            this.pinnedPeerDisconnect = new GenericDelegate<int>.Handler(this.OnPeerDisconnect);
            this.pinnedSignIn = new GenericDelegate.Handler(this.OnSignIn);
            this.pinnedDisconnect = new GenericDelegate.Handler(this.OnDisconnect);
            this.pinnedMessageFromPeer = new GenericDelegate<int, string>.Handler(this.OnMessageFromPeer);
            this.pinnedMessageSent = new GenericDelegate<int>.Handler(this.OnMessageSent);
            this.pinnedServerConnectionFailure = new GenericDelegate.Handler(this.OnServerConnectionFailure);
            this.pinnedSignalingChange = new GenericDelegate<int>.Handler(this.OnSignalingChange);
            this.pinnedAddStream = new GenericDelegate<string>.Handler(this.OnAddStream);
            this.pinnedRemoveStream = new GenericDelegate<string>.Handler(this.OnRemoveStream);
            this.pinnedDataChannel = new GenericDelegate<string>.Handler(this.OnDataChannel);
            this.pinnedRenegotiationNeeded = new GenericDelegate.Handler(this.OnRenegotiationNeeded);
            this.pinnedIceConnectionChange = new GenericDelegate<int>.Handler(this.OnIceConnectionChange);
            this.pinnedIceGatheringChange = new GenericDelegate<int>.Handler(this.OnIceGatheringChange);
            this.pinnedIceCandidate = new GenericDelegate<string>.Handler(this.OnIceCandidate);
            this.pinnedIceConnectionReceivingChange = new GenericDelegate<bool>.Handler(this.OnIceConnectionReceivingChange);

            // marshal the callbacks across into native land
            Native.SetCallbackMap(this.pinnedInputUpdate,
                    this.pinnedLog,
                    this.pinnedPeerConnect,
                    this.pinnedPeerDisconnect,
                    this.pinnedSignIn,
                    this.pinnedDisconnect,
                    this.pinnedMessageFromPeer,
                    this.pinnedMessageSent,
                    this.pinnedServerConnectionFailure,
                    this.pinnedSignalingChange,
                    this.pinnedAddStream,
                    this.pinnedRemoveStream,
                    this.pinnedDataChannel,
                    this.pinnedRenegotiationNeeded,
                    this.pinnedIceConnectionChange,
                    this.pinnedIceGatheringChange,
                    this.pinnedIceCandidate,
                    this.pinnedIceConnectionReceivingChange);
        }

        /// <summary>
        /// The <see cref="CommandBuffer"/> needed to encode and transmit frame data
        /// </summary>
        /// <remarks>
        /// This is consumed by <see cref="EncodeAndTransmitFrame"/> to encode the entire rendertexture
        /// and can also be added to a camera command buffer to encode just that cameras view
        /// </remarks>
        /// <example>
        /// var plugin = new StreamingUnityServerPlugin();
        /// Camera.main.AddCommandBuffer(CameraEvent.AfterEverything, plugin.EncodeAndTransmitCommand);
        /// </example>
        public CommandBuffer EncodeAndTransmitCommand
        {
            get;
            private set;
        }

        /// <summary>
        /// Backing field for <see cref="EncodingStereo"/>
        /// </summary>
        private bool encodingStereo = false;

        /// <summary>
        /// Informs the encoder if we are or are not going to be encoding stereo data
        /// </summary>
        public bool EncodingStereo
        {
            get { return encodingStereo; }
            set { Native.SetEncodingStereo(value); encodingStereo = value; }
        }

        /// <summary>
        /// <see cref="Coroutine"/> that encodes and transmits the current frame in it's entirety
        /// </summary>
        /// <returns>iterator for coroutine</returns>
        public IEnumerator EncodeAndTransmitFrame()
        {
            yield return new WaitForEndOfFrame();

            Graphics.ExecuteCommandBuffer(EncodeAndTransmitCommand);
        }
        
        /// <summary>
        /// Connect to a peer identified by a given id
        /// </summary>
        /// <param name="peerId">the peer id</param>
        public void ConnectToPeer(int peerId)
        {
            Native.ConnectToPeer(peerId);
        }

        /// <summary>
        /// Disconnects from the current peer
        /// </summary>
        public void DisconnectFromPeer()
        {
            Native.DisconnectFromPeer();
        }

        #region IDisposable Support

        private bool disposedValue = false; // To detect redundant calls

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    // just shutdown the underlying plugin
                    Native.Close();
                }

                disposedValue = true;
            }
        }

        // This code added to correctly implement the disposable pattern.
        public void Dispose()
        {
            Dispose(true);
        }

        #endregion

        #region GenericDelegate generics

        /// <summary>
        /// Helper to generate delegate signatures with generics
        /// </summary>
        public abstract class GenericDelegate
        {
            public delegate void Handler();
            private GenericDelegate() { }
        }

        /// <summary>
        /// Helper to generate delegate signatures with generics
        /// </summary>
        public abstract class GenericDelegate<TArg0>
        {
            public delegate void Handler(TArg0 arg0);
            private GenericDelegate() { }
        }

        /// <summary>
        /// Helper to generate delegate signatures with generics
        /// </summary>
        public abstract class GenericDelegate<TArg0, TArg1>
        {
            public delegate void Handler(TArg0 arg0, TArg1 arg1);
            private GenericDelegate() { }
        }

        /// <summary>
        /// Helper to generate delegate signatures with generics
        /// </summary>
        public abstract class GenericDelegate<TArg0, TArg1, TArg2>
        {
            public delegate void Handler(TArg0 arg0, TArg1 arg1, TArg2 arg2);
            private GenericDelegate() { }
        }

        /// <summary>
        /// Helper to generate delegate signatures with generics
        /// </summary>
        public abstract class GenericDelegate<TArg0, TArg1, TArg2, TArg3>
        {
            public delegate void Handler(TArg0 arg0, TArg1 arg1, TArg2 arg2, TArg3 arg3);
            private GenericDelegate() { }
        }

        #endregion
    }
}
