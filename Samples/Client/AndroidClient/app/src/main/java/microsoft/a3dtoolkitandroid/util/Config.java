package microsoft.a3dtoolkitandroid.util;

import org.webrtc.PeerConnection;

/**
 * Created by Arman on 10/4/2017.
 */

public class Config {
    public static String signalingServer = "http://localhost:3001";
    public static String turnServer = "turn:turn:turnserveruri:5349";
    public static String username = "user";
    public static String credential = "password";
    // Set this to PeerConnection.TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK or
    // PeerConnection.TlsCertPolicy.TLS_CERT_POLICY_SECURE
    public static PeerConnection.TlsCertPolicy tlsCertPolicy = PeerConnection.TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK;
}