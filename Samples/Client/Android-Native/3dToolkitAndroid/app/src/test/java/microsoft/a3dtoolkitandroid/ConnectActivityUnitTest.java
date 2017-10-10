package microsoft.a3dtoolkitandroid;

import android.content.Intent;


import com.android.volley.*;

import org.junit.Before;
import org.junit.Test;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

/**
 * Example local unit test, which will execute on the development machine (host).
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
public class ConnectActivityUnitTest extends ActivityTestRule{
    public ConnectActivityUnitTest() {
        super(ServerListActivity.class);
    }

    @Before
    public void setUp() {
        RequestQueue mockRequestQueue = mock(RequestQueue.class);
        doNothing().when(mockRequestQueue).add(any(Request.class));

    }

    @Test
    public void addition_isCorrect() throws Exception {
        assertEquals(4, 2 + 2);
    }
}