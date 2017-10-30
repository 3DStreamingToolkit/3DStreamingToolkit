using MenuStack.Navigation;
using UnityEditor;
using UnityEditor.Events;
using UnityEngine;
using UnityEngine.UI;

namespace MenuStack.Editor
{
    /// <summary>
    /// Custom <see cref="BaseMenuNavigator"/> editor to facilitate autowiring
    /// </summary>
    [CustomEditor(typeof(BaseMenuNavigator), editorForChildClasses: true)]
    [CanEditMultipleObjects]
    public class NavigatorEditor : UnityEditor.Editor
    {
        /// <summary>
        /// Unity editor hook for 
        /// </summary>
        void OnEnable()
        {
            // only autowire if we're not running
            if (!Application.isPlaying && Application.isEditor)
            {
                foreach (var obj in serializedObject.targetObjects)
                {
                    var comp = obj as Component;
                    
                    TryAutoWire(comp.gameObject);
                }
            }
        }

        /// <summary>
        /// Internal helper to try and autowire
        /// </summary>
        /// <param name="root">The root game object</param>
        void TryAutoWire(GameObject root)
        {
            var button = root.GetComponent<Button>();
            if (button == null)
            {
                return;
            }

            var navigator = root.GetComponent<BaseMenuNavigator>();
            if (navigator == null)
            {
                return;
            }

            var onClick = button.onClick;
            var evtCount = onClick.GetPersistentEventCount();
            var hasNavEvt = false;

            for (var i = 0; i < evtCount; i++)
            {
                var evtName = onClick.GetPersistentMethodName(i);
                var evtTarget = onClick.GetPersistentTarget(i);

                if (evtName == INavigatorMethodNames.Navigate.ToString() && evtTarget is INavigator)
                {
                    hasNavEvt = true;
                }
            }

            if (!hasNavEvt)
            {
                UnityEventTools.AddPersistentListener(button.onClick, navigator.Navigate);
            }
        }
    }
}
