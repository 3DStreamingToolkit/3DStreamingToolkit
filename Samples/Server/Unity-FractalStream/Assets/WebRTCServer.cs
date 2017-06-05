using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

public class WebRTCServer : MonoBehaviour
{
    public delegate void FPtr([MarshalAs(UnmanagedType.LPStr)]string value);

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
	[DllImport ("__Internal")]
#else
    [DllImport("StreamingUnityServerPlugin")]
#endif
    private static extern IntPtr GetRenderEventFunc();

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
	[DllImport ("__Internal")]
#else
    [DllImport("StreamingUnityServerPlugin")]
#endif
    private static extern void Login(string server, Int32 port);

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
	[DllImport ("__Internal")]
#else
    [DllImport("StreamingUnityServerPlugin")]
#endif    
    public static extern void SetInputDataCallback(FPtr cb);

#if (UNITY_IPHONE || UNITY_WEBGL) && !UNITY_EDITOR
	[DllImport ("__Internal")]
#else
    [DllImport("StreamingUnityServerPlugin")]
#endif
    private static extern void Close();

    static Vector3 Location = new Vector3();
    static Vector3 LookAt = new Vector3();
    static Vector3 Up = new Vector3();

#if UNITY_EDITOR
    /// <summary>
    /// Copies two config files to the executable directory.
    /// </summary>
    [UnityEditor.Callbacks.PostProcessBuild]
    static void OnPostprocessBuild(
        UnityEditor.BuildTarget target,
        string pathToBuiltProject)
    {
        string sourcePath = Application.dataPath + "/Plugins/";
        string desPath = pathToBuiltProject.Substring(
            0, pathToBuiltProject.LastIndexOf("/") + 1);

        UnityEditor.FileUtil.CopyFileOrDirectory(
            sourcePath + "nvEncConfig.json",
            desPath + "nvEncConfig.json");

        UnityEditor.FileUtil.CopyFileOrDirectory(
            sourcePath + "webrtcConfig.json",
            desPath + "webrtcConfig.json");
    }
#endif // UNITY_EDITOR

    // Use this for initialization
    void Start()
    {
        // Make sure that the render window continues to render when the game window does not have focus
        Application.runInBackground = true;

        CommandBuffer cmb = new CommandBuffer();
        cmb.name = "WebRTC Encoding";
        cmb.IssuePluginEvent(GetRenderEventFunc(), 0);

        Camera.main.AddCommandBuffer(CameraEvent.AfterEverything, cmb);
        
        Login("localhost", 8888);
        
        FPtr cb = new FPtr(OnInputData);

        SetInputDataCallback(cb);
    }

    void Stop()
    {
        Close();
    }

    private void OnDisable()
    {
        Close();
    }

    // Update is called once per frame
    void Update()
    {
        transform.position = Location;
        transform.LookAt(LookAt, Up);
    }

    void OnInputData(string val)
    {
        var node = SimpleJSON.JSON.Parse(val);
        string messageType = node["type"];

        switch (messageType)
        {
            /*
            case "keyboard-event":
                
                var e = UnityEngine.Event.KeyboardEvent("s");
                
                int msg = node["message"];
                int wParam = node["wParam"];


                Debug.Log("Message(" + msg.ToString() + ", " + wParam.ToString() + ")");
                
                break;
            */

            case "camera-transform":
                string cam = node["state"];

                if (cam != null && cam.Length > 0)
                {
                    string[] sp = cam.Split(new char[] { ' ', ',' }, StringSplitOptions.RemoveEmptyEntries);

                    Vector3 loc = new Vector3();
                    loc.x = float.Parse(sp[0]);
                    loc.y = float.Parse(sp[1]);
                    loc.z = float.Parse(sp[2]);
                    Location = loc;

                    Vector3 look = new Vector3();
                    look.x = float.Parse(sp[3]);
                    look.y = float.Parse(sp[4]);
                    look.z = float.Parse(sp[5]);
                    LookAt = look;

                    Vector3 up = new Vector3();
                    up.x = float.Parse(sp[6]);
                    up.y = float.Parse(sp[7]);
                    up.z = float.Parse(sp[8]);
                    Up = up;
                }

                break;

            default:
                /*
                Debug.Log("InputData(" + val + ")");
                Debug.Log("");

                foreach (var child in node.Children)
                {
                    Debug.Log(child.ToString());
                }
                */
                break;
        }
    }
}
