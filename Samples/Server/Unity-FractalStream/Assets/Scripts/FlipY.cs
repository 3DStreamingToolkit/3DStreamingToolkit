using UnityEngine;

/// <summary>
/// Flip the rendered image vertically using shader. This is needed when rendering
/// to texture.
/// </summary>
public class FlipY : MonoBehaviour
{
    public Material matRT;

    void OnRenderImage(RenderTexture src, RenderTexture dest)
    {
        Graphics.Blit(src, dest, matRT);
    }
}
