using Microsoft.Toolkit.ThreeD;
using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

namespace Microsoft.Toolkit.ThreeD
{

    /// <summary>
    /// WebRTC server behavior that enables 3dtoolkit webrtc
    /// </summary>
    /// <remarks>
    /// Adding this component requires an <c>nvEncConfig.json</c> and <c>webrtcConfig.json</c> in the run directory
    /// </remarks>
    [RequireComponent(typeof(Camera))]
    public class WebRTCServer : MonoBehaviour
    {
        /// <summary>
        /// Internal instance that maintains the native lifecycle and communication
        /// </summary>
        private StreamingUnityServerPlugin plugin = null;

        /// <summary>
        /// The location to be placed at as determined by client control
        /// </summary>
        private Vector3 location = Vector3.zero;

        /// <summary>
        /// The location to look at as determined by client control
        /// </summary>
        private Vector3 lookAt = Vector3.zero;

        /// <summary>
        /// The upward vector as determined by client control
        /// </summary>
        private Vector3 up = Vector3.zero;

        /// <summary>
        /// Internal flag used to indicate we are shutting down
        /// </summary>
        private bool isClosing = false;
        
        /// <summary>
        /// Unity engine object Start() hook
        /// </summary>
        private void Start()
        {
            // make sure that the render window continues to render when the game window does not have focus
            Application.runInBackground = true;

            // open the connection
            Open();
        }

        /// <summary>
        /// Unity engine object OnDestroy() hook
        /// </summary>
        private void OnDestroy()
        {
            // close the connection
            Close();
        }

        /// <summary>
        /// Unity engine object Update() hook
        /// </summary>
        private void Update()
        {
            if (!isClosing)
            {
                transform.position = location;
                transform.LookAt(lookAt, up);
            }
        }

        /// <summary>
        /// Opens the webrtc server and gets things rolling
        /// </summary>
        public void Open()
        {
            if (plugin != null)
            {
                Close();
            }

            // Create the plugin
            plugin = new StreamingUnityServerPlugin(this.GetComponent<Camera>());

            // hook it's events
            plugin.Input += OnInputData;
            plugin.Log += OnLogData;
        }

        /// <summary>
        /// Closes the webrtc server and shuts things down
        /// </summary>
        public void Close()
        {
            if (!isClosing && plugin != null)
            {
                plugin.Dispose();
                plugin = null;
            }
        }

        /// <summary>
        /// Handles input data and updates local transformations
        /// </summary>
        /// <param name="data">input data</param>
        private void OnInputData(string data)
        {
            try
            {
                var node = SimpleJSON.JSON.Parse(data);
                string messageType = node["type"];

                switch (messageType)
                {
                    case "keyboard-event":
                        var kbBody = node["body"];
                        int kbMsg = kbBody["msg"];
                        int kbWParam = kbBody["wParam"];

                        break;

                    case "mouse-event":
                        var mouseBody = node["body"];
                        int mouseMsg = mouseBody["msg"];
                        int mouseWParam = mouseBody["wParam"];
                        int mouseLParam = mouseBody["lParam"];

                        break;

                    case "camera-transform-lookat":
                        string cam = node["body"];
                        if (cam != null && cam.Length > 0)
                        {
                            string[] sp = cam.Split(new char[] { ' ', ',' }, StringSplitOptions.RemoveEmptyEntries);

                            Vector3 loc = new Vector3();
                            loc.x = float.Parse(sp[0]);
                            loc.y = float.Parse(sp[1]);
                            loc.z = float.Parse(sp[2]);
                            location = loc;

                            Vector3 look = new Vector3();
                            look.x = float.Parse(sp[3]);
                            look.y = float.Parse(sp[4]);
                            look.z = float.Parse(sp[5]);
                            lookAt = look;

                            Vector3 u = new Vector3();
                            u.x = float.Parse(sp[6]);
                            u.y = float.Parse(sp[7]);
                            u.z = float.Parse(sp[8]);
                            up = u;
                        }

                        break;
                }
            }
            catch (Exception ex)
            {
                Debug.LogWarning("InputUpdate threw " + ex.Message + "\r\n" + ex.StackTrace);
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