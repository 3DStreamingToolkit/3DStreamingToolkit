using UnityEngine;
using System.Collections;

public class MatchCamera : MonoBehaviour {
    public Camera leftCamera;
    public Camera rightCamera;
    public float ipdMM;
    // Update is called once per frame
    void Start()
    {
        if (ipdMM > 0)
        {
            leftCamera.transform.position.Set(
                    leftCamera.transform.position.x - (ipdMM / 100),
                    leftCamera.transform.position.y,
                    leftCamera.transform.position.z);

            rightCamera.transform.position.Set(
                    rightCamera.transform.position.x + (ipdMM / 100),
                    rightCamera.transform.position.y,
                    rightCamera.transform.position.z);
            
        }
    }
}