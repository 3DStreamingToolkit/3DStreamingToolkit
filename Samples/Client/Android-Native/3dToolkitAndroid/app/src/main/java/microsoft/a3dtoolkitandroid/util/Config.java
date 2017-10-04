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
    public static PeerConnection.TlsCertPolicy tlsCertPolicy = null;
}
