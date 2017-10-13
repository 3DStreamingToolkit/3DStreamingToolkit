using System;
using System.Collections;
using System.Collections.Generic;
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

        /// <summary>
        /// A representation of the marshalled function that native code calls when input is received
        /// </summary>
        /// <param name="inputData">the input message data</param>
        public delegate void InputHandlerDelegate(string inputData);

        /// <summary>
        /// A representation of the marshalled function that native code calls when a debug message is logged
        /// </summary>
        /// <param name="logLevel">the severity at which the message occurs</param>
        /// <param name="logData">the data of the message</param>
        public delegate void LogHandlerDelegate(int logLevel, string logData);

        #region Dll Imports

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport ("__Internal")]
#else
        [DllImport(PluginName)]
#endif
        private static extern IntPtr GetRenderEventFunc();

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport ("__Internal")]
#else
        [DllImport(PluginName)]
#endif
        private static extern void SetInputDataCallback(IntPtr cb);

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport ("__Internal")]
#else
        [DllImport(PluginName)]
#endif
        private static extern void SetLogCallback(IntPtr cb);

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport ("__Internal")]
#else
        [DllImport(PluginName)]
        private static extern void Close();
#endif

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport ("__Internal")]
#else
        [DllImport(PluginName)]
        private static extern void SetResolution(int width, int height);
#endif

        #endregion

        /// <summary>
        /// Emitted when input is received
        /// </summary>
        public event InputHandlerDelegate Input;

        /// <summary>
        /// Emitted when log data is received
        /// </summary>
        public event LogHandlerDelegate Log;

        /// <summary>
        /// Instance member delegate that represents <see cref="OnInput(string)"/>
        /// so that the GC won't clean up the delegate until this instance is nuked
        /// </summary>
        private InputHandlerDelegate managedInputDelegate;

        /// <summary>
        /// Instance member delegate that represents <see cref="OnLog(int, string)"/>
        /// so that the GC won't clean up the delegate until this instance is nuked
        /// </summary>
        private LogHandlerDelegate managedLogDelegate;

        /// <summary>
        /// Default ctor
        /// </summary>
        public StreamingUnityServerPlugin()
        {
            // set up the command buffer
            EncodeAndTransmitCommand = new CommandBuffer();
            EncodeAndTransmitCommand.name = PluginName + " Encoding";
            EncodeAndTransmitCommand.IssuePluginEvent(GetRenderEventFunc(), 0);

            // we need to set up the scoped delegates
            this.managedInputDelegate = new InputHandlerDelegate(OnInput);
            this.managedLogDelegate = new LogHandlerDelegate(OnLog);

            // and then we can proxy them across to native land
            SetInputDataCallback(Marshal.GetFunctionPointerForDelegate(this.managedInputDelegate));
            SetLogCallback(Marshal.GetFunctionPointerForDelegate(this.managedLogDelegate));

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

        #region IDisposable Support

        private bool disposedValue = false; // To detect redundant calls

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    // just shutdown the underlying plugin
                    Close();
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
    }
}
