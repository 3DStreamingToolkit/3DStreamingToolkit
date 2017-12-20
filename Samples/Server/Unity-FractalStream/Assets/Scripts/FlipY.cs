using UnityEngine;

public class FlipY : MonoBehaviour
{
    public Material matRT;

    void OnRenderImage(RenderTexture src, RenderTexture dest)
    {
        Graphics.Blit(src, dest, matRT);
    }
}
