using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class Fractals : MonoBehaviour {

    public Mesh mesh;
    public Material material;

    public int maxDepth;
    public float childScale;

    private int depth;

    private static Vector3[] childDirections =
{
        Vector3.up,
        Vector3.right,
        Vector3.left, 
        Vector3.forward,
        Vector3.back
    };

    private static Quaternion[] childOrientations =
    {
        Quaternion.identity,
        Quaternion.Euler(0f, 0f, -90f),
        Quaternion.Euler(0f, 0f, 90f),
        Quaternion.Euler(90f, 0f, 0f),
        Quaternion.Euler(-90f, 0f, 0f)
    };
    
    void Start ()
    {
        MeshFilter meshFilter = gameObject.GetComponent<MeshFilter>();
        if (meshFilter == null)
        {
            meshFilter = gameObject.AddComponent<MeshFilter>();
        }

        meshFilter.mesh = mesh;

        MeshRenderer meshRenderer = gameObject.GetComponent<MeshRenderer>();
        if (meshRenderer == null)
        {
            meshRenderer = gameObject.AddComponent<MeshRenderer>();
        }

        meshRenderer.sharedMaterial = material;

        rotationSpeed = Random.Range(-maxRotationSpeed, maxRotationSpeed);
        spawnFractals();
    }

    public void spawnFractals()
    {
        if ( depth < maxDepth)
        {
            StartCoroutine(CreateChildren());
        }
    }

    private IEnumerator CreateChildren()
    {
        for ( int i = 0; i < childDirections.Length; i++ )
        {
            yield return new WaitForSeconds(Random.Range(0.1f, 0.5f));
            new GameObject("spawn child").AddComponent<Fractals>().Initialize(this, i);
        }

    }

    private void Initialize(Fractals parent, int childIndex)
    {
        mesh = parent.mesh;
        material = parent.material;
        maxDepth = parent.maxDepth;
        depth = parent.depth + 1;
        childScale = parent.childScale;
        transform.parent = parent.transform;
        transform.localScale = Vector3.one * childScale;
        transform.localPosition = childDirections[childIndex] * (0.5f + 0.5f * childScale);
        transform.localRotation = childOrientations[childIndex];
        maxRotationSpeed = parent.maxRotationSpeed;
    }

    public float maxRotationSpeed = 10;
    private float rotationSpeed;

    void Update () {
        transform.Rotate(0f, rotationSpeed * Time.deltaTime, 0f);
	}
}
