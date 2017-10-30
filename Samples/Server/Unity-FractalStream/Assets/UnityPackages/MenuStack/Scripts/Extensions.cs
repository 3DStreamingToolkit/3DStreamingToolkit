using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;

namespace MenuStack
{
    /// <summary>
    /// Extension methods for MenuStack
    /// </summary>
    public static class Extensions
    {
        /// <summary>
        /// Get the lowest derived Menu component that's attached to the behavior
        /// </summary>
        /// <param name="behaviour">the <see cref="Behaviour"/></param>
        /// <returns><see cref="Menu"/></returns>
        public static Menu GetDerivedMenuComponent(this Component behaviour)
        {
            Menu comp;
            
            comp = behaviour.GetComponent<OverlayMenu>();

            if (comp == null)
            {
                comp = behaviour.GetComponent<Menu>();
            }

            return comp;
        }
    }
}
