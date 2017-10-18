package microsoft.a3dtoolkitandroid.view;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.support.v4.view.GestureDetectorCompat;
import android.support.v4.view.MotionEventCompat;
import android.util.AttributeSet;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;

import com.badlogic.gdx.*;

import org.json.JSONException;
import org.json.JSONObject;
import org.webrtc.DataChannel;

import java.nio.ByteBuffer;

import Jama.Matrix;
import microsoft.a3dtoolkitandroid.util.MatrixMath;
import microsoft.a3dtoolkitandroid.util.renderer.SurfaceViewRenderer;

import static java.lang.Math.abs;
import static microsoft.a3dtoolkitandroid.ConnectActivity.LOG;

/**
 * Created by arrahm on 10/9/2017.
 * Custom video renderer that extends the SurfaceViewRenderer with gesture controls.
 */

public class VideoRendererWithControls extends SurfaceViewRenderer implements SensorEventListener {
    //For Gesture Control
    double navHeading = 0.0;
    double navPitch = 0.0;
    double[] navLocation = { 0.0, 0.0, 0.0 };
    boolean isFingerDown = false;
    double fingerDownX = 0.0;
    double fingerDownY = 0.0;
    double downPitch = 0.0;
    double downHeading = 0.0;
    double[] downLocation = { 0.0, 0.0, 0.0 };
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
    private Matrix navTransform;
    private OnMotionEventListener mListener;
    private GestureDetectorCompat mDetector;


    int i = 0;


    public VideoRendererWithControls(Context context) {
        super(context);
    }

    public VideoRendererWithControls(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setEventListener(OnMotionEventListener eventListener, Context context) {
        this.mListener = eventListener;
        mDetector = new GestureDetectorCompat(context, new VideoGestureListener());
    }


    @Override
    public boolean onTouchEvent(MotionEvent event){
        this.mDetector.onTouchEvent(event);
        return true;
    }

    private class VideoGestureListener extends GestureDetector.SimpleOnGestureListener {

        @Override
        public boolean onDown(MotionEvent event) {
            Log.d(LOG,"onDown: " + event.toString());
            return true;
        }

        @Override
        public boolean onFling(MotionEvent event1, MotionEvent event2,
                               float velocityX, float velocityY) {
            Log.d(LOG, "onFling: " + event1.toString() + event2.toString());
            return true;
        }
    }



//    /**
//     * Takes a touch event and transforms it into a navigation matrix
//     * @param event: touch event
//     * @return true, unless listener hasn't been set
//     */
//    @Override
//    public boolean onTouchEvent(MotionEvent event) {
//        if (mListener == null) {
//            return false;
//        }
//        int action = MotionEventCompat.getActionMasked(event);
//        MotionEvent.PointerCoords pointerCoords = new MotionEvent.PointerCoords();
//        int pointerCount = event.getPointerCount();
//
//
//        switch(action) {
//            case (MotionEvent.ACTION_DOWN) :
//                Log.d(LOG, "onTouchEvent: ACTION_DOWN; pointerCount = " + pointerCount);
//
//                isFingerDown = true;
//                event.getPointerCoords(0, pointerCoords);
//                fingerDownX = pointerCoords.x;
//                fingerDownY = pointerCoords.y;
//
//                downPitch = navPitch;
//                downHeading = navHeading;
//                downLocation[0] = navLocation[0];
//                downLocation[1] = navLocation[1];
//                downLocation[2] = navLocation[2];
//                return true;
//            case (MotionEvent.ACTION_MOVE) :
//                if (isFingerDown) {
//
//                    if(pointerCount == 1){
//                        Log.d(LOG, "onTouchEvent: ACTION_MOVE, pointerCount == 1");
//
//                        // location of first finger
//                        event.getPointerCoords(0, pointerCoords);
//                        Log.d(LOG, "onTouchEvent: fingerDownX = " + fingerDownX + " | fingerDownY = " + fingerDownY);
//                        Log.d(LOG, "onTouchEvent: pointerCoords.x  = " + pointerCoords.x  + " | pointerCoords.y = " + pointerCoords.y);
//
//                        double dx = pointerCoords.x - fingerDownX;
//                        double dy = pointerCoords.y - fingerDownY;
//
//                        Log.d(LOG, "onTouchEvent: dx = " + dx + " | dy = " + dy);
//
//                        double dpitch = 0.005 * dy;
//                        double dheading = 0.005 * dx;
//
//                        Log.d(LOG, "onTouchEvent: dpitch = " + dpitch + " | dheading = " + dheading);
//
//                        navHeading = downHeading - dheading;
//                        navPitch = downPitch + dpitch;
//
//                        Log.d(LOG, "onTouchEvent: navPitch = " + navPitch + " | navHeading = " + navHeading);
//
//                        Log.d(LOG, "                                                     ---                                                   ");
//
//                        Matrix locTransform =  matrixMath.MatrixMultiply(matrixMath.MatrixRotateY(navHeading), matrixMath.MatrixRotateZ(navPitch));
//                        navTransform = matrixMath.MatrixMultiply(matrixMath.MatrixTranslate(navLocation), locTransform);
//
//                        toBuffer();
//                    } else if(pointerCount == 2){
//                        Log.d(LOG, "onTouchEvent: ACTION_MOVE, pointerCount == 2");
//
//                        // location of second finger
//                        event.getPointerCoords(1,pointerCoords);
//
//                        double dy = pointerCoords.y - fingerDownY;
//
//                        double dist = -dy;
//
//                        navLocation[0] = downLocation[0] + dist * navTransform.get(0,0);
//                        navLocation[1] = downLocation[1] + dist * navTransform.get(0,1);
//                        navLocation[2] = downLocation[2] + dist * navTransform.get(0,2);
//
//                        navTransform.set(3,0, navLocation[0]);
//                        navTransform.set(3,1, navLocation[1]);
//                        navTransform.set(3,2, navLocation[2]);
//
//                        toBuffer();
//                    }
//                }
//                return true;
//            case (MotionEvent.ACTION_UP) :
//                isFingerDown = false;
//                return true;
//            case (MotionEvent.ACTION_CANCEL) :
//                return true;
//            case (MotionEvent.ACTION_OUTSIDE) :
//                return true;
//            default :
//                return super.onTouchEvent(event);
//        }
//    }

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

//    public boolean touchDown (float x, float y, int pointer, int button) {
//        if (pointer > 1) return false;
//
//        if (pointer == 0) {
//            pointer1.set(x, y);
//            gestureStartTime = Gdx.input.getCurrentEventTime();
//            tracker.start(x, y, gestureStartTime);
//            if (Gdx.input.isTouched(1)) {
//                // Start pinch.
//                inTapSquare = false;
//                pinching = true;
//                initialPointer1.set(pointer1);
//                initialPointer2.set(pointer2);
//                longPressTask.cancel();
//            } else {
//                // Normal touch down.
//                inTapSquare = true;
//                pinching = false;
//                longPressFired = false;
//                tapSquareCenterX = x;
//                tapSquareCenterY = y;
//                if (!longPressTask.isScheduled()) Timer.schedule(longPressTask, longPressSeconds);
//            }
//        } else {
//            // Start pinch.
//            pointer2.set(x, y);
//            inTapSquare = false;
//            pinching = true;
//            initialPointer1.set(pointer1);
//            initialPointer2.set(pointer2);
//            longPressTask.cancel();
//        }
//        return listener.touchDown(x, y, pointer, button);
//    }


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
