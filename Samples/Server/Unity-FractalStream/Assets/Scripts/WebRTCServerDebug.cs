using UnityEngine;

namespace Microsoft.Toolkit.ThreeD
{
    /// <summary>
    /// Behavior to capture and log <see cref="WebRTCServer"/> debug information
    /// </summary>
    /// <remarks>
    /// This includes debug information from the underlying <see cref="StreamingUnityServerPlugin"/>
    /// </remarks>
    [RequireComponent(typeof(WebRTCServer))]
    public class WebRTCServerDebug : MonoBehaviour
    {
        /// <summary>
        /// Unity engine object Start() hook
        /// </summary>
        private void Start()
        {
            var plugin = this.GetComponent<WebRTCServer>().Plugin;

            // if it has a plugin, then attach to it's log event
            if (plugin != null)
            {
                plugin.Log += OnLogData;
            }
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
