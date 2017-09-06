/****************************** Module Header ******************************\
* Module Name:  SampleService.cpp
* Project:      CppWindowsService
* Copyright (c) Microsoft Corporation.
* 
* Provides a sample service class that derives from the service base class - 
* CServiceBase. The sample service logs the service start and stop 
* information to the Application event log, and shows how to run the main 
* function of the service in a thread pool worker thread.
* 
* This source is subject to the Microsoft Public License.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
* 
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

#pragma region Includes
#include "pch.h"
#include "service/render_service.h"
#include "service/thread_pool.h"
#pragma endregion


RenderService::RenderService(
	PWSTR pszServiceName, 
	const std::function<void()>& serviceMainFunc,
	BOOL fCanStop, 
	BOOL fCanShutdown, 
	BOOL fCanPauseContinue) : 
		CServiceBase(
			pszServiceName,
			fCanStop,
			fCanShutdown,
			fCanPauseContinue)
{
    m_fStopping = FALSE;

    // Create a manual-reset event that is not signaled at first to indicate 
    // the stopped signal of the service.
    m_hStoppedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (m_hStoppedEvent == NULL)
    {
        throw GetLastError();
    }

	m_serviceMainFunc = serviceMainFunc;
}


RenderService::~RenderService(void)
{
    if (m_hStoppedEvent)
    {
        CloseHandle(m_hStoppedEvent);
        m_hStoppedEvent = NULL;
    }
}


bool RenderService::InstallService(
	const std::wstring& serviceName,
	const std::wstring& serviceDisplayName,
	const std::wstring& serviceAccount,
	const std::wstring& servicePassword)
{
	wchar_t szPath[MAX_PATH];
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;

	if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)) == 0)
	{
		// GetModuleFileName failed with error
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}

		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}

		return false;
	}

	// Open the local default service control manager database
	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT |
		SC_MANAGER_CREATE_SERVICE);

	if (schSCManager == NULL)
	{
		//OpenSCManager failed with error
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}

		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}

		return false;
	}

	// Install the service into SCM by calling CreateService
	schService = CreateService(
		schSCManager,						// SCManager database
		serviceName.c_str(),				// Name of service
		serviceDisplayName.c_str(),			// Name to display
		SERVICE_QUERY_STATUS,				// Desired access
		SERVICE_WIN32_OWN_PROCESS,			// Service type
		SERVICE_AUTO_START,					// Service start type
		SERVICE_ERROR_NORMAL,				// Error control type
		szPath,								// Service's binary
		NULL,								// No load ordering group
		NULL,								// No tag identifier
		L"",								// Dependencies
		serviceAccount.c_str(),				// Service running account
		servicePassword.c_str()				// Password of the account
	);

	if (schService == NULL)
	{
		// CreateService failed with error
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}

		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}

		return false;
	}

	//Service installed
	return true;
}


bool RenderService::RemoveService(const std::wstring& serviceName)
{
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;
	SERVICE_STATUS ssSvcStatus = {};

	// Open the local default service control manager database
	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (schSCManager == NULL)
	{
		//OpenSCManager failed with error
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}

		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}

		return false;
	}

	// Open the service with delete, stop, and query status permissions
	schService = OpenService(schSCManager, serviceName.c_str(), SERVICE_STOP |
		SERVICE_QUERY_STATUS | DELETE);

	if (schService == NULL)
	{
		//OpenService failed with error
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}

		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}

		return false;
	}

	// Try to stop the service
	if (ControlService(schService, SERVICE_CONTROL_STOP, &ssSvcStatus))
	{
		wprintf(L"Stopping %s.", serviceName);
		Sleep(1000);

		while (QueryServiceStatus(schService, &ssSvcStatus))
		{
			if (ssSvcStatus.dwCurrentState == SERVICE_STOP_PENDING)
			{
				wprintf(L".");
				Sleep(1000);
			}
			else
			{
				break;
			}
		}
	}

	// Now remove the service by calling DeleteService.
	if (!DeleteService(schService))
	{
		// DeleteService failed with error
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}

		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}

		return false;
	}

	//Service is removed
	return true;
}


//
//   FUNCTION: RenderService::OnStart(DWORD, LPWSTR *)
//
//   PURPOSE: The function is executed when a Start command is sent to the 
//   service by the SCM or when the operating system starts (for a service 
//   that starts automatically). It specifies actions to take when the 
//   service starts. In this code sample, OnStart logs a service-start 
//   message to the Application log, and queues the main service function for 
//   execution in a thread pool worker thread.
//
//   PARAMETERS:
//   * dwArgc   - number of command line arguments
//   * lpszArgv - array of command line arguments
//
//   NOTE: A service application is designed to be long running. Therefore, 
//   it usually polls or monitors something in the system. The monitoring is 
//   set up in the OnStart method. However, OnStart does not actually do the 
//   monitoring. The OnStart method must return to the operating system after 
//   the service's operation has begun. It must not loop forever or block. To 
//   set up a simple monitoring mechanism, one general solution is to create 
//   a timer in OnStart. The timer would then raise events in your code 
//   periodically, at which time your service could do its monitoring. The 
//   other solution is to spawn a new thread to perform the main service 
//   functions, which is demonstrated in this code sample.
//
void RenderService::OnStart(DWORD dwArgc, LPWSTR *lpszArgv)
{
    // Log a service start message to the Application log.
	wchar_t log[1024];
	wsprintf(log, L"%ls in OnStart", m_name);
    WriteEventLogEntry(log, EVENTLOG_INFORMATION_TYPE);

    // Queue the main service function for execution in a worker thread.
    CThreadPool::QueueUserWorkItem(&RenderService::ServiceWorkerThread, this);
}


//
//   FUNCTION: RenderService::ServiceWorkerThread(void)
//
//   PURPOSE: The method performs the main function of the service. It runs 
//   on a thread pool worker thread.
//
void RenderService::ServiceWorkerThread(void)
{
    // Periodically check if the service is stopping.
    while (!m_fStopping)
    {
        // Perform main service function here...
		m_serviceMainFunc();
    }

    // Signal the stopped event.
    SetEvent(m_hStoppedEvent);
}


//
//   FUNCTION: RenderService::OnStop(void)
//
//   PURPOSE: The function is executed when a Stop command is sent to the 
//   service by SCM. It specifies actions to take when a service stops 
//   running. In this code sample, OnStop logs a service-stop message to the 
//   Application log, and waits for the finish of the main service function.
//
//   COMMENTS:
//   Be sure to periodically call ReportServiceStatus() with 
//   SERVICE_STOP_PENDING if the procedure is going to take long time. 
//
void RenderService::OnStop()
{
    // Log a service stop message to the Application log.
	wchar_t log[1024];
	wsprintf(log, L"%ls in OnStop", m_name);
    WriteEventLogEntry(log, EVENTLOG_INFORMATION_TYPE);

    // Indicate that the service is stopping and wait for the finish of the 
    // main service function (ServiceWorkerThread).
    m_fStopping = TRUE;
    if (WaitForSingleObject(m_hStoppedEvent, INFINITE) != WAIT_OBJECT_0)
    {
        throw GetLastError();
    }
}
