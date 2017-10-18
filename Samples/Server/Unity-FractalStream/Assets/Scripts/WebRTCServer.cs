using System;
using UnityEngine;

namespace Microsoft.Toolkit.ThreeD
{
    /// <summary>
    /// WebRTC server behavior that enables 3dtoolkit webrtc
    /// </summary>
    /// <remarks>
    /// Adding this component requires an <c>nvEncConfig.json</c> and <c>webrtcConfig.json</c> in the run directory
    /// To see debug information from this component, add a <see cref="WebRTCServerDebug"/> to the same object
    /// </remarks>
    [RequireComponent(typeof(WebRTCEyes))]
    public class WebRTCServer : MonoBehaviour
    {
        /// <summary>
        /// A mutable peers list that we'll keep updated, and derive connect/disconnect operations from
        /// </summary>
        public PeerListState PeerList = null;

        /// <summary>
        /// Should we load the native plugin in the editor?
        /// </summary>
        /// <remarks>
        /// This requires webrtcConfig.json and nvEncConfig.json to exist in the unity
        /// application directory (where Unity.exe) lives, and requires a native plugin
        /// for the architecture of the editor (x64 vs x86).
        /// </remarks>
        public bool UseEditorNativePlugin = false;

        /// <summary>
        /// Instance that represents the underlying native plugin that powers the webrtc experience
        /// </summary>
        public StreamingUnityServerPlugin Plugin = null;

        /// <summary>
        /// Internal reference to the eyes component
        /// </summary>
        /// <remarks>
        /// Since <see cref="WebRTCEyes"/> is a required component, we just set this value in <see cref="Awake"/>
        /// </remarks>
        private WebRTCEyes Eyes = null;

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
        /// The left stereo eye projection as determined by client control
        /// </summary>
        private Matrix4x4 stereoLeftProjection = Matrix4x4.identity;

        /// <summary>
        /// The right stereo eye projection as determined by client control
        /// </summary>
        private Matrix4x4 stereoRightProjection = Matrix4x4.identity;

        /// <summary>
        /// Internal flag used to indicate we are shutting down
        /// </summary>
        private bool isClosing = false;

        /// <summary>
        /// Internal reference to the previous peer
        /// </summary>
        private PeerListState.Peer previousPeer = null;

        /// <summary>
        /// Unity engine object Awake() hook
        /// </summary>
        private void Awake()
        {
            // make sure that the render window continues to render when the game window does not have focus
            Application.runInBackground = true;

            // capture the attached eyes into a member variable
            Eyes = this.GetComponent<WebRTCEyes>();

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

                // apply stereo projection if needed
                if (this.Eyes.TotalEyes == WebRTCEyes.EyeCount.Two &&
                    !stereoLeftProjection.isIdentity &&
                    !stereoRightProjection.isIdentity)
                {
                    this.Eyes.EyeOne.SetStereoProjectionMatrix(Camera.StereoscopicEye.Left, stereoLeftProjection * this.Eyes.EyeOne.worldToCameraMatrix);
                    this.Eyes.EyeTwo.SetStereoProjectionMatrix(Camera.StereoscopicEye.Right, stereoRightProjection * this.Eyes.EyeOne.worldToCameraMatrix);
                }
            }

            // check if we're in the editor, and fail out if we aren't loading the plugin in editor
            if (Application.isEditor && !UseEditorNativePlugin)
            {
                return;
            }

            // encode the entire render texture at the end of the frame
            StartCoroutine(Plugin.EncodeAndTransmitFrame());
            
            // check if we need to connect to a peer, and if so, do it
            if ((previousPeer == null && PeerList.SelectedPeer != null) ||
                (previousPeer != null && PeerList.SelectedPeer != null && !previousPeer.Equals(PeerList.SelectedPeer)))
            {
                Plugin.ConnectToPeer(PeerList.SelectedPeer.Id);
                previousPeer = PeerList.SelectedPeer;
            }
        }

        /// <summary>
        /// Opens the webrtc server and gets things rolling
        /// </summary>
        public void Open()
        {
            if (Plugin != null)
            {
                Close();
            }

            // clear the underlying mutable peer data
            PeerList.Peers.Clear();
            PeerList.SelectedPeer = null;

            // check if we're in the editor, and fail out if we aren't loading the plugin in editor
            if (Application.isEditor && !UseEditorNativePlugin)
            {
                return;
            }

            // Create the plugin
            Plugin = new StreamingUnityServerPlugin();

            // hook it's input event
            // note: if you wish to capture debug data, see the <see cref="StreamingUnityServerDebug"/> behaviour
            Plugin.Input += OnInputData;

            Plugin.PeerConnect += (int peerId, string peerName) =>
            {
                PeerList.Peers.Add(new PeerListState.Peer()
                {
                    Id = peerId,
                    Name = peerName
                });
            };

            Plugin.PeerDisconnect += (int peerId) =>
            {
                // TODO(bengreenier): this can be optimized to stop at the first match
                PeerList.Peers.RemoveAll(p => p.Id == peerId);
            };
        }

        /// <summary>
        /// Closes the webrtc server and shuts things down
        /// </summary>
        public void Close()
        {
            if (!isClosing && Plugin != null)
            {
                Plugin.Dispose();
                Plugin = null;
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

                Debug.Log(data);

                switch (messageType)
                {
                    case "stereo-rendering":
                        // note: 1 represents true, 0 represents false
                        int isStereo;

                        // if it's not a valid bool, don't continue
                        if (!int.TryParse(node["body"].Value, out isStereo))
                        {
                            break;
                        }

                        Debug.Log("iss:" + isStereo);

                        // the visible eyes value needs to be set to Two only when isStereo is true (value of 1)
                        // and the total eyes known by the scene is two (meaning we have a second camera available)
                        this.Eyes.VisibleEyes = (isStereo == 1 && this.Eyes.TotalEyes == WebRTCEyes.EyeCount.Two) ?
                            WebRTCEyes.EyeCount.Two :
                            WebRTCEyes.EyeCount.One;

                        break;

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

                    case "camera-transform-stereo":

                        string camera = node["body"];
                        if (camera != null && camera.Length > 0)
                        {
                            var coords = camera.Split(',');
                            var index = 0;
                            for (int i = 0; i < 4; i++)
                            {
                                for (int j = 0; j < 4; j++)
                                {
                                    // We are receiving 32 values for the left/right matrix.
                                    // The first 16 are for left followed by the right matrix.
                                    stereoRightProjection[i, j] = float.Parse(coords[16+index]);
                                    stereoLeftProjection[i, j] = float.Parse(coords[index++]);
                                }
                            }
                        }
                        
                        break;
                }
            }
            catch (Exception ex)
            {
                Debug.LogWarning("InputUpdate threw " + ex.Message + "\r\n" + ex.StackTrace);
            }
        }
    }
}