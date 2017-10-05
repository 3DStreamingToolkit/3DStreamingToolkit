package microsoft.a3dtoolkitandroid;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.webrtc.MediaStream;
import org.webrtc.PeerConnection;
import org.webrtc.VideoRenderer;
import org.webrtc.VideoTrack;

import microsoft.a3dtoolkitandroid.util.renderer.EglBase;
import microsoft.a3dtoolkitandroid.util.renderer.SurfaceViewRenderer;

import static microsoft.a3dtoolkitandroid.ServerListActivity.ERROR;


/**
 * A simple {@link Fragment} subclass.
 * Activities that contain this fragment must implement the
 * {@link VideoStreamFragmentInteractionListener} interface
 * to handle interaction events.
 * Use the {@link VideoStreamFragment#newInstance} factory method to
 * create an instance of this fragment.
 */
public class VideoStreamFragment extends Fragment {
    private SurfaceViewRenderer remoteVideoRenderer;
    private final EglBase rootEglBase = EglBase.create();
    private static VideoTrack remoteVideoTrack;


    private VideoStreamFragmentInteractionListener mListener;

    public VideoStreamFragment() {
        // Required empty public constructor
    }

    /**
     * Use this factory method to create a new instance of
     * this fragment using the provided parameters.
     *
     * @return A new instance of fragment VideoStreamFragment.
     */
    public static VideoStreamFragment newInstance() {
        VideoStreamFragment fragment = new VideoStreamFragment();
        Bundle args = new Bundle();
        fragment.setArguments(args);
        return fragment;
    }

    public void onAddStream(PeerConnection peerConnection, MediaStream mediaStream){
        remoteVideoRenderer = (SurfaceViewRenderer) getView().findViewById(R.id.remote_video_view);
        remoteVideoRenderer.init(rootEglBase.getEglBaseContext(), null);
        //set the video renderer to visible
        if (peerConnection == null) {
            Log.d(ERROR, "Peer Connection is null");
        }
        if (mediaStream.audioTracks.size() > 1 || mediaStream.videoTracks.size() > 1) {
            Log.d(ERROR, "Weird-looking stream: " + mediaStream);
        }
        if (mediaStream.videoTracks.size() == 1) {
            //add the video renderer to returned streams video track to display video stream
            remoteVideoTrack = mediaStream.videoTracks.get(0);
            remoteVideoTrack.setEnabled(true);
            remoteVideoTrack.addRenderer(new VideoRenderer(remoteVideoRenderer));
        }
    }

    public void disconnect() {
        if (remoteVideoRenderer != null) {
            remoteVideoRenderer.release();
            remoteVideoRenderer = null;
        }
        if(rootEglBase != null){
            rootEglBase.release();
        }
        remoteVideoTrack = null;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        //create video renderer and initialize it

    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        // Inflate the layout for this fragment
        View view = inflater.inflate(R.layout.fragment_video_stream, container, false);
        return view;
    }

    // TODO: Rename method, update argument and hook method into UI event
    public void onButtonPressed(Uri uri) {
        if (mListener != null) {
            mListener.onFragmentInteraction(uri);
        }
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        if (context instanceof VideoStreamFragmentInteractionListener) {
            mListener = (VideoStreamFragmentInteractionListener) context;
        } else {
            throw new RuntimeException(context.toString()
                    + " must implement VideoStreamFragmentInteractionListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();
        mListener = null;
    }

    /**
     * This interface must be implemented by activities that contain this
     * fragment to allow an interaction in this fragment to be communicated
     * to the activity and potentially other fragments contained in that
     * activity.
     * <p>
     * See the Android Training lesson <a href=
     * "http://developer.android.com/training/basics/fragments/communicating.html"
     * >Communicating with Other Fragments</a> for more information.
     */
    public interface VideoStreamFragmentInteractionListener {
        void onFragmentInteraction(Uri uri);
    }
}
