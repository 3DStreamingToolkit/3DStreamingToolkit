using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

using Windows.UI.Core;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409


namespace TestClient
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {       
        public MainPage()
        {
            this.InitializeComponent();

            InitDisplay();
        }

        public async void RunOnUi(DispatchedHandler handler)
        {
            await Dispatcher.RunAsync(
                Windows.UI.Core.CoreDispatcherPriority.Normal,
                handler
            );
        }

        public void InitDisplay()
        {
            RunOnUi(Init1);
            RunOnUi(Init2);
        }

        private void Init1()
        {
            StatusTextBox.Text = WebRtcUtils.Utils.Version();
        }

        private void Init2()
        {
            ListTextBlock.Text = WebRtcUtils.Utils.LibTestCall();
        }
    }
}
