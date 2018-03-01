using SimpleJSON;
using System;
using System.Collections;
using System.Collections.Generic;
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
        public static int VideoFrameWidth = 1280;

        /// <summary>
        /// The video frame height.
        /// </summary>
        public static int VideoFrameHeight = 720;

        /// <summary>
        /// The default eye vector.
        /// </summary>
        public static Vector3 DefaultEyeVector = new Vector3(0, 0, -1.0f);

        /// <summary>
        /// The default look at vector.
        /// </summary>
        public static Vector3 DefaultLookAtVector = new Vector3(0, 0, 0);

        /// <summary>
        /// The default up vector.
        /// </summary>
        public static Vector3 DefaultUpVector = new Vector3(0, 1.0f, 0);

        /// <summary>
        /// If clients don't send "stereo-rendering" message after this time,
        /// the video stream will start in non-stereo mode.
        /// </summary>
        public static int StereoFlagWaitTime = 3000;

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
        /// Stores all remote peers' data.
        /// </summary>
        private Dictionary<int, RemotePeerData> remotePeersData = new Dictionary<int, RemotePeerData>();

        /// <summary>
        /// Stores the left eye camera's default position.
        /// </summary>
        private Vector3 leftEyeDefaultPosition;

        /// <summary>
        /// Stores the left eye camera's default rotation.
        /// </summary>
        private Vector3 leftEyeDefaultRotation;

        /// <summary>
        /// Unity engine object Awake() hook
        /// </summary>
        private void Awake()
        {
            // Make sure that the render window continues to render when the game window 
            // does not have focus
            Application.runInBackground = true;
            Application.targetFrameRate = 60;

            // Setup default cameras.
            leftEyeDefaultPosition = this.LeftEye.transform.position;
            leftEyeDefaultRotation = this.LeftEye.transform.eulerAngles;
            SetupActiveEyes(false);

            // Open the connection.
            Open();

            // Initializes the buffer renderer using render texture.
            StartCoroutine(Plugin.NativeInitWebRTC());
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
                foreach (var peerData in remotePeersData.Values)
                {
                    // Makes sure that the mono/stereo mode has been set.
                    if (peerData.IsStereo.HasValue)
                    {
                        // Lazily initializes the render textures.
                        if (peerData.LeftRenderTexture == null)
                        {
                            peerData.InitializeRenderTextures();
                        }

                        // Resets to the default transform.
                        SetupActiveEyes(peerData.IsStereo.Value);
                        LeftEye.transform.position = leftEyeDefaultPosition;
                        LeftEye.transform.eulerAngles = leftEyeDefaultRotation;
                        LeftEye.ResetProjectionMatrix();

                        if (!peerData.IsStereo.Value)
                        {
                            LeftEye.targetTexture = peerData.LeftRenderTexture;
                            LeftEye.transform.position = peerData.EyeVector;
                            LeftEye.transform.LookAt(peerData.LookAtVector, peerData.UpVector);
                            LeftEye.Render();
                        }
                        else if (peerData.IsNew)
                        {
                            // Sets render textures for both eyes.
                            LeftEye.targetTexture = peerData.LeftRenderTexture;
                            RightEye.targetTexture = peerData.RightRenderTexture;

                            // Updates left and right projection matrices.
                            LeftEye.projectionMatrix = peerData.stereoLeftProjectionMatrix;
                            RightEye.projectionMatrix = peerData.stereoRightProjectionMatrix;

                            // Updates camera transform's position and rotation.
                            // Converts from right-handed to left-handed coordinates.
                            peerData.stereoLeftViewMatrix = Matrix4x4.Inverse(peerData.stereoLeftViewMatrix);
                            peerData.stereoRightViewMatrix = Matrix4x4.Inverse(peerData.stereoRightViewMatrix);

                            // Updates transform's position for both eyes.
                            this.LeftEye.transform.position = new Vector3(
                                peerData.stereoLeftViewMatrix.m03,
                                peerData.stereoLeftViewMatrix.m13,
                                -peerData.stereoLeftViewMatrix.m23);

                            this.RightEye.transform.position = new Vector3(
                                peerData.stereoRightViewMatrix.m03,
                                peerData.stereoRightViewMatrix.m13,
                                -peerData.stereoRightViewMatrix.m23);

                            // Updates transform's rotation for both eyes.
                            Quaternion leftQ = QuaternionFromMatrix(peerData.stereoLeftViewMatrix);
                            this.LeftEye.transform.rotation = new Quaternion(
                                -leftQ.x, -leftQ.y, leftQ.z, leftQ.w);

                            Quaternion rightQ = QuaternionFromMatrix(peerData.stereoRightViewMatrix);
                            this.RightEye.transform.rotation = new Quaternion(
                                -rightQ.x, -rightQ.y, rightQ.z, rightQ.w);

                            // Manually render to textures for both eyes.
                            this.LeftEye.Render();
                            this.RightEye.Render();
                        }
                    }
                    else
                    {
                        // Forces non-stereo mode initialization.
                        if (Environment.TickCount - peerData.startTick > StereoFlagWaitTime)
                        {
                            peerData.IsStereo = false;
                            peerData.EyeVector = DefaultEyeVector;
                            peerData.LookAtVector = DefaultLookAtVector;
                            peerData.UpVector = DefaultUpVector;
                        }
                    }
                }

                StartCoroutine(SendFrame());
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
            Plugin.DataChannelMessage += OnDataChannelMessage;

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
        /// Handles input data and updates local transformations
        /// </summary>
        /// <param name="data">input data</param>
        private void OnDataChannelMessage(int peerId, string data)
        {
            try
            {
                JSONNode node = SimpleJSON.JSON.Parse(data);
                string messageType = node["type"];
                string camera = "";

                // Retrieves remote peer data from dictionary, creates new if needed.
                RemotePeerData peerData = default(RemotePeerData);
                if (!remotePeersData.TryGetValue(peerId, out peerData))
                {
                    peerData = new RemotePeerData();
                    peerData.startTick = Environment.TickCount;
                    remotePeersData.Add(peerId, peerData);
                }

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

                        peerData.IsStereo = isStereo == 1;
                        peerData.EyeVector = DefaultEyeVector;
                        peerData.LookAtVector = DefaultLookAtVector;
                        peerData.UpVector = DefaultUpVector;
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
                            peerData.EyeVector = loc;

                            Vector3 look = new Vector3();
                            look.x = float.Parse(sp[3]);
                            look.y = float.Parse(sp[4]);
                            look.z = float.Parse(sp[5]);
                            peerData.LookAtVector = look;

                            Vector3 u = new Vector3();
                            u.x = float.Parse(sp[6]);
                            u.y = float.Parse(sp[7]);
                            u.z = float.Parse(sp[8]);
                            peerData.UpVector = u;

                            peerData.IsNew = true;
                        }

                        break;

                    case "camera-transform-stereo":
                        camera = node["body"];
                        if (!peerData.IsNew && camera != null && camera.Length > 0)
                        {
                            string[] coords = camera.Split(',');
                            int index = 0;
                            Matrix4x4 leftProjectionMatrix = Matrix4x4.identity;
                            Matrix4x4 leftViewMatrix = Matrix4x4.identity;
                            Matrix4x4 rightProjectionMatrix = Matrix4x4.identity;
                            Matrix4x4 rightViewMatrix = Matrix4x4.identity;
                            for (int i = 0; i < 4; i++)
                            {
                                for (int j = 0; j < 4; j++)
                                {
                                    rightViewMatrix[i, j] = float.Parse(coords[48 + index]);
                                    rightProjectionMatrix[i, j] = float.Parse(coords[32 + index]);
                                    leftViewMatrix[i, j] = float.Parse(coords[16 + index]);
                                    leftProjectionMatrix[i, j] = float.Parse(coords[index++]);
                                }
                            }

                            peerData.stereoLeftProjectionMatrix = leftProjectionMatrix;
                            peerData.stereoLeftViewMatrix = leftViewMatrix;
                            peerData.stereoRightProjectionMatrix = rightProjectionMatrix;
                            peerData.stereoRightViewMatrix = rightViewMatrix;
                            peerData.IsNew = true;
                        }

                        break;

                    case "camera-transform-stereo-prediction":
                        camera = node["body"];
                        if (!peerData.IsNew && camera != null && camera.Length > 0)
                        {
                            string[] coords = camera.Split(',');

                            // Parse the prediction timestamp from the message body.
                            long timestamp = long.Parse(coords[64]);

                            if (timestamp != peerData.LastTimestamp)
                            {
                                peerData.LastTimestamp = timestamp;

                                int index = 0;
                                Matrix4x4 leftProjectionMatrix = Matrix4x4.identity;
                                Matrix4x4 leftViewMatrix = Matrix4x4.identity;
                                Matrix4x4 rightProjectionMatrix = Matrix4x4.identity;
                                Matrix4x4 rightViewMatrix = Matrix4x4.identity;
                                for (int i = 0; i < 4; i++)
                                {
                                    for (int j = 0; j < 4; j++)
                                    {
                                        rightViewMatrix[i, j] = float.Parse(coords[48 + index]);
                                        rightProjectionMatrix[i, j] = float.Parse(coords[32 + index]);
                                        leftViewMatrix[i, j] = float.Parse(coords[16 + index]);
                                        leftProjectionMatrix[i, j] = float.Parse(coords[index++]);
                                    }
                                }

                                peerData.stereoLeftProjectionMatrix = leftProjectionMatrix;
                                peerData.stereoLeftViewMatrix = leftViewMatrix;
                                peerData.stereoRightProjectionMatrix = rightProjectionMatrix;
                                peerData.stereoRightViewMatrix = rightViewMatrix;
                                peerData.IsNew = true;
                            }
                        }

                        break;
                }
            }
            catch (Exception ex)
            {
                Debug.LogWarning("DataChannelMessage threw " + ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        /// <summary>
        /// Setup eye cameras based on number of eyes.
        /// Cameras are disabled as we we take control of render order ourselves, see
        /// https://docs.unity3d.com/ScriptReference/Camera.Render.html for more info
        /// </summary>
        private void SetupActiveEyes(bool isStereo)
        {
            if (isStereo)
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
        }

        /// <summary>
        /// Sends frame buffer.
        /// </summary>
        private IEnumerator SendFrame()
        {
            yield return new WaitForEndOfFrame();
            foreach (var peer in remotePeersData)
            {
                int peerId = peer.Key;
                RemotePeerData peerData = peer.Value;
                if (peerData.LeftRenderTexture && (!peerData.IsStereo.Value || peerData.IsNew))
                {
                    Plugin.SendFrame(
                        peerId,
                        peerData.IsStereo.Value,
                        peerData.LeftRenderTexture,
                        peerData.RightRenderTexture,
                        peerData.LastTimestamp);

                    peerData.IsNew = false;
                }
            }
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

        /// <summary>
        /// Remote peer data.
        /// </summary>
        private class RemotePeerData
        {
            /// <summary>
            /// True if this data hasn't been processed.
            /// </summary>
            public bool IsNew { get; set; }

            /// <summary>
            /// True for stereo output, false otherwise.
            /// </summary>
            public bool? IsStereo { get; set; }

            /// <summary>
            /// The look at vector used in camera transform.
            /// </summary>
            public Vector3 LookAtVector { get; set; }

            /// <summary>
            /// The up vector used in camera transform.
            /// </summary>
            public Vector3 UpVector { get; set; }

            /// <summary>
            /// The eye vector used in camera transform.
            /// </summary>
            public Vector3 EyeVector { get; set; }

            /// <summary>
            /// The left stereo eye projection as determined by client control
            /// </summary>
            public Matrix4x4 stereoLeftProjectionMatrix = Matrix4x4.identity;

            /// <summary>
            /// The left stereo eye view as determined by client control
            /// </summary>
            public Matrix4x4 stereoLeftViewMatrix = Matrix4x4.identity;

            /// <summary>
            /// The right stereo eye projection as determined by client control
            /// </summary>
            public Matrix4x4 stereoRightProjectionMatrix = Matrix4x4.identity;

            /// <summary>
            /// The right stereo eye view as determined by client control
            /// </summary>
            public Matrix4x4 stereoRightViewMatrix = Matrix4x4.identity;

            /// <summary>
            /// The timestamp used for frame synchronization in stereo mode.
            /// </summary>
            public long LastTimestamp { get; set; }

            /// <summary>
            /// The render texture of left eye camera which we use to render.
            /// </summary>
            public RenderTexture LeftRenderTexture { get; set; }

            /// <summary>
            /// The render texture of right eye camera which we use to render.
            /// </summary>
            public RenderTexture RightRenderTexture { get; set; }

            /// <summary>
            /// The starting time.
            /// </summary>
            public long startTick { get; set; }

            /// <summary>
            /// Initializes render textures.
            /// </summary>
            public void InitializeRenderTextures()
            {
                // Left eye.
                LeftRenderTexture = new RenderTexture(
                    VideoFrameWidth, VideoFrameHeight,
                    24, RenderTextureFormat.ARGB32);

                LeftRenderTexture.Create();

                if (IsStereo.Value)
                {
                    // Right eye.
                    RightRenderTexture = new RenderTexture(VideoFrameWidth, VideoFrameHeight,
                        24, RenderTextureFormat.ARGB32);

                    RightRenderTexture.Create();
                }
            }
        }
    }
}
