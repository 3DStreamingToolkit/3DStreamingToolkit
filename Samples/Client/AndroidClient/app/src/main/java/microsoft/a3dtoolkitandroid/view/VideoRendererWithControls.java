package microsoft.a3dtoolkitandroid.view;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.support.v4.view.GestureDetectorCompat;
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

import static java.lang.Math.abs;
import static java.lang.Math.max;
import static java.lang.Math.min;
import static microsoft.a3dtoolkitandroid.ConnectActivity.LOG;

/**
 * Created by arrahm on 10/9/2017.
 * Custom video renderer that extends the SurfaceViewRenderer with gesture controls.
 */

public class VideoRendererWithControls extends SurfaceViewRenderer implements SensorEventListener {
    private enum Mode{
        NONE, DRAG, ZOOM
    }

    private Mode mode;

    //For Gesture Control
    double navHeading = 0.0;
    double navPitch = 0.0;
    double fingerDownX = 0.0;
    double fingerDownY = 0.0;
    double zoomFingerDownY = 0.0;
    double dx = 0.0;
    double dy = 0.0;
    double downPitch = 0.0;
    double downHeading = 0.0;
    double[] downLocation = { 0.0, 0.0, 0.0 };
    double[] navLocation = { 0.0, 0.0, 0.0 };

    double prevNavHeading = 0.0;
    double prevNavPitch = 0.0;

    // Gravity rotational data
    private float gravity[];
    // Magnetic rotational data
    private float magnetic[]; //for magnetic rotational data
    private float accels[] = new float[3];
    private float mags[] = new float[3];
    private float[] values = new float[3];

    // yaw, pitch and roll
    float yaw;
    float pitch;
    float roll;
    float prevYaw = 0.0f;
    float prevPitch = 0.0f;
    float prevRoll = 0.0f;

    private MatrixMath matrixMath = new MatrixMath();
    private Matrix navTransform = matrixMath.MatrixCreateIdentity();
    private OnMotionEventListener mListener;
    private GestureDetectorCompat mDetector;
    private double scaleFactor = 0.0;
    boolean twoFingers = false;



    public VideoRendererWithControls(Context context) {
        super(context);
    }

    public VideoRendererWithControls(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setEventListener(OnMotionEventListener eventListener, Context context) {
        this.mListener = eventListener;
    }


    private void beginTouch(MotionEvent event) {
        mode = Mode.DRAG;
        fingerDownX = event.getX(0);
        fingerDownY = event.getY(0);
        downPitch = navPitch;
        downHeading = navHeading;
        downLocation[0] = navLocation[0];
        downLocation[1] = navLocation[1];
        downLocation[2] = navLocation[2];
    }

    /**
     * Takes a touch event and transforms it into a navigation matrix
     * @param event: touch event
     * @return true, unless listener hasn't been set
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mListener == null) {
            return false;
        }
        switch(event.getAction() & MotionEvent.ACTION_MASK) {
            case (MotionEvent.ACTION_DOWN) :
                Log.d(LOG, "onTouchEvent: ACTION_DOWN ");
                beginTouch(event);
                break;
            case (MotionEvent.ACTION_MOVE) :
                double dx = event.getX(0) - fingerDownX;
                double dy = event.getY(0) - fingerDownY;
                if (mode == Mode.DRAG && (abs(dx) + abs(dy) > 20)){
                    double dpitch = 0.003 * dy;
                    double dheading = 0.003 * dx;

                    navHeading = downHeading - dheading;
                    navPitch = downPitch + dpitch;

                    Matrix locTransform =  matrixMath.MatrixMultiply(matrixMath.MatrixRotateY(navHeading), matrixMath.MatrixRotateZ(navPitch));
                    navTransform = matrixMath.MatrixMultiply(matrixMath.MatrixTranslate(navLocation), locTransform);

                    toBuffer();

                } else if (mode == Mode.ZOOM ){
                    double scaleFactor = event.getY(1) - zoomFingerDownY;
                    if (abs(scaleFactor) > 10){

                        double distance = -.3 * scaleFactor;

                        navLocation[0] = downLocation[0] + distance * navTransform.get(0,0);
                        navLocation[1] = downLocation[1] + distance * navTransform.get(0,1);
                        navLocation[2] = downLocation[2] + distance * navTransform.get(0,2);

                        navTransform.set(3,0, navLocation[0]);
                        navTransform.set(3,1, navLocation[1]);
                        navTransform.set(3,2, navLocation[2]);

                        toBuffer();
                    }
                }
                break;
            case (MotionEvent.ACTION_POINTER_DOWN):
                Log.d(LOG, "onTouchEvent: ACTION_POINTER_DOWN ");
                mode = Mode.ZOOM;
                zoomFingerDownY = event.getY(1);
                break;
            case (MotionEvent.ACTION_UP) :
                Log.d(LOG, "onTouchEvent: ACTION_UP ");
                mode = Mode.NONE;
                break;
            case (MotionEvent.ACTION_POINTER_UP):
                Log.d(LOG, "onTouchEvent: ACTION_POINTER_UP ");
                beginTouch(event);
                break;
        }

        return true;
    }

    /**
     * Takes movement data (from matrix) and creates a byte buffer to send to the server
     */
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


    @Override
    public void onSensorChanged(SensorEvent event) {
//        switch (event.sensor.getType()) {
//            case Sensor.TYPE_MAGNETIC_FIELD:
//                mags = event.values.clone();
//                break;
//            case Sensor.TYPE_ACCELEROMETER:
//                accels = event.values.clone();
//                break;
//        }
//
//        if (mags != null && accels != null) {
//            gravity = new float[9];
//            magnetic = new float[9];
//            SensorManager.getRotationMatrix(gravity, magnetic, accels, mags);
//            float[] outGravity = new float[9];
//            SensorManager.remapCoordinateSystem(gravity, SensorManager.AXIS_X, SensorManager.AXIS_Z, outGravity);
//            SensorManager.getOrientation(outGravity, values);
//
//            prevYaw = yaw;
//            prevPitch = pitch;
//            prevRoll = roll;
//
//            yaw = values[0] * 57.2957795f;
//            pitch = values[1] * 57.2957795f;
//            roll = values[2] * 57.2957795f;
//
//
//            float dheading = yaw - prevYaw;
//            float dpitch = pitch - prevPitch;
//            float droll = roll - prevRoll;
//
//            mags = null;
//            accels = null;
//
////            if (abs(dheading) < 10 && abs(dpitch) < 10) {
////                return;
////            }
//
//            prevNavHeading = navHeading;
//            prevNavPitch = navPitch;
//
//            navPitch = prevNavPitch - dpitch;
//            navHeading = prevNavHeading - dheading;
//
//            if (i == 7){
//                Log.d(LOG, "onSensorChanged: azimuth = " + yaw + " | Pitch = " + pitch + " | Roll = " + roll);
//                Log.d(LOG, "onSensorChanged: dheading = " + abs(dheading) + " | dpitch = " + abs(dpitch));
//                Log.d(LOG, "onSensorChanged: navPitch = " + navPitch + " | navHeading = " + navHeading);
//                Log.d(LOG, "\r\n");
//                Log.d(LOG, "\r\n");
//                i = 0;
//            } else {
//                i++;
//            }
//
//
//            Matrix locTransform =  matrixMath.MatrixMultiply(matrixMath.MatrixRotateY(navHeading), matrixMath.MatrixRotateZ(navPitch));
//            navTransform = matrixMath.MatrixMultiply(matrixMath.MatrixTranslate(navLocation), locTransform);
//
//            toBuffer();
//        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {

    }

    public interface OnMotionEventListener {
        void sendTransform(DataChannel.Buffer server);
    }
}
