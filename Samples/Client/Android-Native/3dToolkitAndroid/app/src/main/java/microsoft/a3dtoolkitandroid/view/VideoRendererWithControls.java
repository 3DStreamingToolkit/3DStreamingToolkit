package microsoft.a3dtoolkitandroid.view;

import android.content.Context;
import android.support.v4.view.MotionEventCompat;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;

import org.json.JSONException;
import org.json.JSONObject;
import org.webrtc.DataChannel;

import java.nio.ByteBuffer;

import Jama.Matrix;
import microsoft.a3dtoolkitandroid.util.MatrixMath;
import microsoft.a3dtoolkitandroid.util.renderer.SurfaceViewRenderer;

import static microsoft.a3dtoolkitandroid.ConnectActivity.LOG;

/**
 * Created by arrahm on 10/9/2017.
 */

public class VideoRendererWithControls extends SurfaceViewRenderer {
    //For Gesture Control
    double navHeading = 0;
    double navPitch = 0;
    double[] navLocation = { 0, 0, 0 };
    boolean isFingerDown = false;
    double fingerDownX;
    double fingerDownY;
    double downPitch = 0;
    double downHeading = 0;
    double[] downLocation = { 0, 0, 0 };
    private MatrixMath mat = new MatrixMath();
    private Matrix navTransform;
    private OnMotionEventListener mListener;


    public VideoRendererWithControls(Context context) {
        super(context);
    }

    public VideoRendererWithControls(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setEventListener(OnMotionEventListener eventListener) {
        this.mListener = eventListener;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mListener == null) {
            return false;
        }
        int action = MotionEventCompat.getActionMasked(event);

        switch(action) {
            case (MotionEvent.ACTION_DOWN) :
                Log.d(LOG,"Action was DOWN");
                isFingerDown = true;
                fingerDownX = event.getRawX();
                fingerDownY = event.getRawY();

                downPitch = navPitch;
                downHeading = navHeading;
                downLocation[0] = navLocation[0];
                downLocation[1] = navLocation[1];
                downLocation[2] = navLocation[2];
                return true;
            case (MotionEvent.ACTION_MOVE) :
                Log.d(LOG,"Action was MOVE");
                if (isFingerDown) {
                    int pointerCount = event.getPointerCount();

                    if(pointerCount == 1){
                        // location of first finger
                        MotionEvent.PointerCoords pointerCoords = new MotionEvent.PointerCoords();
                        event.getPointerCoords(0,pointerCoords);

                        double dx = pointerCoords.x - fingerDownX;
                        double dy = pointerCoords.y - fingerDownY;

                        double dpitch = 0.005 * dy;
                        double dheading = 0.005 * dx;

                        navHeading = downHeading - dheading;
                        navPitch = downPitch + dpitch;

                        Matrix locTransform =  mat.MatrixMultiply(mat.MatrixRotateY(navHeading), mat.MatrixRotateZ(navPitch));
                        navTransform = mat.MatrixMultiply(mat.MatrixTranslate(navLocation), locTransform);

                        toBuffer();
                    } else if(pointerCount == 2){
                        // location of second finger
                        MotionEvent.PointerCoords pointerCoords = new MotionEvent.PointerCoords();
                        event.getPointerCoords(1,pointerCoords);

                        double dy = pointerCoords.y - fingerDownY;

                        double dist = -dy;

                        navLocation[0] = downLocation[0] + dist * navTransform.get(0,0);
                        navLocation[1] = downLocation[1] + dist * navTransform.get(0,1);
                        navLocation[2] = downLocation[2] + dist * navTransform.get(0,2);

                        navTransform.set(3,0, navLocation[0]);
                        navTransform.set(3,1, navLocation[1]);
                        navTransform.set(3,2, navLocation[2]);

                        toBuffer();
                    }
                }
                return true;
            case (MotionEvent.ACTION_UP) :
                Log.d(LOG,"Action was UP");
                isFingerDown = false;
                return true;
            case (MotionEvent.ACTION_CANCEL) :
                Log.d(LOG,"Action was CANCEL");
                return true;
            case (MotionEvent.ACTION_OUTSIDE) :
                Log.d(LOG,"Movement occurred outside bounds " +
                        "of current screen element");
                return true;
            default :
                return super.onTouchEvent(event);
        }
    }

    private void toBuffer(){
        if (mListener == null) {
            return;
        }
        double[] eye = {navTransform.get(3,0), navTransform.get(3,1), navTransform.get(3,2)};
        double[] lookat = {navTransform.get(3,0) + navTransform.get(0,0), navTransform.get(3,1) + navTransform.get(0,1), navTransform.get(3,2) + navTransform.get(0,2)};
        double[] up = {navTransform.get(1,0), navTransform.get(1,1), navTransform.get(1,2)};

        String data = eye[0] + ", " + eye[1] + ", " + eye[2] + ", " +
                lookat[0] + ", " + lookat[1] + ", " + lookat[2] + ", " +
                up[0] + ", " + up[1] + ", " + up[2];

        JSONObject jsonObject = new JSONObject();
        try {
            jsonObject.put("type", "camera-transform-lookat");
            jsonObject.put("body", data);
        } catch (JSONException e) {
            e.printStackTrace();
        }

        ByteBuffer byteBuffer = ByteBuffer.wrap(jsonObject.toString().getBytes());
        DataChannel.Buffer buffer = new DataChannel.Buffer(byteBuffer, false);
        mListener.sendTransform(buffer);
    }

    public interface OnMotionEventListener {
        void sendTransform(DataChannel.Buffer server);
    }
}
