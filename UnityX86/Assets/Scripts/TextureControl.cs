using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class TextureControl : MonoBehaviour
{
    public Renderer renderer;
    private Texture2D sourceTexture2D;
  
    void Start()
    {
        sourceTexture2D = new Texture2D(640, 640, TextureFormat.ARGB32, false);
        renderer.material.mainTexture = sourceTexture2D;


        for (int x = 0; x < 640; x++)
        {
            for (int y = 0; y < 640; y++)
            {
                if (Mathf.Abs(x - y) < 50)
                {
                    sourceTexture2D.SetPixel(x, y, Color.yellow);
                }
                else
                {
                    sourceTexture2D.SetPixel(x, y, Color.blue);
                }
            }
        }
        sourceTexture2D.Apply();
    }

    void Update()
    {
        
        
    }
}
