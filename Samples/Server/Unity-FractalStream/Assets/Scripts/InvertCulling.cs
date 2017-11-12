using UnityEngine;

public class InvertCulling : MonoBehaviour
{
    private bool oldCulling;

    public void OnPreRender()
    {
        oldCulling = GL.invertCulling;
        GL.invertCulling = true;
    }

    public void OnPostRender()
    {
        GL.invertCulling = oldCulling;
    }
}
