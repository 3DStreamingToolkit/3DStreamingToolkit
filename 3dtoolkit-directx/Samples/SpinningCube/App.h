#pragma once

#include "pch.h"
#include "DeviceResources.h"

using namespace Platform;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;

namespace Toolkit3DSample
{
	ref class App sealed : IFrameworkView
	{
	public:
		virtual void Initialize(CoreApplicationView^);
		virtual void SetWindow(CoreWindow^);
		virtual void Load(String^);
		virtual void Run();
		virtual void Uninitialize();
		void OnActivated(CoreApplicationView^, IActivatedEventArgs^);

	private:
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
	};

	ref class AppSource sealed : IFrameworkViewSource
	{
	public:
		virtual IFrameworkView^ CreateView();
	};
}
