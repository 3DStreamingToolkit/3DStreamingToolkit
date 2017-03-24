using System;
using System.Collections.Generic;


namespace TestLib
{
    public class TestClass1
    {
        public string Echo(string msg)
        {
            return "Echo: " + msg;
        }

        public string Call(string msg)
        {
            return "Call: " + msg;
        }
    }
}
