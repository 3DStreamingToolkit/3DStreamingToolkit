package microsoft.a3dtoolkitandroid;

/**
 * Created by arrahm on 10/10/2017.
 */
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.webrtc.IceCandidate;
import org.webrtc.SessionDescription;


public class ObserverTests {
    private static final String DUMMY_SDP_MID = "sdpMid";
    private static final String DUMMY_SDP = "sdp";
    public static final int SERVER_WAIT = 100;
    public static final int NETWORK_TIMEOUT = 1000;
    ServerListActivity.PeerConnectionObserver peerConnectionObserver;
    ServerListActivity.SDPObserver sdpObserver;
    @Before
    public void setUp() {
        peerConnectionObserver = mock(ServerListActivity.PeerConnectionObserver.class);
        sdpObserver = mock(ServerListActivity.SDPObserver.class);
    }
}