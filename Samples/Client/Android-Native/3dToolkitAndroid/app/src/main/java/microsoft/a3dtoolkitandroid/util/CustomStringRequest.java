package microsoft.a3dtoolkitandroid.util;

import com.android.volley.NetworkResponse;
import com.android.volley.ParseError;
import com.android.volley.Request;
import com.android.volley.Response;
import com.android.volley.toolbox.HttpHeaderParser;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.UnsupportedEncodingException;
import java.util.Map;

/**
 * Created by arrahm on 9/26/2017.
 */

public class CustomStringRequest extends Request<CustomStringRequest.ResponseM> {

    private Response.Listener<CustomStringRequest.ResponseM> mListener;

    public CustomStringRequest(int method, String url, Response.Listener<CustomStringRequest.ResponseM> responseListener, Response.ErrorListener listener) {
        super(method, url, listener);
        this.mListener = responseListener;
    }


    @Override
    protected void deliverResponse(ResponseM response) {
        this.mListener.onResponse(response);
    }

    @Override
    protected Response<ResponseM> parseNetworkResponse(NetworkResponse response) {
        JSONObject parsed;
        try {
            String jsonString = new String(response.data, HttpHeaderParser.parseCharset(response.headers));
            if (jsonString.charAt(0) == '{'){
                parsed = new JSONObject(jsonString);
            } else{
                parsed = new JSONObject();
                parsed.put("Server", jsonString);
            }
        } catch (UnsupportedEncodingException e) {
            parsed = new JSONObject();
        } catch (JSONException je) {
            return Response.error(new ParseError(je));
        }

        ResponseM responseM = new ResponseM();
        responseM.headers = response.headers;
        responseM.response = parsed;

        return Response.success(responseM, HttpHeaderParser.parseCacheHeaders(response));
    }


    public static class ResponseM {
        public Map<String, String> headers;
        public JSONObject response;
    }

}