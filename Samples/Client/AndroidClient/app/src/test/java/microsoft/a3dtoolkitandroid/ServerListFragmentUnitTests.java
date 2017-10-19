package microsoft.a3dtoolkitandroid;

import org.junit.Test;

import static util.SharedTestStrings.*;
import static org.junit.Assert.*;
import static org.mockito.Mockito.*;


/**
 * Created by arrahm on 10/11/2017.
 */

public class ServerListFragmentUnitTests {

    @Test
    public void testFragmentInitialization() throws Exception{
        ServerListFragment mockServerListFragment = new ServerListFragment();
        mockServerListFragment.mListener = mock(ServerListFragment.OnListFragmentInteractionListener.class);
        mockServerListFragment.adapter = mock(ServerListFragment.ServerListRecyclerViewAdapter.class);
        mockServerListFragment.initializeList(mockServerListString);
        assertEquals(3, mockServerListFragment.servers.size());
    }
}
