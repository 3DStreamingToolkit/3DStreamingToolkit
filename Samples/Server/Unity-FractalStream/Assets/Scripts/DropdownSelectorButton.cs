using UnityEngine;
using UnityEngine.UI;

/// <summary>
/// Allows a button to trigger a dropdown selection for the selected value
/// </summary>
[RequireComponent(typeof(Button))]
public class DropdownSelectorButton : MonoBehaviour
{
    /// <summary>
    /// The dropdown to trigger on button click
    /// </summary>
    public Dropdown ElementList;

    /// <summary>
    /// Unity engine object Start() hook
    /// </summary>
    private void Start()
    {
        // add a listener to the click that triggers the value change
        this.GetComponent<Button>().onClick.AddListener(() =>
        {
            this.ElementList.onValueChanged.Invoke(this.ElementList.value);
        });
    }
}