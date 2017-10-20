using System;
using System.Threading;
using UnityEngine;

namespace Microsoft.Toolkit.ThreeD
{
    /// <summary>
    /// Behavior to capture and log <see cref="WebRTCServer"/> debug information based on command line arguments
    /// </summary>
    /// <remarks>
    /// This includes debug information from the underlying <see cref="StreamingUnityServerPlugin"/>
    /// </remarks>
    [RequireComponent(typeof(WebRTCServer))]
    public class WebRTCServerDebug : MonoBehaviour
    {
        /// <summary>
        /// Indicates if we should log plugin info
        /// </summary>
        private bool shouldLog = false;
        
        /// <summary>
        /// Unity engine object Awake() hook
        /// </summary>
        private void Awake()
        {
            var args = Environment.GetCommandLineArgs();
            for (var i = 0;  i < args.Length; i++)
            {
                // support -webrtc-debug-log to enable detailed logging
                if (args[i].ToLower() == "-webrtc-debug-log")
                {
                    shouldLog = true;
                }
                // support -webrtc-debug-sleep [intervalMs] to enable sleep on start, default intervalMs = 5000
                else if (args[i].ToLower() == "-webrtc-debug-sleep")
                {
                    int sleepVal;
                    if (args.Length <= i + 1 || !int.TryParse(args[i + 1], out sleepVal))
                    {
                        sleepVal = 5000;
                    }

                    // sleep the main thread
                    Thread.Sleep(sleepVal);
                }
            }
        }

        /// <summary>
        /// Unity engine object Start() hook
        /// </summary>
        private void Start()
        {
            var plugin = this.GetComponent<WebRTCServer>().Plugin;

            // hook plugin internals if plugin exists
            if (plugin != null)
            {
                plugin.Error += OnError;

                if (shouldLog)
                {
                    plugin.Log += OnLogData;
                }
            }
        }

        /// <summary>
        /// Handles error data and writes it to the debug console
        /// </summary>
        /// <param name="ex">plugin exception data</param>
        private void OnError(Exception ex)
        {
            Debug.LogError(ex.Message + ":" + ex.StackTrace);
        }

        /// <summary>
        /// Handles log data and writes it to the debug console
        /// </summary>
        /// <param name="logLevel">severity level</param>
        /// <param name="logData">log data</param>
        private void OnLogData(int logLevel, string logData)
        {
            Debug.Log("[" + logLevel + "]: " + logData);
        }
    }
}
