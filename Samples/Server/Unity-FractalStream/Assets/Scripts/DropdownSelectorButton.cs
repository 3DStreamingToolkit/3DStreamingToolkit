using UnityEngine;
using UnityEngine.UI;

[RequireComponent(typeof(Button))]
public class DropdownSelectorButton : MonoBehaviour
{
    public Dropdown ElementList;

    private Button button;

    private void Start()
    {
        this.button = this.GetComponent<Button>();
        this.button.onClick.AddListener(() =>
        {
            this.ElementList.onValueChanged.Invoke(this.ElementList.value);
        });
    }
}