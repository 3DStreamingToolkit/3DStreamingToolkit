package util;

import com.google.gson.Gson;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by arrahm on 10/11/2017.
 */

public class SharedTestStrings {
    public static String mockServerListString = "myclient,1,1\nrenderingserver1,2,1\nrenderingserver1,3,1\nrenderingserver1,4,1";
    public static String mockServer = "http.mockServer.com";
    public static String mockSdpAnswer = "{\n" + "   \"sdp\" : \"v=0\\r\\no=- 5143951714993185809 2 IN IP4 127.0.0.1\\r\\ns=-\\r\\nt=0 0\\r\\na=group:BUNDLE video data\\r\\na=msid-semantic: WMS stream_label\\r\\nm=video 9 UDP/TLS/RTP/SAVPF 100 96 97 98 99 101 127 125 104\\r\\nc=IN IP4 0.0.0.0\\r\\na=rtcp:9 IN IP4 0.0.0.0\\r\\na=ice-ufrag:3ElO\\r\\na=ice-pwd:Ll+l7UBULmFhh5pFoRA9oJvp\\r\\na=fingerprint:sha-256 86:C6:19:D9:53:EB:09:E2:A0:E0:F2:26:07:F8:2D:90:C7:3C:D0:F3:AB:0D:C6:D8:B2:F6:22:5B:C0:73:50:4E\\r\\na=setup:active\\r\\na=mid:video\\r\\na=extmap:2 urn:ietf:params:rtp-hdrext:toffset\\r\\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\\r\\na=extmap:4 urn:3gpp:video-orientation\\r\\na=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\\r\\na=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\\r\\na=sendonly\\r\\na=rtcp-mux\\r\\na=rtcp-rsize\\r\\na=rtpmap:100 H264/90000\\r\\na=rtcp-fb:100 ccm fir\\r\\na=rtcp-fb:100 nack\\r\\na=rtcp-fb:100 nack pli\\r\\na=rtcp-fb:100 goog-remb\\r\\na=rtcp-fb:100 transport-cc\\r\\na=fmtp:100 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\\r\\na=rtpmap:96 VP8/90000\\r\\na=rtcp-fb:96 ccm fir\\r\\na=rtcp-fb:96 nack\\r\\na=rtcp-fb:96 nack pli\\r\\na=rtcp-fb:96 goog-remb\\r\\na=rtcp-fb:96 transport-cc\\r\\na=rtpmap:97 rtx/90000\\r\\na=fmtp:97 apt=96\\r\\na=rtpmap:98 VP9/90000\\r\\na=rtcp-fb:98 ccm fir\\r\\na=rtcp-fb:98 nack\\r\\na=rtcp-fb:98 nack pli\\r\\na=rtcp-fb:98 goog-remb\\r\\na=rtcp-fb:98 transport-cc\\r\\na=rtpmap:99 rtx/90000\\r\\na=fmtp:99 apt=98\\r\\na=rtpmap:101 rtx/90000\\r\\na=fmtp:101 apt=100\\r\\na=rtpmap:127 red/90000\\r\\na=rtpmap:125 rtx/90000\\r\\na=fmtp:125 apt=127\\r\\na=rtpmap:104 ulpfec/90000\\r\\na=ssrc-group:FID 1037776739 1578719390\\r\\na=ssrc:1037776739 cname:rq1mB44TtGQfiYGn\\r\\na=ssrc:1037776739 msid:stream_label video_label\\r\\na=ssrc:1037776739 mslabel:stream_label\\r\\na=ssrc:1037776739 label:video_label\\r\\na=ssrc:1578719390 cname:rq1mB44TtGQfiYGn\\r\\na=ssrc:1578719390 msid:stream_label video_label\\r\\na=ssrc:1578719390 mslabel:stream_label\\r\\na=ssrc:1578719390 label:video_label\\r\\nm=application 9 DTLS/SCTP 5000\\r\\nc=IN IP4 0.0.0.0\\r\\nb=AS:30\\r\\na=ice-ufrag:3ElO\\r\\na=ice-pwd:Ll+l7UBULmFhh5pFoRA9oJvp\\r\\na=fingerprint:sha-256 86:C6:19:D9:53:EB:09:E2:A0:E0:F2:26:07:F8:2D:90:C7:3C:D0:F3:AB:0D:C6:D8:B2:F6:22:5B:C0:73:50:4E\\r\\na=setup:active\\r\\na=mid:data\\r\\na=sctpmap:5000 webrtc-datachannel 1024\\r\\n\",\n" + "   \"type\" : \"answer\"\n" + "}\n";
    public static String mockSdpOffer = "";

    public static JSONObject mockSdpJSON(String mock) {
        JSONObject mockSdp = null;
        try {
            mockSdp = new JSONObject(mock);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return mockSdp;
    }
}
