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
import static org.mockito.Mockito.verify;
import org.junit.Before;


public class ObserverTests {
    private static final String DUMMY_SDP_MID = "sdpMid";
    private static final String DUMMY_SDP = "sdp";
    public static final int SERVER_WAIT = 100;
    public static final int NETWORK_TIMEOUT = 1000;
    ConnectActivity.PeerConnectionObserver peerConnectionObserver;
    ConnectActivity.SDPObserver sdpObserver;
    @Before
    public void setUp() {
        peerConnectionObserver = mock(ConnectActivity.PeerConnectionObserver.class);
        sdpObserver = mock(ConnectActivity.SDPObserver.class);
    }
}