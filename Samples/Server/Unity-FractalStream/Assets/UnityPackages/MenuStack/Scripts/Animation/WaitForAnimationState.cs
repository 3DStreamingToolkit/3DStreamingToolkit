using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;

namespace MenuStack.Animation
{
    /// <summary>
    /// Waits for a specific animation state to be transitioned to, and for it's motion to complete
    /// </summary>
    public class WaitForAnimationState : CustomYieldInstruction
    {
        private Animator animator;
        private string state;
        private int layer;

        /// <summary>
        /// Default ctor
        /// </summary>
        /// <param name="animator">the animator to check against</param>
        /// <param name="state">the state to wait for</param>
        /// <param name="layer">the layer to check against</param>
        public WaitForAnimationState(Animator animator, string state, int layer = 0)
        {
            this.animator = animator;
            this.state = state;
            this.layer = layer;
        }

        /// <summary>
        /// <see cref="CustomYieldInstruction"/> https://docs.unity3d.com/ScriptReference/CustomYieldInstruction.html
        /// </summary>
        /// <remarks>
        /// This will be queried between Update() and LateUpdate()
        /// </remarks>
        public override bool keepWaiting
        {
            get
            {
                var stateInfo = this.animator.GetCurrentAnimatorStateInfo(this.layer);

                return (!stateInfo.IsName(this.state) || this.animator.IsInTransition(this.layer)) ||
                    (this.animator.GetCurrentAnimatorClipInfoCount(this.layer) == 0 || stateInfo.normalizedTime < 1);
            }
        }
    }
}
