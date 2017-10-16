package microsoft.a3dtoolkitandroid;


import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import com.android.volley.Request;
import com.android.volley.VolleyError;
import com.facebook.stetho.Stetho;

import org.json.JSONException;
import org.json.JSONObject;
import org.webrtc.DataChannel;
import org.webrtc.IceCandidate;
import org.webrtc.Logging;
import org.webrtc.MediaConstraints;
import org.webrtc.MediaStream;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.RtpReceiver;
import org.webrtc.SdpObserver;
import org.webrtc.SessionDescription;
import org.webrtc.VideoRenderer;
import org.webrtc.VideoTrack;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;

import microsoft.a3dtoolkitandroid.util.Config;
import microsoft.a3dtoolkitandroid.util.CustomStringRequest;
import microsoft.a3dtoolkitandroid.util.GenericRequest;
import microsoft.a3dtoolkitandroid.util.HttpRequestQueue;
import microsoft.a3dtoolkitandroid.util.renderer.EglBase;
import microsoft.a3dtoolkitandroid.view.VideoRendererWithControls;

import static java.lang.Integer.parseInt;
import static microsoft.a3dtoolkitandroid.util.HelperFunctions.REQUEST_TAG;
import static microsoft.a3dtoolkitandroid.util.HelperFunctions.addRequest;
import static microsoft.a3dtoolkitandroid.util.HelperFunctions.logAndToast;
import static microsoft.a3dtoolkitandroid.util.HelperFunctions.preferCodec;

public class ConnectActivity extends AppCompatActivity implements
        ServerListFragment.OnListFragmentInteractionListener{

    public static final String LOG = "ServerListLog";
    public static final String ERROR = "ServerListLogError";
    public static final String DTLS = "true";
    public static final String OfferToReceiveAudio = "false";
    public static final String OfferToReceiveVideo= "true";


    private final int heartbeatIntervalInMs = 5000;
    public Intent intent;
    public int my_id = -1;
    public int peer_id = -1;
    public String server;
    public String stringList;
    public boolean isInitiator;
    public boolean activityRunning;

    // WebRTC Variables
    public PeerConnection peerConnection;
    public PeerConnectionFactory peerConnectionFactory;
    public DataChannel inputChannel;
    public VideoTrack remoteVideoTrack;
    public final PeerConnectionObserver peerConnectionObserver = new PeerConnectionObserver();
    public final SDPObserver sdpObserver = new SDPObserver();
    public MediaConstraints sdpMediaConstraints;
    public MediaConstraints peerConnectionConstraints;
    public SessionDescription localSdp; // either offer or answer SDP
    public VideoRendererWithControls remoteVideoRenderer;
    public final EglBase rootEglBase = EglBase.create();

    //Fragment variables
    public FragmentManager fragmentManager;
    public ServerListFragment serverListFragment;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Stetho.initializeWithDefaults(this);
        setContentView(R.layout.activity_server_list);
        intent = getIntent();
        server = intent.getStringExtra(SignInActivity.SERVER_SERVER);
        fragmentManager = getSupportFragmentManager();
    }

    /**
     * Action for when a server is clicked from {@link ServerListFragment}
     */
    @Override
    public void onServerClicked(int server) {
        peer_id = server;
        joinPeer();
    }

    /**
     * Sets myID when the server list is initialized from {@link ServerListFragment}
     */
    @Override
    public void addMyID(int myID) {
        my_id = myID;
    }

    /**
     * Starts the setup process for connecting to a client.
     */
    private void startUpActivity(){
        activityRunning = true;

        // initialize media
        createMediaConstraints();

        // loop sending out heartbeat signals and loop hanging get to receive server responses
        startHangingGet();
        startHeartBeat();

        // Set up server list
        setUpListView();
    }

    /**
     * Sets up the server list view and click listener
     */
    private void setUpListView(){
        // get the string response from the login request in the Connect Activity
        stringList = intent.getStringExtra(SignInActivity.SERVER_LIST);

        // initialize list fragment with server list string
        fragmentManager = getSupportFragmentManager();
        fragmentManager.beginTransaction()
                .add(R.id.server_list_fragment_container, ServerListFragment.newInstance(), "ServerListFragment")
                .commit();
        fragmentManager.executePendingTransactions();
        serverListFragment = (ServerListFragment) fragmentManager.findFragmentByTag("ServerListFragment");
        serverListFragment.initializeList(stringList);
    }

    /**
     * Sets up peer connection constraints and sdp constraints for connection
     */
    private void createMediaConstraints() {
        runOnUiThread(() -> {
                    //Android flags for video UI
                    getWindow().addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD | WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED
                            | WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
                    getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);

                    //create video renderer and initialize it
                    remoteVideoRenderer = findViewById(R.id.remote_video_view);
                    remoteVideoRenderer.init(rootEglBase.getEglBaseContext(), null);
                    remoteVideoRenderer.setEventListener(new VideoRendererWithControls.OnMotionEventListener() {
                        /**
                         * Sends gesture control buffer from {@link VideoRendererWithControls}
                         *
                         * @param buffer: DataChannel buffer to send to server
                         */
                        @Override
                        public void sendTransform(DataChannel.Buffer buffer) {
                            if (inputChannel != null) {
                                inputChannel.send(buffer);
                            }
                        }
                    });
                });

        // Create peer connection constraints.
        peerConnectionConstraints = new MediaConstraints();

        // Enable DTLS for normal calls and disable for loopback calls.
        peerConnectionConstraints.optional.add(new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", DTLS));

        //Create SDP constraints.
        sdpMediaConstraints = new MediaConstraints();
        sdpMediaConstraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveAudio", OfferToReceiveAudio));
        sdpMediaConstraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveVideo", OfferToReceiveVideo));
    }

    /**
     * Joins server selected from list of peers
     */
    public void joinPeer() {
        createPeerConnection();
        inputChannel = peerConnection.createDataChannel("inputDataChannel", new DataChannel.Init());
        inputChannel.registerObserver(new DataChannel.Observer() {
            @Override
            public void onBufferedAmountChange(long l) {

            }

            @Override
            public void onStateChange() {

            }

            @Override
            public void onMessage(DataChannel.Buffer buffer) {

            }
        });
        createOffer();
    }

    /**
     * Creates a peer connection using the ICE server and media constraints specified
     */
    public void createPeerConnection() {
        // show the video renderer and set the screen to landscape
        runOnUiThread(() -> {
            remoteVideoRenderer.setVisibility(View.VISIBLE);
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        });

        if (peerConnectionFactory == null){
            //Initialize PeerConnectionFactory globals.
            //Params are context, initAudio, initVideo and videoCodecHwAcceleration
            PeerConnectionFactory.initializeAndroidGlobals(this, false, true, true);
            peerConnectionFactory = new PeerConnectionFactory(null);
        }

        // add initial ICE Server from Config File
        List<PeerConnection.IceServer> iceServerList = new ArrayList<>();
        iceServerList.add(new PeerConnection.IceServer(Config.turnServer, Config.username, Config.credential, Config.tlsCertPolicy));

        // TCP candidates are only useful when connecting to a server that supports ICE-TCP.
        PeerConnection.RTCConfiguration rtcConfig = new PeerConnection.RTCConfiguration(iceServerList);
        rtcConfig.iceTransportsType = PeerConnection.IceTransportsType.RELAY;

        if (peerConnection == null){
            peerConnection = peerConnectionFactory.createPeerConnection(rtcConfig, peerConnectionConstraints, peerConnectionObserver);
        }

        // Set default WebRTC tracing and INFO libjingle logging.
        // NOTE: this _must_ happen while |factory| is alive!
        Logging.enableTracing("logcat:", EnumSet.of(Logging.TraceLevel.TRACE_DEFAULT));
        Logging.enableLogToDebugOutput(Logging.Severity.LS_INFO);

    }

    /**
     * Sends POST request to server with JSON data represented as a Map<String, String>.
     *
     * @param params (HashMap<String, String></String,>): json post data to send to server
     */
    private void sendToPeer(HashMap<String, String> params) {
        try {
            if (my_id == -1) {
                logAndToast(this, "sendToPeer: Not Connected");
                return;
            }
            if (peer_id == my_id) {
                logAndToast(this, "sendToPeer: Can't send a message to oneself :)");
                return;
            }

            Map<String, String> headerParams = new HashMap<>();
            headerParams.put("Peer-Type", "Client");

            GenericRequest<String> getRequest = new GenericRequest<>(Request.Method.POST ,server + "/message?peer_id=" + my_id + "&to=" + peer_id, String.class, params,
                    response -> {
                    }, error -> logAndToast(this, "onErrorResponse: SendToPeer = " + error), headerParams, true, true);

            // Add the request to the RequestQueue.
            HttpRequestQueue.getInstance(this).addToQueue(getRequest, REQUEST_TAG);
        } catch (Throwable e) {
            logAndToast(this, "send to peer error: " + e.toString());
        }
    }

    /**
     * Handles messages from the server (offer, answer, adding ICE candidate).
     *
     * @param data: JSON response from server
     * @throws JSONException
     */
    private void handlePeerMessage(JSONObject data) throws JSONException {
        Log.d(LOG, "handlePeerMessage: Message from '" + peer_id + ":" + data);

        //first check if the json object has a field "type"
        if (data.has("type")) {
            if (data.getString("type").equals("offer")) {
                createPeerConnection();
                peerConnection.setRemoteDescription(sdpObserver, new SessionDescription(SessionDescription.Type.OFFER, data.getString("sdp")));
                createAnswer();
            } else if (data.getString("type").equals("answer")) {
                isInitiator = true;
                peerConnection.setRemoteDescription(sdpObserver, new SessionDescription(SessionDescription.Type.ANSWER, data.getString("sdp")));
            }
        } else {
            IceCandidate candidate = new IceCandidate(data.getString("sdpMid"), data.getInt("sdpMLineIndex"), data.getString("candidate"));
            peerConnection.addIceCandidate(candidate);
        }
    }

    /**
     * Handles adding new servers to the list of servers
     *
     * @param server: e.g. "renderingserver_phcherne@PHCHERNE-PR04,941,1"
     */
    private void handleServerNotification(String server) {
        String[] parsed = server.trim().split("\\s*,\\s*");
        if (parseInt(parsed[2]) != 0 && serverListFragment != null) {
            serverListFragment.addPeerList(parseInt(parsed[1]), parsed[0]);
        }
    }

    /**
     * Handles adding new servers to list of severs and handles peer messages with the server (offer, answer, adding ICE candidate).
     * Loops on request timeout.
     */
    private void startHangingGet() {
        // stop hanging get if activity is closed
        if (!activityRunning){
            return;
        }
        // Created a custom request class to access headers of responses.
        CustomStringRequest stringRequest = new CustomStringRequest(Request.Method.GET, server + "/wait?peer_id=" + my_id,
                this::onHangingGetSuccess, this::onHangingGetFailure);
        HttpRequestQueue.getInstance(this).addToQueue(stringRequest, REQUEST_TAG);

    }

    /**
     * Callback for a sucessfull response from server
     * @param result: custom response object that allows access to response headers
     */
    public void onHangingGetSuccess(CustomStringRequest.ResponseM result){
        if (!result.headers.containsKey("Pragma")){
            logAndToast(this, "No Pragma Field");
            return;
        }
        //get id from header
        String get_id_string = result.headers.get("Pragma");
        int get_id = parseInt(get_id_string);

        JSONObject response = result.response;
        // if the ID returned is equal to my_id then it is message from a server notifying it's presence
        if (get_id == my_id) {
            // Update the list of peers
            try {
                handleServerNotification(response.getString("Server"));
            } catch (JSONException e) {
                e.printStackTrace();
            }
        } else {
            // Handle other messages from server and set peer_id
            peer_id = get_id;
            try {
                handlePeerMessage(response);
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }

        // keep looping hanging get to wait for any message from the server
        if (my_id != -1){
            startHangingGet();
        }
    }

    /**
     * Callback for a failed response from server
     * @param error: server error
     */
    public void onHangingGetFailure(VolleyError error){
        // keep looping on request timeout
        if (error.toString().equals("com.android.volley.TimeoutError")) {
            startHangingGet();
        } else {
            logAndToast(this, "startHangingGet: ERROR" + error.toString());
        }
    }

    /**
     * Sends heartBeat request to server at a regular interval
     */
    private void startHeartBeat() {
        new Timer().scheduleAtFixedRate(new TimerTask() {
            @Override
            public void run() {
                addRequest(server + "/heartbeat?peer_id=" + my_id, Request.Method.GET, ConnectActivity.this, response -> {});
            }
        }, 0, heartbeatIntervalInMs);
    }

    /**
     * Create sdp offer
     */
    public void createOffer() {
        if (peerConnection != null) {
            isInitiator = true;
            peerConnection.createOffer(sdpObserver, sdpMediaConstraints);
        }
    }

    /**
     * Create sdp answer
     */
    public void createAnswer() {
        if (peerConnection != null ) {
            isInitiator = false;
            peerConnection.createAnswer(sdpObserver, sdpMediaConstraints);
        }
    }


    // Disconnect from remote resources, dispose of local resources, and exit.
    void disconnect() {
        Log.d(LOG, "Disconnecting.");
        activityRunning = false;
        if (remoteVideoRenderer != null) {
            remoteVideoRenderer.release();
            remoteVideoRenderer = null;
        }
        if (peerConnection != null) {
            peerConnection.close();
            peerConnection.dispose();
            peerConnection = null;
        }
        if (peerConnectionFactory != null) {
            peerConnectionFactory.dispose();
            peerConnectionFactory = null;
        }
        if(rootEglBase != null){
            try {
                rootEglBase.release();
            } catch (RuntimeException e){
                logAndToast(this, e.toString());
            }
        }
        if(serverListFragment != null){
            serverListFragment.onDestroy();
            serverListFragment = null;
        }
        PeerConnectionFactory.stopInternalTracingCapture();
        PeerConnectionFactory.shutdownInternalTracer();
        finish();
    }

    // Activity interfaces
    @Override
    public void onStop() {
        activityRunning = false;
        disconnect();
        super.onStop();
    }

    @Override
    public void onStart() {
        super.onStart();
        startUpActivity();
    }

    @Override
    protected void onDestroy() {
        Thread.setDefaultUncaughtExceptionHandler(null);
        disconnect();
        activityRunning = false;
        super.onDestroy();
    }

    /**
     * Implementation detail: observe ICE & stream changes and react accordingly.
     */
    public class PeerConnectionObserver implements PeerConnection.Observer {
        // Send Ice candidate to the server.
        @Override
        public void onIceCandidate(final IceCandidate iceCandidate) {
            HashMap<String, String> params = new HashMap<>();
            params.put("sdpMLineIndex", String.valueOf(iceCandidate.sdpMLineIndex));
            params.put("sdpMid", iceCandidate.sdpMid);
            params.put("candidate", iceCandidate.sdp);
            sendToPeer(params);
        }

        @Override
        public void onIceCandidatesRemoved(final IceCandidate[] candidates) {
            Log.d(LOG, "onIceCandidatesRemoved: " + Arrays.toString(candidates));

        }

        @Override
        public void onSignalingChange(PeerConnection.SignalingState newState) {
            Log.d(LOG, "SignalingState: " + newState);
        }

        @Override
        public void onIceConnectionChange(final PeerConnection.IceConnectionState newState) {
            if (newState == PeerConnection.IceConnectionState.CONNECTED) {
                Log.d(LOG, "onIceConnectionChange: " + newState);
            } else if (newState == PeerConnection.IceConnectionState.DISCONNECTED) {
                Log.d(LOG, "onIceConnectionChange: " + newState);
            } else if (newState == PeerConnection.IceConnectionState.FAILED) {
                logAndToast(ConnectActivity.this, "ICE connection failed.");
            }
        }

        @Override
        public void onIceGatheringChange(PeerConnection.IceGatheringState newState) {
            Log.d(LOG, "IceGatheringState: " + newState);
        }

        @Override
        public void onIceConnectionReceivingChange(boolean receiving) {
            Log.d(LOG, "IceConnectionReceiving changed to " + receiving);
        }

        @Override
        public void onAddStream(final MediaStream stream) {
            //set the video renderer to visible
            if (peerConnection == null) {
                return;
            }
            if (stream.audioTracks.size() > 1 || stream.videoTracks.size() > 1) {
                logAndToast(ConnectActivity.this, "Weird-looking stream: " + stream);
                return;
            }
            if (stream.videoTracks.size() == 1) {
                //add the video renderer to returned streams video track to display video stream
                remoteVideoTrack = stream.videoTracks.get(0);
                remoteVideoTrack.setEnabled(true);
                remoteVideoTrack.addRenderer(new VideoRenderer(remoteVideoRenderer));
            }
        }

        @Override
        public void onRemoveStream(final MediaStream stream) {
            remoteVideoTrack = null;
        }

        @Override
        public void onDataChannel(final DataChannel dc) {
            dc.registerObserver(new DataChannel.Observer() {
                public void onBufferedAmountChange(long previousAmount) {
                    Log.d(LOG, "Data channel buffered amount changed: " + dc.label() + ": " + dc.state());
                }

                @Override
                public void onStateChange() {
                    Log.d(LOG, "Data channel state changed: " + dc.label() + ": " + dc.state());
                }

                @Override
                public void onMessage(final DataChannel.Buffer buffer) {
                    if (buffer.binary) {
                        Log.d(LOG, "Received binary msg over " + dc);
                        return;
                    }
                    ByteBuffer data = buffer.data;
                    final byte[] bytes = new byte[data.capacity()];
                    data.get(bytes);
                    String strData = new String(bytes);
                    Log.d(LOG, "Got msg: " + strData + " over " + dc);
                }
            });
            inputChannel = dc;
        }

        @Override
        public void onRenegotiationNeeded() {
            // No need to do anything; Client follows a pre-agreed-upon signaling/negotiation protocol.
        }

        @Override
        public void onAddTrack(RtpReceiver rtpReceiver, MediaStream[] mediaStreams) {

        }
    }


    /**
     * Implementation detail: handle offer creation/signaling and answer setting,
     * as well as adding remote ICE candidates once the answer SDP is set.
     */
    public class SDPObserver implements SdpObserver {
        @Override
        public void onCreateSuccess(final SessionDescription origSdp) {
            if (localSdp != null) {
                logAndToast(ConnectActivity.this, "Multiple SDP create.");
                return;
            }
            String sdpDescription = origSdp.description;
            sdpDescription = preferCodec(sdpDescription, "H264", false);
            final SessionDescription sdp = new SessionDescription(origSdp.type, sdpDescription);
            localSdp = sdp;

            if (peerConnection != null) {
                Log.d(LOG, "Setting local SDP from " + sdp.type);
                peerConnection.setLocalDescription(sdpObserver, sdp);
            }
        }

        @Override
        public void onSetSuccess() {
            if (peerConnection == null || localSdp == null) {
                return;
            }
            if (isInitiator) {
                // For offering peer connection we first create offer and set
                // local SDP, then after receiving answer set remote SDP.
                if (peerConnection.getRemoteDescription() == null) {
                    // We've just set our local SDP so time to send it.
                    Log.d(LOG, "Local SDP set succesfully -> sending offer");
                    HashMap<String, String> params = new HashMap<>();
                    params.put("type", "offer");
                    params.put("sdp", localSdp.description);
                    sendToPeer(params);
                } else {
                    // We've just set remote description, so drain remote
                    // and send local ICE candidates.
                    Log.d(LOG, "Remote SDP set succesfully");
                }
            } else {
                // For answering peer connection we set remote SDP and then
                // create answer and set local SDP.
                if (peerConnection.getLocalDescription() != null) {
                    // We've just set our local SDP so time to send it, drain
                    // remote and send local ICE candidates.
                    Log.d(LOG, "Local SDP set succesfully -> sending answer");
                    HashMap<String, String> params = new HashMap<>();
                    params.put("type", "answer");
                    params.put("sdp", localSdp.description);
                    sendToPeer(params);
                } else {
                    // We've just set remote SDP - do nothing for now -
                    // answer will be created soon.
                    Log.d(LOG, "Remote SDP set succesfully");
                }
            }

        }

        @Override
        public void onCreateFailure(final String error) {
            logAndToast(ConnectActivity.this, "createSDP error: " + error);
        }

        @Override
        public void onSetFailure(final String error) {
            logAndToast(ConnectActivity.this, "setSDP error: " + error);
        }
    }
}