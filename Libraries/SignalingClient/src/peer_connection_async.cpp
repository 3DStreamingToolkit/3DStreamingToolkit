#include "peer_connection_async.h"
namespace SignalingClient
{
    function<http_response(task<http_response>)> HandleError(function<bool(exception)> handler)
    {
        return [handler](task<http_response> prevTask)
        {
            try
            {
                return prevTask.get();
            }
            catch (const exception& ex)
            {
                if (handler(ex))
                {
                    cancel_current_task();
                }
            }

            return http_response();
        };
    }

    function<http_response(task<http_response>)> LogSev(LoggingSeverity sev, bool abort, std::string additional)
    {
        return HandleError([sev, abort, additional](exception ex) {
            LOG_V(sev) << ex.what() << (additional.length() > 0 ? " " : "") << additional;

            // and abort (since this is an error)
            return abort;
        });
    }

    function<http_response(task<http_response>)> LogError(std::string additional)
    {
        return LogSev(LoggingSeverity::LS_ERROR, true, additional);
    }

    function<http_response(task<http_response>)> LogWarning(std::string additional)
    {
        return LogSev(LoggingSeverity::LS_WARNING, false, additional);
    }

    function<http_response(task<http_response>)> LogInfo(std::string additional)
    {
        return LogSev(LoggingSeverity::LS_WARNING, false, additional);
    }

    function<http_response(http_response)> ExpectStatus(status_code code, bool andCancel)
    {
        return [code, andCancel](http_response res)
        {
            auto resCode = res.status_code();
            if (resCode != code)
            {
                if (andCancel)
                {
                    cancel_current_task();
                }
                else
                {
                    throw new web::http::http_exception("invalid status_code " + to_string(resCode) + " != " + to_string(code));
                }
            }

            return res;
        };
    }

    function<task<string>(http_response)> ParseBodyA()
    {
        return [](http_response res)
        {
            return res.extract_utf8string();
        };
    }

    function<vector<string>(string)> SplitString(char separator)
    {
        return [separator](string str)
        {
            std::vector<std::string> result;
            auto resultInserter = std::back_inserter(result);
            std::stringstream ss;
            ss.str(str);
            std::string item;
            while (std::getline(ss, item, separator))
            {
                (resultInserter++) = item;
            }
            return result;
        };
    }
}