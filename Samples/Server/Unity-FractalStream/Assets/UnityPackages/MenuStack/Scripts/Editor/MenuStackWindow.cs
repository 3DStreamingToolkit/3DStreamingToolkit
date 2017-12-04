using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using UnityEditor;
using UnityEngine;

namespace MenuStack.Editor
{
    /// <summary>
    /// An EditorWindow for quickly working with <see cref="Menu"/>s
    /// </summary>
    public class MenuStackWindow : EditorWindow
    {
        /// <summary>
        /// Internal representation of an object change, to track history
        /// </summary>
        private struct ObjectChange
        {
            public Menu Object;
            public bool Operation;
        }
        
        /// <summary>
        /// Initializer that gets or creates a window
        /// </summary>
        [MenuItem("Window/MenuStack/Hierarchy")]
        static void Init()
        {
            MenuStackWindow window = (MenuStackWindow)EditorWindow.GetWindow(typeof(MenuStackWindow));
            window.titleContent.text = "MenuStack - Hierarchy";

            window.Refresh();
            window.Show();
        }

        /// <summary>
        /// Internal reference to a MenuRoot
        /// </summary>
        private MenuRoot selectedRoot;
        
        /// <summary>
        /// Internal reference to all tracked menus
        /// </summary>
        private List<Menu> menus = new List<Menu>();

        /// <summary>
        /// Internal reference to menus that we tagged just for this view
        /// </summary>
        /// <remarks>
        /// These need to be removed when we're done with the view
        /// </remarks>
        private List<Menu> taggedMenus = new List<Menu>();

        /// <summary>
        /// Internal history stack of <see cref="ObjectChange"/>s
        /// </summary>
        private Stack<ObjectChange> history = new Stack<ObjectChange>();
        
        /// <summary>
        /// Finds all tracked <see cref="Menu"/>s and adds them to <see cref="menus"/>
        /// </summary>
        void Refresh()
        {
            if (selectedRoot == null)
            {
                selectedRoot = GameObject.FindObjectOfType<MenuRoot>();
            }

            var existingMenus = RecursivelyGetMenus(selectedRoot.transform);
            var tagger = new RuntimeMenuTagger(selectedRoot.transform, selectedRoot.MenuPrefix, selectedRoot.OverlayPrefix);
            var tagged = tagger.Tag();
            
            menus.AddRange(selectedRoot.TrackedMenus);
            menus.AddRange(tagged);
            taggedMenus.AddRange(tagged.Except(existingMenus));

            menus = menus.Where(m => m != null).Distinct().ToList();
        }

        /// <summary>
        /// Undoes the history of all operations
        /// </summary>
        /// <param name="keepState">indicates if we should keep the visibility states (like on save)</param>
        /// <param name="destroy">indicates if we should destroy the menu</param>
        void Cleanup(bool keepState = false)
        {
            while (history.Count > 0)
            {
                var top = history.Pop();

                if (!keepState)
                {
                    top.Object.SetVisible(!top.Operation);
                }
            }
        }

        /// <summary>
        /// Untags all tagged menus that were tagged just for this view
        /// </summary>
        void Untag()
        {
            foreach (var tagged in taggedMenus)
            {
                DestroyImmediate(tagged);
            }
        }

        /// <summary>
        /// Self explanatory name
        /// </summary>
        /// <param name="root"></param>
        /// <returns></returns>
        List<Menu> RecursivelyGetMenus(Transform root)
        {
            List<Menu> menus = new List<Menu>();
            for (var i = 0; i < root.childCount; i++)
            {
                var child = root.GetChild(i);

                if (child.childCount > 0)
                {
                    menus.AddRange(RecursivelyGetMenus(child));
                }

                menus.AddRange(child.GetComponents<Menu>());
            }

            return menus;
        }

        /// <summary>
        /// Unity hook for rendering
        /// </summary>
        void OnGUI()
        {
            // create a label
            EditorGUILayout.LabelField("Menus  (v" + MenuRoot.Version + ")");
            EditorGUILayout.Separator();

            // only make the Reset button clickable if we have something to reset
            GUI.enabled = history.Count > 0;
            if (GUILayout.Button("Reset"))
            {
                Cleanup(keepState: false);
            }
            if (GUILayout.Button("Save"))
            {
                Cleanup(keepState: true);
            }
            GUI.enabled = true;

            // create a vertical section full of menus that you can toggle visibility on/off for
            GUILayout.BeginVertical(GUILayout.ExpandWidth(true), GUILayout.ExpandHeight(true));
            foreach (var m in menus)
            {
                var name = string.IsNullOrEmpty(m.CustomName) ? m.name : m.CustomName;
                var res = GUILayout.Toggle(m.Visible, name, GUILayout.ExpandWidth(true));
                if (res != m.Visible)
                {
                    // apply and track the change
                    m.SetVisible(res);
                    history.Push(new ObjectChange()
                    {
                        Object = m,
                        Operation = res
                    });
                }
            }
            GUILayout.EndVertical();
        }

        /// <summary>
        /// Unity hook for window focus
        /// </summary>
        void OnFocus()
        {
            Refresh();
        }

        /// <summary>
        /// Unity hook for scene heirarchy change
        /// </summary>
        void OnHeirarchyChange()
        {
            Refresh();
        }

        /// <summary>
        /// Unity hook for window destruction
        /// </summary>
        void OnDestroy()
        {
            Cleanup();
            Untag();
        }

        /// <summary>
        /// Unity hook for project change
        /// </summary>
        void OnProjectChange()
        {
            Cleanup();
            Untag();
        }
    }
}