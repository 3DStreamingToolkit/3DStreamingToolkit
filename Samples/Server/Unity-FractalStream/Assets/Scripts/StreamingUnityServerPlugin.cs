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
            public static extern void SetResolution(int width, int height);
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
            public static extern void SetCallbackMap(GenericDelegate<string>.Handler inputFunc,
            GenericDelegate<int, string>.Handler logFunc,
            GenericDelegate<int, string>.Handler peerConnectFunc,
            GenericDelegate<int>.Handler peerDisconnectFunc);
#endif
        }

        #endregion
        
        /// <summary>
        /// Emitted when input is received
        /// </summary>
        public event GenericDelegate<string>.Handler Input;

        /// <summary>
        /// Emitted when log data is received
        /// </summary>
        public event GenericDelegate<int, string>.Handler Log;

        /// <summary>
        /// Emitted when a peer connects
        /// </summary>
        public event GenericDelegate<int, string>.Handler PeerConnect;

        /// <summary>
        /// Emitted when a peer disconnects
        /// </summary>
        public event GenericDelegate<int>.Handler PeerDisconnect;
        
        /// <summary>
        /// log delegate tied to the lifetime of this instance
        /// </summary>
        private GenericDelegate<int, string>.Handler pinnedLogDelegate;

        /// <summary>
        /// input delegate tied to the lifetime of this instance
        /// </summary>
        private GenericDelegate<string>.Handler pinnedInputDelegate;

        /// <summary>
        /// peer connect delegate tied to the lifetime of this instance
        /// </summary>
        private GenericDelegate<int, string>.Handler pinnedPeerConnectDelegate;

        /// <summary>
        /// peer disconnect delegate tied to the lifetime of this instance
        /// </summary>
        private GenericDelegate<int>.Handler pinnedPeerDisconnectDelegate;

        /// <summary>
        /// Default ctor
        /// </summary>
        public StreamingUnityServerPlugin()
        {
            // set up the command buffer
            EncodeAndTransmitCommand = new CommandBuffer();
            EncodeAndTransmitCommand.name = PluginName + " Encoding";
            EncodeAndTransmitCommand.IssuePluginEvent(Native.GetRenderEventFunc(), 0);

            this.pinnedInputDelegate = new GenericDelegate<string>.Handler(OnInput);
            this.pinnedLogDelegate = new GenericDelegate<int, string>.Handler(OnLog);
            this.pinnedPeerConnectDelegate = new GenericDelegate<int, string>.Handler(OnPeerConnect);
            this.pinnedPeerDisconnectDelegate = new GenericDelegate<int>.Handler(OnPeerDisconnect);
            
            // marshal the callbacks across into native land
            Native.SetCallbackMap(this.pinnedInputDelegate,
                this.pinnedLogDelegate,
                this.pinnedPeerConnectDelegate,
                this.pinnedPeerDisconnectDelegate);

            // TODO(bengreenier): do we want this optimization? see native
            //SetResolution(Screen.width, Screen.height);
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

        /// <summary>
        /// Called by NativeCode when input is received
        /// </summary>
        /// <param name="inputData">input data</param>
        void OnInput(string inputData)
        {
            if (this.Input != null)
            {
                this.Input(inputData);
            }
        }

        /// <summary>
        /// Called by NativeCode when log data is received
        /// </summary>
        /// <param name="logLevel">log level</param>
        /// <param name="logData">log data</param>
        void OnLog(int logLevel, string logData)
        {
            if (this.Log != null)
            {
                this.Log(logLevel, logData);
            }
        }

        /// <summary>
        /// Called by NativeCode when a peer is connected
        /// </summary>
        /// <param name="peerId">the peer id</param>
        /// <param name="peerName">the peer name</param>
        void OnPeerConnect(int peerId, string peerName)
        {
            if (this.PeerConnect != null)
            {
                this.PeerConnect(peerId, peerName);
            }
        }

        /// <summary>
        /// Called by NativeCode when a peer is disconnected
        /// </summary>
        /// <param name="peerId">the peer id</param>
        void OnPeerDisconnect(int peerId)
        {
            if (this.PeerDisconnect != null)
            {
                this.PeerDisconnect(peerId);
            }
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
