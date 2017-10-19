package microsoft.a3dtoolkitandroid;

/**
 * Created by arrahm on 10/10/2017.
 */

import org.junit.Before;

import static org.mockito.Mockito.mock;


public class ObserverTests {

    ConnectActivity.PeerConnectionObserver peerConnectionObserver;
    ConnectActivity.SDPObserver sdpObserver;
    @Before
    public void setUp() {
        peerConnectionObserver = mock(ConnectActivity.PeerConnectionObserver.class);
        sdpObserver = mock(ConnectActivity.SDPObserver.class);
    }
}