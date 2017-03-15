#include "pch.h"
#include "App.h"
#include "DeviceResources.h"
#include "CubeRenderer.h"

using namespace Toolkit3DSample;

IFrameworkView^ AppSource::CreateView()
{
	return ref new App();
}

void App::Initialize(CoreApplicationView^ appView)
{
	appView->Activated += ref new TypedEventHandler
		<CoreApplicationView^, IActivatedEventArgs ^>(this, &App::OnActivated);

	// At this point we have access to the device. 
	// We can create the device-dependent resources.
	m_deviceResources = std::make_shared<DX::DeviceResources>();
}

void App::SetWindow(CoreWindow^ window)
{
	m_deviceResources->SetWindow(window);
}

void App::Load(String^ entryPoint)
{
}

void App::Run()
{
	CubeRenderer* cube = new CubeRenderer(m_deviceResources);
	while (true)
	{
		CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(
			CoreProcessEventsOption::ProcessAllIfPresent);

		cube->Update();
		cube->Render();
		m_deviceResources->Present();
	}
}

void App::Uninitialize()
{
}

void App::OnActivated(CoreApplicationView^ appView, IActivatedEventArgs^ args)
{
	CoreWindow^ window = CoreWindow::GetForCurrentThread();
	window->Activate();
}

[MTAThread]
int main(Array<String^>^ args)
{
	CoreApplication::Run(ref new AppSource());
	return 0;
}
