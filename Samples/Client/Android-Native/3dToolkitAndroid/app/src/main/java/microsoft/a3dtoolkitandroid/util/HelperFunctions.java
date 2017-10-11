package microsoft.a3dtoolkitandroid.util;

import android.content.Context;
import android.util.Log;
import android.widget.Toast;

import com.android.volley.AuthFailureError;
import com.android.volley.RequestQueue;
import com.android.volley.Response;
import com.android.volley.toolbox.StringRequest;
import com.android.volley.toolbox.Volley;
import com.facebook.stetho.okhttp3.StethoInterceptor;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import okhttp3.Interceptor;

import static microsoft.a3dtoolkitandroid.ServerListActivity.ERROR;
import static microsoft.a3dtoolkitandroid.ServerListActivity.LOG;

/**
 * Created by arrahm on 10/9/2017.
 */

public class HelperFunctions {
    public static final String REQUEST_TAG = "SERVER_LIST_ACTIVITY";
    private static Toast logToast;

    /**
     * Log Error message and create toast (popup)
     * @param context: context to create toast in
     * @param message: error message
     */
    public static void logAndToast(Context context, String message) {
        Log.d(ERROR, message);
        if (logToast != null) {
            logToast.cancel();
        }
        logToast = Toast.makeText(context, message, Toast.LENGTH_SHORT);
        logToast.show();
    }

    /**
     * Adds a http request to volley requestQueue.
     *
     * @param url      (String): http url
     * @param method   (int): etc Request.Method.GET or Request.Method.POST
     * @param listener (Response.Listener<String>): custom listener for responses
     */
    public static void addRequest(String url, int method, Context context, Response.Listener<String> listener) {
        // Request a string response from the server
        StringRequest getRequest = new StringRequest(method, url, listener,
                error -> {
                    Log.d(ERROR, "Sorry request did not work!");
                }){
            @Override
            public Map<String, String> getHeaders() throws AuthFailureError {
                Map<String, String> params = new HashMap<>();
                params.put("Peer-Type", "Client");
                return params;
            }
        };
        HttpRequestQueue.getInstance(context).addToQueue(getRequest, REQUEST_TAG);
    }


    /**
     * Changes a SDP description to prefer the given video or audio codec and returns a new description string
     * @param sdpDescription: original SDP description
     * @param codec: Codec name to switch to
     * @param isAudio: Is codec an audio codec?
     * @return updated SDP description string
     */
    public static String preferCodec(String sdpDescription, String codec, boolean isAudio) {
        String[] lines = sdpDescription.split("\r\n");
        int mLineIndex = -1;
        String codecRtpMap = null;
        // a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]
        String regex = "^a=rtpmap:(\\d+) " + codec + "(/\\d+)+[\r]?$";
        Pattern codecPattern = Pattern.compile(regex);
        String mediaDescription = "m=video ";
        if (isAudio) {
            mediaDescription = "m=audio ";
        }
        for (int i = 0; (i < lines.length) && (mLineIndex == -1 || codecRtpMap == null); i++) {
            if (lines[i].startsWith(mediaDescription)) {
                mLineIndex = i;
                continue;
            }
            Matcher codecMatcher = codecPattern.matcher(lines[i]);
            if (codecMatcher.matches()) {
                codecRtpMap = codecMatcher.group(1);
            }
        }
        if (mLineIndex == -1) {
            Log.w(LOG, "No " + mediaDescription + " line, so can't prefer " + codec);
            return sdpDescription;
        }
        if (codecRtpMap == null) {
            Log.w(LOG, "No rtpmap for " + codec);
            return sdpDescription;
        }
        Log.d(LOG, "Found " + codec + " rtpmap " + codecRtpMap + ", prefer at " + lines[mLineIndex]);
        String[] origMLineParts = lines[mLineIndex].split(" ");
        if (origMLineParts.length > 3) {
            StringBuilder newMLine = new StringBuilder();
            int origPartIndex = 0;
            // Format is: m=<media> <port> <proto> <fmt> ...
            newMLine.append(origMLineParts[origPartIndex++]).append(" ");
            newMLine.append(origMLineParts[origPartIndex++]).append(" ");
            newMLine.append(origMLineParts[origPartIndex++]).append(" ");
            newMLine.append(codecRtpMap);
            for (; origPartIndex < origMLineParts.length; origPartIndex++) {
                if (!origMLineParts[origPartIndex].equals(codecRtpMap)) {
                    newMLine.append(" ").append(origMLineParts[origPartIndex]);
                }
            }
            lines[mLineIndex] = newMLine.toString();
            Log.d(LOG, "Change media description: " + lines[mLineIndex]);
        } else {
            Log.e(LOG, "Wrong SDP media description format: " + lines[mLineIndex]);
        }
        StringBuilder newSdpDescription = new StringBuilder();
        for (String line : lines) {
            newSdpDescription.append(line).append("\r\n");
        }
        return newSdpDescription.toString();
    }
}
