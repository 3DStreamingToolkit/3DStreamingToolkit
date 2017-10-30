using UnityEngine;

public class FractalRoot : MonoBehaviour
{

    Fractals rootSettings;

    public int absMinimumDrop = 2;
    public int absMaximumBoost = 8;
    public bool isHL = false;

    void Start()
    {
        rootSettings = this.GetComponent<Fractals>();
    }

    private void VoiceManager_OnVoiceCommand(string command)
    {
        switch (command )
        {
            case "boost":
                Respawn(1);
                break;
            case "drop":
                Respawn(-1);
                break;
        }
    }

    public void Respawn(int step)
    {
        int newMaxDepth = rootSettings.maxDepth + step;
        newMaxDepth = Mathf.Min(Mathf.Max(newMaxDepth, absMinimumDrop), absMaximumBoost);
        rootSettings.maxDepth = newMaxDepth;
        clearChildren();
        rootSettings.spawnFractals();
    }

    private void clearChildren()
    {
        int childs = transform.childCount;
        for (int i = childs - 1; i >= 0; i--)
        {
            GameObject.Destroy(transform.GetChild(i).gameObject);
        }
    }

}



