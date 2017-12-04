using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MenuStack.Navigation
{
    /// <summary>
    /// Defines a common interface that all navigators implement
    /// </summary>
    public interface INavigator
    {
        /// <summary>
        /// Triggers navigation
        /// </summary>
        void Navigate();
    }

    /// <summary>
    /// Internally defined names of <see cref="INavigator"/> methods
    /// for reference in other parts of the codebase
    /// </summary>
    /// <remarks>
    /// This is a workaround for unity mono not supporting https://msdn.microsoft.com/en-us/library/dn986596.aspx
    /// </remarks>
    public enum INavigatorMethodNames
    {
        None = 0,
        Navigate
    }
}
