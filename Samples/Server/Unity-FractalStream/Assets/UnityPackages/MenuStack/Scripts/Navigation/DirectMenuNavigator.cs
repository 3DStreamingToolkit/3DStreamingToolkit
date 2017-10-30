using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MenuStack.Navigation
{
    /// <summary>
    /// Represents a menu navigator that navigates to a specific <see cref="Menu"/> in the <see cref="MenuRoot"/>
    /// </summary>
    public class DirectMenuNavigator : BaseMenuNavigator
    {
        /// <summary>
        /// The menu to directly navigate to
        /// </summary>
        public Menu NavigateTo;

        /// <summary>
        /// Provides a location to navigate to, when navigation occurs
        /// </summary>
        /// <returns><see cref="Menu"/> to navigate to</returns>
        protected override Menu GetNavigationLocation()
        {
            return this.NavigateTo;
        }
    }
}
