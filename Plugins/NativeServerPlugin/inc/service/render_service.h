/****************************** Module Header ******************************\
* Module Name:  SampleService.h
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

#pragma once

#include <functional>

#include "service/service_base.h"


class RenderService : public CServiceBase
{
public:
	RenderService(
		PWSTR pszServiceName,
		const std::function<void()>& serviceMainFunc,
        BOOL fCanStop = TRUE, 
        BOOL fCanShutdown = TRUE, 
        BOOL fCanPauseContinue = FALSE);

    virtual ~RenderService(void);

	static bool InstallService(
		const std::wstring& serviceName,
		const std::wstring& serviceDisplayName,
		const std::wstring& serviceAccount,
		const std::wstring& servicePassword);

	static bool RemoveService(const std::wstring& serviceName);

protected:
    virtual void OnStart(DWORD dwArgc, PWSTR *pszArgv);
    virtual void OnStop();
    void ServiceWorkerThread(void);

private:
    BOOL m_fStopping;
    HANDLE m_hStoppedEvent;
	std::function<void()> m_serviceMainFunc;
};
