using System;
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
        /// <param name="camera">the camera to hook on</param>
        public StreamingUnityServerPlugin(Camera camera)
        {
            // we need to set up the scoped delegates
            this.managedInputDelegate = new InputHandlerDelegate(OnInput);
            this.managedLogDelegate = new LogHandlerDelegate(OnLog);

            // and then we can proxy them across to native land
            SetInputDataCallback(Marshal.GetFunctionPointerForDelegate(this.managedInputDelegate));
            SetLogCallback(Marshal.GetFunctionPointerForDelegate(this.managedLogDelegate));

            // configure a command to issue an event to the render event function
            CommandBuffer cmb = new CommandBuffer();
            cmb.name = PluginName + " Encoding";
            cmb.IssuePluginEvent(GetRenderEventFunc(), 0);

            // add the command to the camera so it's issued after all camera rendering occurs
            Camera.main.AddCommandBuffer(CameraEvent.AfterEverything, cmb);
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
