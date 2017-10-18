package microsoft.a3dtoolkitandroid.util;

import org.webrtc.PeerConnection;

/**
 * Created by Arman on 10/4/2017.
 */

public class Config {
    public static String signalingServer = "https://3dtoolkit-signaling-server.azurewebsites.net:443";
    public static String turnServer = "turn:turnserver3dstreaming.centralus.cloudapp.azure.com:5349";
    public static String username = "user";
    public static String credential = "3Dtoolkit072017";
    public static PeerConnection.TlsCertPolicy tlsCertPolicy = PeerConnection.TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK;
}