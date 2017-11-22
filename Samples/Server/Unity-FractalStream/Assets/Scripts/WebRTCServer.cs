using System;
using System.Linq;
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
    public class WebRTCServer : MonoBehaviour
    {
        /// <summary>
        /// The video frame width.
        /// </summary>
        public int VideoFrameWidth = 1280;

        /// <summary>
        /// The video frame height.
        /// </summary>
        public int VideoFrameHeight = 720;

        /// <summary>
        /// The left eye camera
        /// </summary>
        /// <remarks>
        /// This camera is always needed, even if running in mono
        /// </remarks>
        [Tooltip("The left eye camera, or the only camera in a mono setup")]
        public Camera LeftEye;

        /// <summary>
        /// The right eye camera
        /// </summary>
        /// <remarks>
        /// This camera is only needed if running in stereo
        /// </remarks>
        [Tooltip("The right eye camera, only used in a stereo setup")]
        public Camera RightEye;

        /// <summary>
        /// Indicates the current rendering approach
        /// </summary>
        /// <remarks>
        /// When this is <c>true</c> stereo rendering using <see cref="LeftEye"/> and <see cref="RightEye"/> will be used
        /// When this is <c>false</c> mono rendering using <see cref="LeftEye"/> will be used
        /// </remarks>
        [Tooltip("Flag indicating if we are currently rendering in stereo")]
        public bool IsStereo = true;

        /// <summary>
        /// A mutable peers list that we'll keep updated, and derive connect/disconnect operations from
        /// </summary>
        [Tooltip("A mutable peers list that we'll keep updated, and derive connect/disconnect operations from")]
        public PeerListState PeerList = null;

        /// <summary>
        /// Should we load the native plugin in the editor?
        /// </summary>
        /// <remarks>
        /// This requires webrtcConfig.json and nvEncConfig.json to exist in the unity
        /// application directory (where Unity.exe) lives, and requires a native plugin
        /// for the architecture of the editor (x64 vs x86).
        /// </remarks>
        [Tooltip("Flag indicating if we should load the native plugin in the editor")]
        public bool UseEditorNativePlugin = false;

        /// <summary>
        /// Instance that represents the underlying native plugin that powers the webrtc experience
        /// </summary>
        public StreamingUnityServerPlugin Plugin = null;

        /// <summary>
        /// Stereo render texture for left and right cameras.
        /// </summary>
        private RenderTexture stereoRenderTexture;

        /// <summary>
        /// Render texture for left camera.
        /// </summary>
        private RenderTexture renderTexture;

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
        /// Internal tracking id for the peer we're trying to connect to for video/data
        /// </summary>
        /// <remarks>
        /// We use this to know who is connected on <see cref="StreamingUnityServerPlugin.AddStream"/>
        /// </remarks>
        private int? offerPeer = null;

        /// <summary>
        /// Internal tracking bool for indicating if the peer offer succeeded
        /// </summary>
        private bool offerSucceeded = false;

        /// <summary>
        /// Flag indicating if we were stereo on the last <see cref="SetupActiveEyes"/> call
        /// </summary>
        private bool? lastSetupVisibleEyes = null;

        /// <summary>
        /// The last prediction timestamp.
        /// </summary>
        private long lastTimestamp = -1;

        /// <summary>
        /// Flag indicating if the camera needs to be updated.
        /// </summary>
        private bool cameraNeedUpdated = false;

        /// <summary>
        /// Unity engine object Awake() hook
        /// </summary>
        private void Awake()
        {
            // Make sure that the render window continues to render when the game window 
            // does not have focus
            Application.runInBackground = true;
            //Application.targetFrameRate = 60;

            // Set screen resolution.
            Screen.SetResolution(VideoFrameWidth, VideoFrameHeight, false);

            // Initializes render textures.
            InitializeRenderTextures();

            // Open the connection.
            Open();
            
            // Setup the eyes for the first time.
            SetupActiveEyes();

            // Initializes the buffer renderer using render texture.
            StartCoroutine(Plugin.InitializeBufferRenderer(LeftEye.targetTexture.GetNativeTexturePtr()));
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
                if (!IsStereo)
                {
                    transform.position = location;
                    transform.LookAt(lookAt, up);
                }
                else
                {
                    if (cameraNeedUpdated)
                    {
                        // Invert Y-axis for rendering to texture.
                        // Invert Z-axis for depth test.
                        Matrix4x4 invertMatrix = Matrix4x4.Scale(new Vector3(1, -1, -1));
                        this.LeftEye.projectionMatrix = this.LeftEye.worldToCameraMatrix * invertMatrix * stereoLeftProjection;
                        this.RightEye.projectionMatrix = this.RightEye.worldToCameraMatrix * invertMatrix * stereoRightProjection;
                        this.LeftEye.Render();
                        this.RightEye.Render();
                        Plugin.LockFrameBuffer(lastTimestamp);
                        cameraNeedUpdated = false;
                    }
                }
            }

            // if the eye config changes, reconfigure
            if (this.lastSetupVisibleEyes.HasValue &&
                this.lastSetupVisibleEyes.Value != this.IsStereo)
            {
                SetupActiveEyes();
            }

            // check if we're in the editor, and fail out if we aren't loading the plugin in editor
            if (Application.isEditor && !UseEditorNativePlugin)
            {
                return;
            }

            // if we got an offer, track that we're connected to them
            // in a way that won't trip our connection logic (we don't
            // want to accidently make them an offer, just an answer)
            if (offerPeer.HasValue && offerSucceeded)
            {
                previousPeer = PeerList.Peers.First(p => p.Id == offerPeer.Value);
                PeerList.SelectedPeer = previousPeer;
            }

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
            Plugin.InputUpdate += OnInputData;

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

            // when we get a message, check if it's an offer and if it is track who it's
            // from so that we can use it to determine who we're connected to in AddStream
            Plugin.MessageFromPeer += (int peer, string message) =>
            {
                try
                {
                    var msg = SimpleJSON.JSON.Parse(message);

                    if (!msg["sdp"].IsNull && msg["type"].Value == "offer")
                    {
                        offerPeer = peer;
                    }
                }
                catch (Exception)
                {
                    // swallow
                }
            };

            // when we add a stream successfully, track that
            Plugin.AddStream += (string streamLabel) =>
            {
                offerSucceeded = true;
            };

            // when we remove a stream, track that
            Plugin.RemoveStream += (string streamLabel) =>
            {
                offerSucceeded = false;
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
        /// Initializes render textures.
        /// </summary>
        private void InitializeRenderTextures()
        {
            // Creates the stereo render texture.
            stereoRenderTexture = new RenderTexture(
                VideoFrameWidth * 2, VideoFrameHeight, 24, RenderTextureFormat.ARGB32);

            stereoRenderTexture.Create();

            // Creates the mono render texture.
            renderTexture = new RenderTexture(
                VideoFrameWidth, VideoFrameHeight, 24, RenderTextureFormat.ARGB32);

            renderTexture.Create();
        }

        /// <summary>
        /// Setup camera viewport and target texture.
        /// </summary>
        private void SetupCameras()
        {
            if (IsStereo)
            {
                // Viewport
                LeftEye.rect = new Rect(0, 0, 0.5f, 1);
                RightEye.rect = new Rect(0.5f, 0, 0.5f, 1);

                // Target texture
                LeftEye.targetTexture = stereoRenderTexture;
                RightEye.targetTexture = stereoRenderTexture;
            }
            else
            {
                // Viewport
                LeftEye.rect = new Rect(0, 0, 1, 1);

                // Target texture
                LeftEye.targetTexture = renderTexture;
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
                    case "stereo-rendering":
                        // note: 1 represents true, 0 represents false
                        int isStereo;

                        // if it's not a valid bool, don't continue
                        if (!int.TryParse(node["body"].Value, out isStereo))
                        {
                            break;
                        }

                        // the visible eyes value needs to be set to Two only when isStereo is true (value of 1)
                        // and the total eyes known by the scene is two (meaning we have a second camera available)
                        this.IsStereo = (isStereo == 1 && this.RightEye != null) ? true : false;

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
                        if (!cameraNeedUpdated && camera != null && camera.Length > 0)
                        {
                            var coords = camera.Split(',');

                            // Parse the prediction timestamp from the message body.
                            long timestamp = long.Parse(coords[32]);

                            if (timestamp != lastTimestamp)
                            {
                                var index = 0;
                                for (int i = 0; i < 4; i++)
                                {
                                    for (int j = 0; j < 4; j++)
                                    {
                                        // We are receiving 32 values for the left/right matrix.
                                        // The first 16 are for left followed by the right matrix.
                                        stereoRightProjection[i, j] = float.Parse(coords[16 + index]);
                                        stereoLeftProjection[i, j] = float.Parse(coords[index++]);
                                    }
                                }

                                lastTimestamp = timestamp;
                                cameraNeedUpdated = true;
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

        /// <summary>
        /// Setup eye cameras based on number of eyes
        /// </summary>
        private void SetupActiveEyes()
        {
            if (this.IsStereo)
            {
                this.LeftEye.stereoTargetEye = StereoTargetEyeMask.Left;
                this.LeftEye.enabled = false;
                this.RightEye.enabled = false;
            }
            else
            {
                // none means use the eye like a single camera, which is what we want here
                this.LeftEye.stereoTargetEye = StereoTargetEyeMask.None;
                this.LeftEye.enabled = true;
                this.RightEye.enabled = false;
            }

            // update our last setup visible eye value
            this.lastSetupVisibleEyes = this.IsStereo;

            // notify the plugin of the potential stereo change as well
            if (this.Plugin != null)
            {
                this.Plugin.EncodingStereo = this.IsStereo;
            }

            // Setup camera.
            SetupCameras();
        }
    }
}