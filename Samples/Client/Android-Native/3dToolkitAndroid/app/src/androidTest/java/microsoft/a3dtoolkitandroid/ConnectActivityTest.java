package microsoft.a3dtoolkitandroid;

import android.content.Intent;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;


import com.android.volley.*;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.webrtc.MediaConstraints;


import microsoft.a3dtoolkitandroid.util.HttpRequestQueue;

import static android.support.test.espresso.Espresso.*;
import static android.support.test.espresso.action.ViewActions.*;
import static android.support.test.espresso.matcher.ViewMatchers.*;
import static microsoft.a3dtoolkitandroid.ServerListActivity.DTLS;
import static microsoft.a3dtoolkitandroid.ServerListActivity.OfferToReceiveAudio;
import static microsoft.a3dtoolkitandroid.ServerListActivity.OfferToReceiveVideo;
import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import static util.SharedTestStrings.*;

@RunWith(AndroidJUnit4.class)
public class ConnectActivityTest extends ActivityTestRule<ServerListActivity> {
    private ServerListActivity activity;
    public ConnectActivityTest() {
        super(ServerListActivity.class);
    }

    @Before
    public void setUp() {
        HttpRequestQueue mockRequestQueue = mock(HttpRequestQueue.class);
        doNothing().when(mockRequestQueue).addToQueue(any(Request.class), anyString());
        HttpRequestQueue.setInstance(mockRequestQueue);
        Intent intent = new Intent();
        intent.putExtra(ConnectActivity.SERVER_LIST, mockServerListString);
        intent.putExtra(ConnectActivity.SERVER_SERVER, mockServer);
        launchActivity(intent);
        activity = getActivity();
    }

    @Test
    public void testStartUpActivity() throws Exception{
        assertTrue(activity.activityRunning);
        assertEquals(mockServer, activity.server);
        assertEquals(mockServerListString, activity.stringList);
    }

    @Test
    public void testCreateMediaConstraints() throws Exception{
        assertNotNull(activity.remoteVideoRenderer);
        assertEquals(1, activity.peerConnectionConstraints.optional.size());
        assertEquals(new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", DTLS), activity.peerConnectionConstraints.optional.get(0));
        assertEquals(2, activity.peerConnectionConstraints.mandatory.size());
        assertEquals(new MediaConstraints.KeyValuePair("OfferToReceiveAudio", OfferToReceiveAudio), activity.peerConnectionConstraints.mandatory.get(0));
        assertEquals(new MediaConstraints.KeyValuePair("OfferToReceiveVideo", OfferToReceiveVideo), activity.peerConnectionConstraints.mandatory.get(1));
    }

    @Test
    public void testServerListFragment() throws Exception{
        assertNotNull(activity.serverListFragment);
        assertEquals(1, activity.my_id);
//        onView(withId(R.id.ServerList))
//                .perform(actionOnItemAtPosition(0, click()));
        assertEquals(3, activity.serverListFragment.servers.size());
    }

}