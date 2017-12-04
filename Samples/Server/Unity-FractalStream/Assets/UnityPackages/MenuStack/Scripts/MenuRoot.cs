using MenuStack.Animation;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace MenuStack
{
    /**
     * \mainpage unity-menustack docs
     * You probably want to check out the <a href='./annotated.html'>Class List</a> 
     */

    /// <summary>
    /// Represents the root node (and controller) of a menu system
    /// </summary>
    public class MenuRoot : MonoBehaviour
    {
        /// <summary>
        /// The current Unity-MenuStack version
        /// </summary>
        public static readonly Version Version = new Version(1, 4, 0);

        /// <summary>
        /// Menu event handler
        /// </summary>
        /// <param name="menu">the changed menu</param>
        public delegate void MenuChangedHandler(Menu menu);

        /// <summary>
        /// Menu opened event. fired when a menu is opened
        /// </summary>
        public event MenuChangedHandler Opened;

        /// <summary>
        /// Menu closed event. fired when a menu is closed
        /// </summary>
        public event MenuChangedHandler Closed;

        /// <summary>
        /// Disabling runtime menu tagging requires the caller to specify <see cref="TrackedMenus"/> in order for anything to work
        /// </summary>
        /// <remarks>
        /// The caller may find this useful if they see runtime perf costs of iterating over a large menu system
        /// </remarks>
        [Tooltip("With this on, we won't auto tag the menu tree at runtime")]
        public bool DisableRuntimeMenuTagging = false;

        /// <summary>
        /// A custom prefix with which menu components are identified
        /// </summary>
        /// <remarks>
        /// This is only used when <see cref="DisableRuntimeMenuTagging"/> is <c>false</c>
        /// </remarks>
        [Tooltip("The prefix with which menu objects should begin")]
        public string MenuPrefix = "menu";

        /// <summary>
        /// A custom prefix with which overlay menu components are identified
        /// </summary>
        /// <remarks>
        /// This is only used when <see cref="DisableRuntimeMenuTagging"/> is <c>false</c>
        /// </remarks>
        [Tooltip("The prefix with which overlay menu objects should begin")]
        public string OverlayPrefix = "overlay";

        /// <summary>
        /// An interactivity value to set on all <see cref="Menu"/>s that are created with prefixing
        /// </summary>
        [Tooltip("The InteractionType with which all generated menu objects should begin")]
        public Menu.InteractableType PrefixDefaultInteractionType = Menu.InteractableType.SelectableInteractive;

        /// <summary>
        /// Manually specify <see cref="Menu"/>s that this <see cref="MenuRoot"/> controls
        /// </summary>
        /// <remarks>
        /// Overriden at runtime when <see cref="DisableRuntimeMenuTagging"/> is <c>false</c>
        /// </remarks>
        [Tooltip("Manually track specific menus as part of this MenuRoot")]
        public Menu[] TrackedMenus = null;

        /// <summary>
        /// The initially selected menu
        /// </summary>
        /// <remarks>
        /// If this isn't set, the first <see cref="Menu"/> in the heirarchy will be used
        /// </remarks>
        [Tooltip("Manually set the default selected menu")]
        public Menu SelectedMenu = null;

        /// <summary>
        /// Specific tagger to use to tag menus
        /// </summary>
        /// <remarks>
        /// This is only used when <see cref="DisableRuntimeMenuTagging"/> is <c>false</c>
        /// </remarks>
        public RuntimeMenuTagger Tagger = null;

        /// <summary>
        /// Internal history stack
        /// </summary>
        private Stack<Menu> history = new Stack<Menu>();
        
        /// <summary>
        /// Unity engine hook for object awake
        /// </summary>
        void Awake()
        {
            if (!DisableRuntimeMenuTagging)
            {
                if (Tagger == null)
                {
                    Tagger = new RuntimeMenuTagger(this.transform, this.MenuPrefix, this.OverlayPrefix);
                }

                this.TrackedMenus = Tagger.Tag().ToArray();
            }

            if (TrackedMenus != null)
            {
                foreach (var menu in TrackedMenus)
                {
                    menu.InteractionType = this.PrefixDefaultInteractionType;
                    menu.SetInteractable(false);
                    menu.SetVisible(false);
                }
            }

            if (this.SelectedMenu == null && this.TrackedMenus != null && this.TrackedMenus.Length > 0)
            {
                this.SelectedMenu = this.TrackedMenus[0];
            }

            if (SelectedMenu != null)
            {
                OpenAsync(SelectedMenu);
            }
        }


        /// <summary>
        /// Opens a specific <see cref="Menu"/>
        /// </summary>
        /// <param name="menu">the <see cref="Menu"/> to open</param>
        public void OpenAsync(Menu menu)
        {
            StartCoroutine(OpenAsync_Internal(menu));
        }

        /// <summary>
        /// Closes the currently open <see cref="Menu"/>
        /// </summary>
        public void CloseAsync()
        {
            StartCoroutine(CloseAsync_Internal());
        }

        /// <summary>
        /// Internal helper for closing a menu
        /// </summary>
        /// <param name="menu">the <see cref="Menu"/> to close</param>
        /// <param name="leaveVisible">indicates if we leave the <see cref="Menu"/> visible</param>
        /// <returns></returns>
        private IEnumerator CloseMenuAsync(Menu menu, bool leaveVisible = false)
        {
            if (menu == null)
            {
                yield break;
            }

            menu.SetInteractable(false);
            
            var animator = menu.GetComponent<MenuAnimator>();
            if (animator != null)
            {
                yield return animator.CloseAsync();
            }

            if (!leaveVisible)
            {
                menu.SetVisible(false);
            }

            if (this.Closed != null)
            {
                this.Closed(menu);
            }
        }

        /// <summary>
        /// Internal helper for opening a menu
        /// </summary>
        /// <param name="menu">the <see cref="Menu"/> to open</param>
        /// <returns></returns>
        private IEnumerator OpenMenuAsync(Menu menu)
        {
            if (menu == null)
            {
                yield break;
            }


            menu.SetVisible(true);

            var animator = menu.GetComponent<MenuAnimator>();
            if (animator != null)
            {
                yield return animator.OpenAsync();
            }

            menu.SetInteractable(true);

            if (this.Opened != null)
            {
                this.Opened(menu);
            }
        }

        /// <summary>
        /// Opens a specific <see cref="Menu"/>
        /// </summary>
        /// <param name="menu">the <see cref="Menu"/> to open</param>
        private IEnumerator OpenAsync_Internal(Menu menu)
        {
            var top = history.Count == 0 ? null : history.Peek();

            if (top != null)
            {
                yield return CloseMenuAsync(top, leaveVisible: (menu is OverlayMenu));
            }

            yield return OpenMenuAsync(menu);

            history.Push(menu);
        }

        /// <summary>
        /// Closes the currently open <see cref="Menu"/>
        /// </summary>
        private IEnumerator CloseAsync_Internal()
        {
            var top = history.Count == 0 ? null : history.Pop();

            if (top != null)
            {
                yield return CloseMenuAsync(top);
            }

            var newTop = history.Count == 0 ? null : history.Peek();

            if (newTop != null)
            {
                yield return OpenMenuAsync(newTop);
            }
        }
    }
}