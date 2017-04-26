using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;


// Based on Unity3D Low-Level Plugin Reference Source Code
public class TexturePlugin : MonoBehaviour
{
#if UNITY_EDITOR
    [DllImport("TexturesWin32")]
#else
    [DllImport("TexturesUWP")]
#endif
    private static extern void SetTextureFromUnity(System.IntPtr texture, int w, int h);

#if UNITY_EDITOR
    [DllImport("TexturesWin32")]
#else
    [DllImport("TexturesUWP")]
#endif
    private static extern unsafe void ProcessRawFrame(int w, int h, byte[] yPlane, int yStride, byte[] uPlane, int uStride,
        byte[] vPlane, int vStride);

#if UNITY_EDITOR
    [DllImport("TexturesWin32")]
#else
    [DllImport("TexturesUWP")]
#endif
    private static extern IntPtr GetRenderEventFunc();

    
    unsafe void Start()
    {
        CreateTextureAndPassToPlugin();
        StartCoroutine(CallPluginAtEndOfFrames());

        
        byte[] textureDataY = new byte[409600 * 4];
        byte[] textureDataU = new byte[102400];
        byte[] textureDataV = new byte[102400];        
        for (int y = 0; y < 640; y++)
        {
            for (int x = 0; x < 640; x++)
            {
                if (Mathf.Abs(x - y) < 20)
                {
                    textureDataY[(y * 640) + x] = 150;
                }
                else
                {
                    textureDataY[(y * 640) + x] = 0;
                }                
            }
        }
        ProcessRawFrame(640, 640, textureDataY, 640, textureDataU, 320, textureDataV, 320);
    }


    private void CreateTextureAndPassToPlugin()
    {
        // Create a texture
        Texture2D tex = new Texture2D(640, 640, TextureFormat.ARGB32, false);

        // Set point filtering just so we can see the pixels clearly
        tex.filterMode = FilterMode.Point;
        // Call Apply() so it's actually uploaded to the GPU
        tex.Apply();

        // Set texture onto our material
        GetComponent<Renderer>().material.mainTexture = tex;

        // Pass texture pointer to the plugin
        SetTextureFromUnity(tex.GetNativeTexturePtr(), tex.width, tex.height);
    }

    private IEnumerator CallPluginAtEndOfFrames()
    {
        while (true)
        {
            // Wait until all frame rendering is done
            yield return new WaitForEndOfFrame();
            
            // Issue a plugin event with arbitrary integer identifier.
            // The plugin can distinguish between different
            // things it needs to do based on this ID.
            // For our simple plugin, it does not matter which ID we pass here.
            GL.IssuePluginEvent(GetRenderEventFunc(), 1);
        }
    }
}
