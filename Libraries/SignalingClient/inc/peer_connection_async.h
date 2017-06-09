#pragma once

#include <functional>
#include <vector>
#include <string>
#include <ppl.h>
#include <agents.h>
#include <cpprest/http_client.h>

#include "webrtc/base/logging.h"

namespace SignalingClient
{
    using namespace std;
    using namespace concurrency;
    using namespace web::http;
    using namespace rtc;

    function<http_response(task<http_response>)> HandleError(function<bool(exception)> handler);
    function<http_response(task<http_response>)> LogSev(LoggingSeverity sev, bool abort = true, std::string additional = "");
    function<http_response(task<http_response>)> LogError(std::string additional = "");
    function<http_response(task<http_response>)> LogWarning(std::string additional = "");
    function<http_response(task<http_response>)> LogInfo(std::string additional = "");

    function<http_response(http_response)> ExpectStatus(status_code code, bool andCancel = true);

    function<task<string>(http_response)> ParseBodyA();

    function<vector<string>(string)> SplitString(char separator);
}