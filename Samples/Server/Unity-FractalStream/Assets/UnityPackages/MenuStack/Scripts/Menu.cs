using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

namespace MenuStack
{
    /// <summary>
    /// Represents any menu (a logical grouping of ui elements) within a <see cref="MenuRoot"/>
    /// </summary>
    public class Menu : MonoBehaviour
    {
        /// <summary>
        /// Represents a particular style of interactivity for a <see cref="Menu"/>
        /// </summary>
        public enum InteractableType
        {
            /// <summary>
            /// Indicates that we should toggle interactivity by enabling or
            /// disabling any attached <see cref="Selectable"/>s
            /// </summary>
            SelectableEnable,

            /// <summary>
            /// Indicates that we should toggle interactivity by changing the
            /// <see cref="Selectable.interactable"/> property on any attached <see cref="Selectable"/>s
            /// </summary>
            SelectableInteractive
        }

        /// <summary>
        /// The runtime name for this menu
        /// </summary>
        public string Name
        {
            get
            {
                return this.CustomName == null ? this.name : this.CustomName;
            }
        }

        /// <summary>
        /// Indicates if this menu is visible
        /// </summary>
        public bool Visible
        {
            get;
            protected set;
        }

        /// <summary>
        /// Indicates if this menu is interactable
        /// </summary>
        public bool Interactable
        {
            get;
            protected set;
        }

        /// <summary>
        /// A custom name for this menu
        /// </summary>
        /// <remarks>
        /// Overrides the default <see cref="Name"/>
        /// </remarks>
        [Tooltip("Overrides the default name")]
        public string CustomName = null;

        /// <summary>
        /// Determines how we handle interactivity for this menu
        /// </summary>
        /// <remarks>
        /// <see cref="SetInteractable(bool)"/>
        /// </remarks>
        [Tooltip("Determines how we handle interactivity for this menu")]
        public Menu.InteractableType InteractionType = InteractableType.SelectableInteractive;

        /// <summary>
        /// The initially selected <see cref="Selectable"/> when this <see cref="Menu"/> is <see cref="Visible"/>
        /// </summary>
        [Tooltip("The initially selected selectable when this menu is visible")]
        public Selectable InitialSelected = null;

        /// <summary>
        /// Determines if we use initially selected <see cref="Selectable"/> when returning to the menu
        /// after the first time
        /// </summary>
        /// <remarks>
        /// If this is <c>false</c> we'll instead keep history and return to the last selected <see cref="Selectable"/>
        /// </remarks>
        [Tooltip("Determines if we use the InitialSelected value when returning to the menu")]
        public bool AlwaysUseInitialSelected = false;

        /// <summary>
        /// Internal flag used to track when this object hasn't been <see cref="Start"/>-ed yet
        /// </summary>
        private bool uninitialized = true;

        /// <summary>
        /// Internal storage for the <see cref="Selectable"/> that was selected when we last viewed this menu
        /// </summary>
        /// <remarks>
        /// This is only used when <see cref="AlwaysUseInitialSelected"/> is <c>false</c>
        /// </remarks>
        private Selectable previouslySelected = null;

        /// <summary>
        /// Unity engine hook for object start
        /// </summary>
        void Start()
        {
            this.uninitialized = false;
        }

        /// <summary>
        /// Setter to change the interactable state of this menu
        /// </summary>
        /// <remarks>
        /// Note: this changes the state of the menus underlying <see cref="Selectable"/> components
        /// </remarks>
        /// <param name="value">new state</param>
        public virtual void SetInteractable(bool value)
        {
            if (!value && (this.Interactable || this.uninitialized))
            {
                // indicates if we've already tagged the selected this walk
                // note: this is used as an early exit for the selection logic below
                bool foundSelectedAlreadyThisWalk = false;

                WalkChildComponents<Selectable>(c =>
                {
                    // determine the current selectable and track if if we aren't using AlwaysUseInitialSelected
                    // for more info on this logic, see Selectable.Select() from UnityEngine.UI
                    if(!this.AlwaysUseInitialSelected &&
                        !foundSelectedAlreadyThisWalk &&
                        EventSystem.current != null &&
                        c.gameObject == EventSystem.current.currentSelectedGameObject &&
                        !EventSystem.current.alreadySelecting)
                    {
                        this.previouslySelected = c;
                        foundSelectedAlreadyThisWalk = true;
                    }

                    // mark everything with the correct "closed" state
                    if (this.InteractionType == InteractableType.SelectableInteractive)
                    {
                        c.interactable = false;
                    }
                    else if (this.InteractionType == InteractableType.SelectableEnable)
                    {
                        c.enabled = false;
                    }
                });
            }
            else if (value && (!this.Interactable || this.uninitialized))
            {
                // indicates if we've already reset the selected this walk
                // note: this is used as an early exit for the select logic below
                bool setSelectedAlreadyThisWalk = false;

                WalkChildComponents<Selectable>(c =>
                {
                    // mark everything with the correct "opened" state
                    if (this.InteractionType == InteractableType.SelectableInteractive)
                    {
                        c.interactable = true;
                    }
                    else if (this.InteractionType == InteractableType.SelectableEnable)
                    {
                        c.enabled = true;
                    }

                    // determine which selectable should be selected first, and select it
                    // if InitialSelected || previouslySelected == null, select the first one
                    // note: this if has weird syntax to make it easier to understand...
                    if
                    (
                        !setSelectedAlreadyThisWalk &&
                        (
                            (this.AlwaysUseInitialSelected &&
                                (this.InitialSelected == c ||
                                this.InitialSelected == null)
                            ) ||
                            (!this.AlwaysUseInitialSelected &&
                                (this.previouslySelected == c ||
                                this.previouslySelected == null)
                            )
                        )
                    )
                    {
                        c.Select();
                        setSelectedAlreadyThisWalk = true;
                    }
                }, skipMenus: true);
            }

            this.Interactable = value;
        }

        /// <summary>
        /// Setter to change the visible state of this menu
        /// </summary>
        /// <remarks>
        /// Note: this changes the state of the menus underlying <see cref="GameObject"/>s
        /// </remarks>
        /// <param name="value">new state</param>
        public virtual void SetVisible(bool value)
        {
            if (!value && (this.Visible || this.uninitialized))
            {
                SetChildActive(false, skipMenus: true);
            }
            else if (value && (!this.Visible || this.uninitialized))
            {
                SetChildActive(true, skipMenus: true);
            }

            this.Visible = value;
        }

        /// <summary>
        /// Internal helper to walk child <see cref="Behaviour"/>s
        /// </summary>
        /// <typeparam name="TComp">component type</typeparam>
        /// <param name="func">a function called for each <see cref="Behaviour"/></param>
        /// <param name="skipMenus">indicates if we should include or exclude children with <see cref="Menu"/> components</param>
        private void WalkChildComponents<TComp>(Action<TComp> func, bool skipMenus = false) where TComp : Behaviour
        {
            foreach (var comp in this.GetComponentsInChildren<TComp>())
            {
                if (skipMenus && comp.GetComponent<Menu>() != null)
                {
                    continue;
                }

                func(comp);
            }
        }

        /// <summary>
        /// Internal helper to enable/disable child <see cref="GameObject"/>
        /// </summary>
        /// <param name="value"></param>
        /// <param name="skipMenus">indicates if we should include of exclude children with <see cref="Menu"/> components</param>
        private void SetChildActive(bool value, bool skipMenus = false)
        {
            for (var i = 0; i < this.transform.childCount; i++)
            {
                var child = this.transform.GetChild(i);

                if (skipMenus && child.GetComponent<Menu>() != null)
                {
                    continue;
                }

                child.gameObject.SetActive(value);
            }
        }
    }
}
