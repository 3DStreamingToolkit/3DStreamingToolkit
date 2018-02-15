using SimpleJSON;
using System;
using System.Collections;
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
        /// If this flag is set to true, the target texture of the camera is generated
        /// automatically using <see cref="InitializeRenderTextures"/>. Otherwise, the
        /// target texture must be created and assigned manually in Editor using 
        /// <see cref="RenderTexture"/>
        /// </summary>
        public bool GenerateTargetTexture = true;

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
        public bool IsStereo = false;

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
        /// Render texture for the left camera.
        /// </summary>
        private RenderTexture leftRenderTexture;

        /// <summary>
        /// Render texture for the right camera.
        /// </summary>
        private RenderTexture rightRenderTexture;

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
        private Matrix4x4 stereoLeftProjectionMatrix = Matrix4x4.identity;

        /// <summary>
        /// The left stereo eye view as determined by client control
        /// </summary>
        private Matrix4x4 stereoLeftViewMatrix = Matrix4x4.identity;

        /// <summary>
        /// The right stereo eye projection as determined by client control
        /// </summary>
        private Matrix4x4 stereoRightProjectionMatrix = Matrix4x4.identity;

        /// <summary>
        /// The right stereo eye view as determined by client control
        /// </summary>
        private Matrix4x4 stereoRightViewMatrix = Matrix4x4.identity;

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
            Application.targetFrameRate = 60;

            // Initializes render textures if needed.
            if (GenerateTargetTexture)
            {
                InitializeRenderTextures();
            }

            // Open the connection.
            Open();

            // Setup the eyes for the first time.
            SetupActiveEyes();

            // Validates target texture of camera.
            if (LeftEye.targetTexture == null || LeftEye.targetTexture.format != RenderTextureFormat.ARGB32 ||
                RightEye.targetTexture == null || RightEye.targetTexture.format != RenderTextureFormat.ARGB32)
            {
                throw new ArgumentException("Invalid target texture of camera.");
            }
            else
            {
                // Initializes the buffer renderer using render texture.
                StartCoroutine(Plugin.InitializeBufferCapturer(
                    LeftEye.targetTexture.GetNativeTexturePtr(),
                    RightEye.targetTexture.GetNativeTexturePtr()));
            }
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
                    LeftEye.transform.position = location;
                    LeftEye.transform.LookAt(lookAt, up);
                    this.LeftEye.Render();
                    StartCoroutine(SendFrame(false));
                }
                else if (cameraNeedUpdated)
                {
                    // Updates left and right projection matrices.
                    this.LeftEye.projectionMatrix = stereoLeftProjectionMatrix;
                    this.RightEye.projectionMatrix = stereoRightProjectionMatrix;

                    // Updates camera transform's position and rotation.
                    // Converts from right-handed to left-handed coordinates.
                    stereoLeftViewMatrix = Matrix4x4.Inverse(stereoLeftViewMatrix);
                    stereoRightViewMatrix = Matrix4x4.Inverse(stereoRightViewMatrix);

                    this.LeftEye.transform.position = new Vector3(
                        stereoLeftViewMatrix.m03,
                        stereoLeftViewMatrix.m13,
                        -stereoLeftViewMatrix.m23);

                    this.RightEye.transform.position = new Vector3(
                        stereoRightViewMatrix.m03,
                        stereoRightViewMatrix.m13,
                        -stereoRightViewMatrix.m23);

                    Quaternion leftQ = QuaternionFromMatrix(stereoLeftViewMatrix);
                    this.LeftEye.transform.rotation = new Quaternion(
                        -leftQ.x, -leftQ.y, leftQ.z, leftQ.w);

                    Quaternion rightQ = QuaternionFromMatrix(stereoRightViewMatrix);
                    this.RightEye.transform.rotation = new Quaternion(
                        -rightQ.x, -rightQ.y, rightQ.z, rightQ.w);

                    // Manually render to textures.
                    this.LeftEye.Render();
                    this.RightEye.Render();

                    // Starts sending frame.
                    StartCoroutine(SendFrame(true));
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
            // Left.
            leftRenderTexture = new RenderTexture(
                VideoFrameWidth, VideoFrameHeight, 24, RenderTextureFormat.ARGB32);

            leftRenderTexture.Create();
            LeftEye.targetTexture = leftRenderTexture;

            // Right
            rightRenderTexture = new RenderTexture(
                VideoFrameWidth, VideoFrameHeight, 24, RenderTextureFormat.ARGB32);

            rightRenderTexture.Create();
            RightEye.targetTexture = rightRenderTexture;
        }

        /// <summary>
        /// Handles input data and updates local transformations
        /// </summary>
        /// <param name="data">input data</param>
        private void OnInputData(string data)
        {
            try
            {
                JSONNode node = SimpleJSON.JSON.Parse(data);
                string messageType = node["type"];
                string camera = "";

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
                        JSONNode kbBody = node["body"];
                        int kbMsg = kbBody["msg"];
                        int kbWParam = kbBody["wParam"];

                        break;

                    case "mouse-event":
                        JSONNode mouseBody = node["body"];
                        int mouseMsg = mouseBody["msg"];
                        int mouseWParam = mouseBody["wParam"];
                        int mouseLParam = mouseBody["lParam"];

                        break;

                    case "camera-transform-lookat":
                        camera = node["body"];
                        if (camera != null && camera.Length > 0)
                        {
                            string[] sp = camera.Split(new char[] { ' ', ',' }, StringSplitOptions.RemoveEmptyEntries);

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
                        camera = node["body"];
                        if (!cameraNeedUpdated && camera != null && camera.Length > 0)
                        {
                            string[] coords = camera.Split(',');
                            int index = 0;
                            for (int i = 0; i < 4; i++)
                            {
                                for (int j = 0; j < 4; j++)
                                {
                                    stereoRightViewMatrix[i, j] = float.Parse(coords[48 + index]);
                                    stereoRightProjectionMatrix[i, j] = float.Parse(coords[32 + index]);
                                    stereoLeftViewMatrix[i, j] = float.Parse(coords[16 + index]);
                                    stereoLeftProjectionMatrix[i, j] = float.Parse(coords[index++]);
                                }
                            }

                            cameraNeedUpdated = true;
                        }

                        break;

                    case "camera-transform-stereo-prediction":
                        camera = node["body"];
                        if (!cameraNeedUpdated && camera != null && camera.Length > 0)
                        {
                            string[] coords = camera.Split(',');

                            // Parse the prediction timestamp from the message body.
                            long timestamp = long.Parse(coords[64]);

                            if (timestamp != lastTimestamp)
                            {
                                lastTimestamp = timestamp;

                                int index = 0;
                                for (int i = 0; i < 4; i++)
                                {
                                    for (int j = 0; j < 4; j++)
                                    {
                                        stereoRightViewMatrix[i, j] = float.Parse(coords[48 + index]);
                                        stereoRightProjectionMatrix[i, j] = float.Parse(coords[32 + index]);
                                        stereoLeftViewMatrix[i, j] = float.Parse(coords[16 + index]);
                                        stereoLeftProjectionMatrix[i, j] = float.Parse(coords[index++]);
                                    }
                                }

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
        /// Setup eye cameras based on number of eyes.
        /// Cameras are disabled as we we take control of render order ourselves, see
        /// https://docs.unity3d.com/ScriptReference/Camera.Render.html for more info
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
                this.LeftEye.enabled = false;
                this.RightEye.enabled = false;
            }

            // update our last setup visible eye value
            this.lastSetupVisibleEyes = this.IsStereo;

            // notify the plugin of the potential stereo change as well
            if (this.Plugin != null)
            {
                this.Plugin.EncodingStereo = this.IsStereo;
            }
        }

        /// <summary>
        /// Sends frame buffer.
        /// </summary>
        private IEnumerator SendFrame(bool isStereo)
        {
            yield return Plugin.SendFrame(isStereo, lastTimestamp);
            cameraNeedUpdated = false;
        }

        /// <summary>
        /// From: https://answers.unity.com/questions/11363/converting-matrix4x4-to-quaternion-vector3.html
        /// </summary>
        public static Quaternion QuaternionFromMatrix(Matrix4x4 m)
        {
            Quaternion q = new Quaternion();
            q.w = Mathf.Sqrt(Mathf.Max(0, 1 + m[0, 0] + m[1, 1] + m[2, 2])) / 2;
            q.x = Mathf.Sqrt(Mathf.Max(0, 1 + m[0, 0] - m[1, 1] - m[2, 2])) / 2;
            q.y = Mathf.Sqrt(Mathf.Max(0, 1 - m[0, 0] + m[1, 1] - m[2, 2])) / 2;
            q.z = Mathf.Sqrt(Mathf.Max(0, 1 - m[0, 0] - m[1, 1] + m[2, 2])) / 2;
            q.x *= Mathf.Sign(q.x * (m[2, 1] - m[1, 2]));
            q.y *= Mathf.Sign(q.y * (m[0, 2] - m[2, 0]));
            q.z *= Mathf.Sign(q.z * (m[1, 0] - m[0, 1]));
            return q;
        }
    }
}
