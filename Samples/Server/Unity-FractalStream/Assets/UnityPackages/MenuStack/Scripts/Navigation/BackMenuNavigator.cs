using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;

namespace MenuStack.Navigation
{
    /// <summary>
    /// Represents a menu navigator that goes back (effectively closing the current menu) in the <see cref="MenuRoot"/>
    /// </summary>
    public class BackMenuNavigator : BaseMenuNavigator
    {
        /// <summary>
        /// Not implemented
        /// </summary>
        /// <remarks>
        /// Since the <see cref="BackMenuNavigator"/> overrides navigate to make a raw call to the <see cref="MenuRoot"/>
        /// we do not actually need the implemented <see cref="GetNavigationLocation"/> - however, it must have some implementation
        /// because it's an abstract method in <see cref="BaseMenuNavigator"/> (by design, navigators should not use this pattern)
        /// but back is super special :)
        /// </remarks>
        /// <returns></returns>
        protected override Menu GetNavigationLocation()
        {
            throw new NotImplementedException();
        }

        /// <summary>
        /// Triggers navigation
        /// </summary>
        public override void Navigate()
        {
            this.GetComponentInParent<MenuRoot>().CloseAsync();
        }
    }
}
