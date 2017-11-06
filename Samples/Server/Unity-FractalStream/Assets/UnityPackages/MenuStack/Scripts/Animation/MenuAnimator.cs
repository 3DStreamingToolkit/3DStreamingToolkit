using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;

namespace MenuStack.Animation
{
    /// <summary>
    /// Allows configuration of menu animations
    /// </summary>
    [RequireComponent(typeof(Animator))]
    [RequireComponent(typeof(Menu))]
    public class MenuAnimator : MonoBehaviour
    {
        /// <summary>
        /// The menu open animator state name, in the underlying animator
        /// </summary>
        public string OpenStateName = "Open";

        /// <summary>
        /// The menu close animator state name, in the underlying animator
        /// </summary>
        public string CloseStateName = "Close";
        
        /// <summary>
        /// The layer in which menu animation is defined, in the underlying animator
        /// </summary>
        public int AnimationLayer = 0;

        /// <summary>
        /// The default state to use in the underlying animator
        /// </summary>
        /// <remarks>
        /// If this is left unset, we'll determine at runtime if we should start in
        /// <c>OpenStateName</c> or <c>CloseStateName</c>
        /// </remarks>
        public string DefaultStateName = null;

        /// <summary>
        /// Internal reference to the underlying animator
        /// </summary>
        private Animator animator;
        
        /// <summary>
        /// Unity start hook
        /// </summary>
        void Start()
        {
            if (this.animator == null)
            {
                this.animator = this.GetComponent<Animator>();
            }

            if (!string.IsNullOrEmpty(this.DefaultStateName))
            {
                this.animator.Play(this.DefaultStateName, this.AnimationLayer);
            }
            else
            {
                var menu = this.GetDerivedMenuComponent();

                var defaultState = menu.Visible ? this.OpenStateName : this.CloseStateName;

                this.animator.Play(defaultState, this.AnimationLayer);
            }
        }

        /// <summary>
        /// Opens the menu, using the underlying animator
        /// </summary>
        /// <remarks>
        /// This method doesn't async complete until the animation (and motion) is complete
        /// </remarks>
        /// <returns>async operation result</returns>
        public IEnumerator OpenAsync()
        {
            if (this.animator == null)
            {
                this.animator = this.GetComponent<Animator>();
            }
            
            if (this.animator.GetCurrentAnimatorStateInfo(this.AnimationLayer).IsName(this.OpenStateName))
            {
                yield break;
            }
            
            this.animator.SetTrigger(this.OpenStateName);

            // Note: this will expose a bug where if your animator doesn't function properly
            // (specifically if it doesn't go to and stay in the open state when the open state trigger is set)
            // we'll get stuck here forever. this is acceptable for now
            yield return new WaitForAnimationState(this.animator, this.OpenStateName, this.AnimationLayer);
        }

        /// <summary>
        /// Closes the menu, using the underlying animator
        /// </summary>
        /// <remarks>
        /// This method doesn't async complete until the animation (and motion) is complete
        /// </remarks>
        /// <returns>async operation result</returns>
        public IEnumerator CloseAsync()
        {
            if (this.animator == null)
            {
                this.animator = this.GetComponent<Animator>();
            }

            if (this.animator.GetCurrentAnimatorStateInfo(this.AnimationLayer).IsName(this.CloseStateName))
            {
                yield break;
            }

            this.animator.SetTrigger(this.CloseStateName);

            // Note: this will expose a bug where if your animator doesn't function properly
            // (specifically if it doesn't go to and stay in the close state when the close state trigger is set)
            // we'll get stuck here forever. this is acceptable for now
            yield return new WaitForAnimationState(this.animator, this.CloseStateName, this.AnimationLayer);
        }
    }
}
