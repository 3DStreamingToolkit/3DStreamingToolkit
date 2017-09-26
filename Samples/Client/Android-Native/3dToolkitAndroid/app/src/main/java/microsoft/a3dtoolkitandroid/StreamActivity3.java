package microsoft.a3dtoolkitandroid;

import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.Window;
import android.view.WindowManager;

import org.webrtc.VideoRenderer;

import java.net.URL;

import apprtc.UnhandledExceptionHandler;

public class StreamActivity3 extends AppCompatActivity {

    private final static int VIDEO_CALL_SENT = 666;
    private static final String VIDEO_CODEC_VP9 = "H264";
    // Remote video screen position
    private static final int REMOTE_X = 0;
    private static final int REMOTE_Y = 0;
    private static final int REMOTE_WIDTH = 100;
    private static final int REMOTE_HEIGHT = 100;
    private GLSurfaceView vsv;
    private VideoRenderer.Callbacks remoteRender;
    private String mSocketAddress;
    private String callerId;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    public void singIn(){
        String urlString = server + "/sign_in?peer_name=" + localName;
    }
}
