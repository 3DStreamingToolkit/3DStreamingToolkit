package microsoft.a3dtoolkitandroid;


import android.content.Intent;
import android.databinding.DataBindingUtil;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import com.android.volley.AuthFailureError;
import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.Response;
import com.android.volley.VolleyError;
import com.android.volley.toolbox.JsonObjectRequest;
import com.android.volley.toolbox.StringRequest;
import com.android.volley.toolbox.Volley;
import com.facebook.stetho.Stetho;
import com.facebook.stetho.okhttp3.StethoInterceptor;

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
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import microsoft.a3dtoolkitandroid.databinding.ActivityServerListBinding;
import microsoft.a3dtoolkitandroid.util.CustomStringRequest;
import microsoft.a3dtoolkitandroid.util.EglBase;
import microsoft.a3dtoolkitandroid.util.GenericRequest;
import microsoft.a3dtoolkitandroid.util.OkHttpStack;
import microsoft.a3dtoolkitandroid.util.SurfaceViewRenderer;
import okhttp3.Interceptor;

import static java.lang.Integer.parseInt;
import static microsoft.a3dtoolkitandroid.util.Constants.REMOTE_HEIGHT;
import static microsoft.a3dtoolkitandroid.util.Constants.REMOTE_WIDTH;
import static microsoft.a3dtoolkitandroid.util.Constants.REMOTE_X;
import static microsoft.a3dtoolkitandroid.util.Constants.REMOTE_Y;
import static microsoft.a3dtoolkitandroid.util.RendererCommon.ScalingType.SCALE_ASPECT_FILL;

public class ServerListActivity extends AppCompatActivity {

    public static final String SERVER_NAME = "com.microsoft.a3dtoolkitandroid.SERVER_NAME";
    private static final String LOG = "ServerListLog";
    private static final String ERROR = "ServerListLogError";
    private final int heartbeatIntervalInMs = 5000;
    private Intent intent;
    private RequestQueue requestQueue;
    private HashMap<Integer, String> otherPeers = new HashMap<>();
    private List<String> peers;
    private int myID;
    private int peer_id;
    private String server;
    private String port;
    private int messageCounter = 0;
    private PeerConnection peerConnection;
    private PeerConnectionFactory peerConnectionFactory;
    private DataChannel inputChannel;
    private ArrayAdapter adapter;
    private VideoTrack remoteVideoTrack;
    private SurfaceViewRenderer fullscreenRenderer;
    private final EglBase rootEglBase = EglBase.create();
    private final PeerConnectionObserver peerConnectionObserver = new PeerConnectionObserver();
    private final SDPObserver sdpObserver = new SDPObserver();

    // Queued remote ICE candidates are consumed only after both local and
    // remote descriptions are set. Similarly local ICE candidates are sent to
    // remote peer after both local and remote description are set.
    private LinkedList<IceCandidate> queuedRemoteCandidates;
    private MediaConstraints sdpMediaConstraints;
    private MediaConstraints peerConnectionConstraints;
    private ActivityServerListBinding binding;
    private final List<VideoRenderer.Callbacks> remoteRenderers = new ArrayList<>();
    private SessionDescription localSdp; // either offer or answer SDP


    //alert dialog
    private AlertDialog.Builder builder;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Stetho.initializeWithDefaults(this);
        setContentView(R.layout.activity_server_list);
        final ListView listview = (ListView) findViewById(R.id.ServerListView);

        //create surface renderer and initialize it
        fullscreenRenderer = (SurfaceViewRenderer) findViewById(R.id.remote_video_view);
        fullscreenRenderer.init(rootEglBase.getEglBaseContext(), null);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD | WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED
                | WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        createMediaConstraintsInternal();

        intent = getIntent();
        server = intent.getStringExtra(ConnectActivity.SERVER_SERVER);
        port = intent.getStringExtra(ConnectActivity.SERVER_PORT);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            builder = new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert);
        } else {
            builder = new AlertDialog.Builder(this);
        }

        // Initialize list from sign in response
        String response = intent.getStringExtra(ConnectActivity.SERVER_LIST);
        peers = new ArrayList<>(Arrays.asList(response.split("\n")));
        myID = parseInt(peers.remove(0).split(",")[1]);
        Log.d(LOG, "My ID: " + myID);
        for (int i = 0; i < peers.size(); ++i) {
            if (peers.get(i).length() > 0) {
                Log.d(LOG, "Peer " + i + ": " + peers.get(i));
                String[] parsed = peers.get(i).split(",");
                otherPeers.put(parseInt(parsed[1]), parsed[0]);
            }
        }
        adapter = new ArrayAdapter(this, android.R.layout.simple_list_item_1, peers);
        listview.setAdapter(adapter);

        startHangingGet();
        startHeartBeat();
        updatePeerList();

        // creates a click listener for the peer list
        listview.setOnItemClickListener((parent, view, position, id) -> {
            String item = (String) parent.getItemAtPosition(position);
            Log.d(LOG, "onItemClick: item = " + item);
            String serverName = item.split(",")[0].trim();
            Log.d(LOG, "onItemClick: serverName = " + serverName);
            for (Map.Entry<Integer, String> serverEntry : otherPeers.entrySet()) {
                if (serverEntry.getValue().equals(serverName)) {
                    Log.d(LOG, "onItemClick: PeerID = " + serverEntry.getKey());
                    peer_id = serverEntry.getKey();
                    fullscreenRenderer.setVisibility(View.VISIBLE);
//                    binding = DataBindingUtil.setContentView(this, R.layout.activity_server_list);
//                    remoteRenderers.add(binding.remoteVideoView);
                    joinPeer();
                }
            }
        });

    }

    private void createMediaConstraintsInternal() {
        // Create peer connection constraints.
        peerConnectionConstraints = new MediaConstraints();

        // Enable DTLS for normal calls and disable for loopback calls.
        peerConnectionConstraints.optional.add(new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", "true"));

        //Create SDP constraints.
        sdpMediaConstraints = new MediaConstraints();
        sdpMediaConstraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveAudio", "false"));
        sdpMediaConstraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveVideo", "true"));
    }

    /**
     * Joins server selected from list of peers
     */
    private void joinPeer() {
        Log.d(LOG, "joinPeer: ");

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
    private void createPeerConnection() {
        Log.d(LOG, "createPeerConnection: ");

        //Initialize PeerConnectionFactory globals.
        //Params are context, initAudio, initVideo and videoCodecHwAcceleration
        PeerConnectionFactory.initializeAndroidGlobals(this, false, true, true);
        peerConnectionFactory = new PeerConnectionFactory();

        createPeerConnectionInternal();

        Log.d(LOG, "createPeerConnection: PeerConnection = " + peerConnection.toString());
    }

    private void createPeerConnectionInternal() {
        if (peerConnectionFactory == null) {
            Log.e(LOG, "Peerconnection factory is not created");
            return;
        }
        Log.d(LOG, "Create peer connection.");

        Log.d(LOG, "PCConstraints: " + peerConnectionConstraints.toString());
        queuedRemoteCandidates = new LinkedList<>();

        List<PeerConnection.IceServer> iceServerList = new ArrayList<>();
        iceServerList.add(new PeerConnection.IceServer("stun:stun.l.google.com:19302"));
        iceServerList.add(new PeerConnection.IceServer("stun:stun1.l.google.com:19302"));
        iceServerList.add(new PeerConnection.IceServer("stun:stun2.l.google.com:19302"));
        iceServerList.add(new PeerConnection.IceServer("stun:stun3.l.google.com:19302"));
        iceServerList.add(new PeerConnection.IceServer("stun:stun4.l.google.com:19302"));
        //iceServerList.add(new PeerConnection.IceServer("turn:turnserveruri:5349", "user", "password", PeerConnection.TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK));

        PeerConnection.RTCConfiguration rtcConfig = new PeerConnection.RTCConfiguration(iceServerList);
        // TCP candidates are only useful when connecting to a server that supports
        // ICE-TCP.
        rtcConfig.iceTransportsType = PeerConnection.IceTransportsType.ALL;


        peerConnection = peerConnectionFactory.createPeerConnection(rtcConfig, peerConnectionConstraints, peerConnectionObserver);

        // Set default WebRTC tracing and INFO libjingle logging.
        // NOTE: this _must_ happen while |factory| is alive!
        Logging.enableTracing("logcat:", EnumSet.of(Logging.TraceLevel.TRACE_DEFAULT));
        Logging.enableLogToDebugOutput(Logging.Severity.LS_INFO);

        Log.d(LOG, "Peer connection created.");
    }

    /**
     * Sends sdp offer to server
     *
     * @param params (HashMap<String, String></String,>): json post data
     */
    private void sendToPeer(HashMap<String, String> params) {
        try {
            Log.d(LOG, "sendToPeer(): " + peer_id + " Send ");
            if (myID == -1) {
                Log.d(ERROR, "sendToPeer: Not Connected");
                return;
            }
            if (peer_id == myID) {
                Log.d(ERROR, "sendToPeer: Can't send a message to oneself :)");
                return;
            }

//            Map<String, String> headerParams = new HashMap<>();
//            headerParams.put("Peer-Type", "Client");
//
//            GenericRequest<String> getRequest = new GenericRequest<>(Request.Method.POST ,server + "/message?peer_id=" + myID + "&to=" + peer_id, String.class, params,
//                    response -> {
//                        Log.d(LOG, "sendToPeer(): Response = " + response);
//                    }, error -> {
//                        Log.d(ERROR, "onErrorResponse: SendToPeer = " + error);
//                    }, headerParams, true, true);

            JsonObjectRequest getRequest = new JsonObjectRequest(server + "/message?peer_id=" + myID + "&to=" + peer_id, new JSONObject(params),
                    response -> {
                        Log.d(LOG, "sendToPeer(): Response = " + response.toString());
                        //Process os success response
                    }, error -> {
                Log.d(ERROR, "onErrorResponse: SendToPeer = " + error);
            }) {
                @Override
                public Map<String, String> getHeaders() throws AuthFailureError {
                    Map<String, String> headerParams = new HashMap<>();
                    headerParams.put("Peer-Type", "Client");
                    return headerParams;
                }

                @Override
                public String getBodyContentType() {
                    return "text/plain";
                }
            };


            // Add the request to the RequestQueue.
            getVolleyRequestQueue().add(getRequest);
        } catch (Throwable e) {
            Log.d(ERROR, "send to peer error: " + e.toString());
        }
    }

    /**
     * Handles messages from the server (offer, answer, adding ICE candidate).
     *
     * @param peer_id: id of peer
     * @param data:    JSON response from server
     * @throws JSONException
     */
    private void handlePeerMessage(final int peer_id, JSONObject data) throws JSONException {
        Log.d(LOG, "handlePeerMessage: START");
        messageCounter++;
        Log.d(LOG, "handlePeerMessage: Message from '" + otherPeers.get(peer_id) + ":" + data);

        if (data.getString("type").equals("offer")) {
            Log.d(LOG, "handlePeerMessage: Got offer " + data);
            this.peer_id = peer_id;
            createPeerConnection();
            peerConnection.setRemoteDescription(sdpObserver, new SessionDescription(SessionDescription.Type.OFFER, data.getString("sdp")));
            createAnswer();
        } else if (data.getString("type").equals("answer")) {
            Log.d(LOG, "handlePeerMessage: Got answer " + data);
            peerConnection.setRemoteDescription(sdpObserver, new SessionDescription(SessionDescription.Type.ANSWER, data.getString("sdp")));
        } else {
            Log.d(LOG, "handlePeerMessage: Adding ICE candiate " + data);
            IceCandidate candidate = new IceCandidate(data.getString("sdpMid"), data.getInt("sdpMLineIndex"), data.getString("candidate"));
            peerConnection.addIceCandidate(candidate);
        }
        Log.d(LOG, "handlePeerMessage: END");
    }

    /**
     * Updates the peer list adapter with any new entries
     */
    private void updatePeerList() {
        Log.d(LOG, "updatePeerList: ");
        try {
            for (Map.Entry<Integer, String> peer : otherPeers.entrySet()) {
                peers.add(peer.getValue());
                adapter.notifyDataSetChanged();
            }

        } catch (Throwable error) {
            Log.d(ERROR, error.toString());
        }
    }


    /**
     * Handles adding new servers to the list of servers
     *
     * @param server: e.g. "renderingserver_phcherne@PHCHERNE-PR04,941,1"
     */
    private void handleServerNotification(String server) {
        Log.d(LOG, "handleServerNotification: " + server);
        String[] parsed = server.trim().split("\\s*,\\s*");
        ;
        if (parseInt(parsed[2]) != 0) {
            otherPeers.put(parseInt(parsed[1]), parsed[0]);
        }
        updatePeerList();
    }

    /**
     * Handles adding new servers to list of severs and handles peer messages with the server (offer, answer, adding ICE candidate).
     * Loops on request timeout.
     */
    private void startHangingGet() {
        Log.d(LOG, "Start Hanging Get Start");
        // Created a custom request class to access headers of responses.
        CustomStringRequest stringRequest = new CustomStringRequest(Request.Method.GET, server + "/wait?peer_id=" + myID,
                result -> {
                    //From here you will get headers
                    Log.d(LOG, "startHangingGet: onResponse");
                    String peer_id_string = result.headers.get("Pragma");
                    int peer_id = parseInt(peer_id_string);
                    JSONObject response = result.response;

                    Log.d(LOG, "startHangingGet: Message from:" + peer_id + ':' + response);
                    if (peer_id == myID) {
                        // Update the list of peers
                        Log.d(LOG, "startHangingGet: peer if = myif");
                        try {
                            handleServerNotification(response.getString("Server"));
                        } catch (JSONException e) {
                            e.printStackTrace();
                        }
                    } else {
                        // Handle other messages from server
                        try {
                            Log.d(LOG, "startHangingGet: handlePeerMessage");
                            handlePeerMessage(peer_id, response);
                        } catch (JSONException e) {
                            e.printStackTrace();
                        }
                    }
                },
                error -> {
                    Log.d(ERROR, "startHangingGet: ERROR" + error.toString());
                    if (error.toString().equals("com.android.volley.TimeoutError")) {
                        startHangingGet();
                    } else {
                        builder.setTitle(ERROR).setMessage("Sorry request did not work!");
                    }
                });
        getVolleyRequestQueue().add(stringRequest);

        Log.d(LOG, "Start Hanging Get END");
    }

    /**
     * Sends heartBeat request to server at a regular interval to maintain peerlist
     */
    private void startHeartBeat() {
        Log.d(LOG, "startHeartBeat: ");
        new Timer().scheduleAtFixedRate(new TimerTask() {
            @Override
            public void run() {
                try {
                    addRequest(server + "/heartbeat?peer_id=" + myID, Request.Method.GET, response -> {
                    });
                } catch (Throwable error) {
                    Log.d(ERROR, error.toString());
                }
            }
        }, 0, heartbeatIntervalInMs);
    }

    public void createOffer() {
        Log.d(LOG, "PC Create OFFER");
        peerConnection.createOffer(sdpObserver, sdpMediaConstraints);
    }

    public void createAnswer() {
        Log.d(LOG, "PC create ANSWER");
        peerConnection.createAnswer(sdpObserver, sdpMediaConstraints);
    }

    public void addRemoteIceCandidate(final IceCandidate candidate) {
        if (peerConnection != null) {
            if (queuedRemoteCandidates != null) {
                queuedRemoteCandidates.add(candidate);
            } else {
                peerConnection.addIceCandidate(candidate);
            }
        }
    }

    public void removeRemoteIceCandidates(final IceCandidate[] candidates) {
        if (peerConnection == null) {
            return;
        }
        // Drain the queued remote candidates if there is any so that
        // they are processed in the proper order.
        drainCandidates();
        peerConnection.removeIceCandidates(candidates);
    }

    public void setRemoteDescription(final SessionDescription sdp) {
        if (peerConnection == null) {
            return;
        }
        String sdpDescription = sdp.description;
        sdpDescription = preferCodec(sdpDescription, "H264", false);
        Log.d(LOG, "Set remote SDP.");
        SessionDescription sdpRemote = new SessionDescription(sdp.type, sdpDescription);
        peerConnection.setRemoteDescription(sdpObserver, sdpRemote);
    }

    private void drainCandidates() {
        if (queuedRemoteCandidates != null) {
            Log.d(LOG, "Add " + queuedRemoteCandidates.size() + " remote candidates");
            for (IceCandidate candidate : queuedRemoteCandidates) {
                peerConnection.addIceCandidate(candidate);
            }
            queuedRemoteCandidates = null;
        }
    }

    /**
     * Adds a http request to volley requestQueue.
     *
     * @param url      (String): http url
     * @param method   (int): etc Request.Method.GET or Request.Method.POST
     * @param listener (Response.Listener<String>): custom listener for responses
     */
    private void addRequest(String url, int method, Response.Listener<String> listener) {
        // Request a string response from the server
        StringRequest getRequest = new StringRequest(method, url, listener,
                new Response.ErrorListener() {
                    @Override
                    public void onErrorResponse(VolleyError error) {
                        builder.setTitle(ERROR).setMessage("Sorry request did not work!");
                    }
                }) {
            @Override
            public Map<String, String> getHeaders() throws AuthFailureError {
                Map<String, String> params = new HashMap<>();
                params.put("Peer-Type", "Client");
                return params;
            }
        };
        // Add the request to the RequestQueue.
        getVolleyRequestQueue().add(getRequest);
    }

    public RequestQueue getVolleyRequestQueue() {
        if (requestQueue == null) {
            ArrayList<Interceptor> interceptors = new ArrayList<>();
            interceptors.add(new StethoInterceptor());
            requestQueue = Volley.newRequestQueue
                    (this, new OkHttpStack(interceptors));
        }

        return requestQueue;
    }

    private static String preferCodec(String sdpDescription, String codec, boolean isAudio) {
        String[] lines = sdpDescription.split("\r\n");
        int mLineIndex = -1;
        String codecRtpMap = null;
        // a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]
        String regex = "^a=rtpmap:(\\d+) " + codec + "(/\\d+)+[\r]?$";
        Pattern codecPattern = Pattern.compile(regex);
        String mediaDescription = "m=video ";
        if (isAudio) {
            mediaDescription = "m=audio ";
        }
        for (int i = 0; (i < lines.length) && (mLineIndex == -1 || codecRtpMap == null); i++) {
            if (lines[i].startsWith(mediaDescription)) {
                mLineIndex = i;
                continue;
            }
            Matcher codecMatcher = codecPattern.matcher(lines[i]);
            if (codecMatcher.matches()) {
                codecRtpMap = codecMatcher.group(1);
            }
        }
        if (mLineIndex == -1) {
            Log.w(LOG, "No " + mediaDescription + " line, so can't prefer " + codec);
            return sdpDescription;
        }
        if (codecRtpMap == null) {
            Log.w(LOG, "No rtpmap for " + codec);
            return sdpDescription;
        }
        Log.d(LOG, "Found " + codec + " rtpmap " + codecRtpMap + ", prefer at " + lines[mLineIndex]);
        String[] origMLineParts = lines[mLineIndex].split(" ");
        if (origMLineParts.length > 3) {
            StringBuilder newMLine = new StringBuilder();
            int origPartIndex = 0;
            // Format is: m=<media> <port> <proto> <fmt> ...
            newMLine.append(origMLineParts[origPartIndex++]).append(" ");
            newMLine.append(origMLineParts[origPartIndex++]).append(" ");
            newMLine.append(origMLineParts[origPartIndex++]).append(" ");
            newMLine.append(codecRtpMap);
            for (; origPartIndex < origMLineParts.length; origPartIndex++) {
                if (!origMLineParts[origPartIndex].equals(codecRtpMap)) {
                    newMLine.append(" ").append(origMLineParts[origPartIndex]);
                }
            }
            lines[mLineIndex] = newMLine.toString();
            Log.d(LOG, "Change media description: " + lines[mLineIndex]);
        } else {
            Log.e(LOG, "Wrong SDP media description format: " + lines[mLineIndex]);
        }
        StringBuilder newSdpDescription = new StringBuilder();
        for (String line : lines) {
            newSdpDescription.append(line).append("\r\n");
        }
        return newSdpDescription.toString();
    }

    private void updateVideoView() {
//        binding.remoteVideoLayout.setPosition(REMOTE_X, REMOTE_Y, REMOTE_WIDTH, REMOTE_HEIGHT);
//        binding.remoteVideoView.setScalingType(SCALE_ASPECT_FILL);
//        binding.remoteVideoView.setMirror(false);
//        binding.remoteVideoView.requestLayout();
    }

    // Implementation detail: observe ICE & stream changes and react accordingly.
    private class PeerConnectionObserver implements PeerConnection.Observer {
        @Override
        public void onIceCandidate(final IceCandidate iceCandidate) {
            Log.d(LOG, "onIceCandidate: " + iceCandidate.toString());
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
                updateVideoView();
            } else if (newState == PeerConnection.IceConnectionState.DISCONNECTED) {
                Log.d(LOG, "onIceConnectionChange: " + newState);
            } else if (newState == PeerConnection.IceConnectionState.FAILED) {
                Log.d(ERROR, "ICE connection failed.");
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
            Log.d(LOG, "onAddStream: " + stream.toString());
            if (peerConnection == null) {
                return;
            }
            if (stream.audioTracks.size() > 1 || stream.videoTracks.size() > 1) {
                Log.d(ERROR, "Weird-looking stream: " + stream);
                return;
            }
            if (stream.videoTracks.size() == 1) {
                remoteVideoTrack = stream.videoTracks.get(0);
                remoteVideoTrack.setEnabled(true);
                remoteVideoTrack.addRenderer(new VideoRenderer(fullscreenRenderer));
            }
        }

        @Override
        public void onRemoveStream(final MediaStream stream) {
            remoteVideoTrack = null;
        }

        @Override
        public void onDataChannel(final DataChannel dc) {
            Log.d(LOG, "New Data channel " + dc.label());

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
            // No need to do anything; Client follows a pre-agreed-upon
            // signaling/negotiation protocol.
        }

        @Override
        public void onAddTrack(RtpReceiver rtpReceiver, MediaStream[] mediaStreams) {

        }
    }


    // Implementation detail: handle offer creation/signaling and answer setting,
    // as well as adding remote ICE candidates once the answer SDP is set.
    private class SDPObserver implements SdpObserver {
        @Override
        public void onCreateSuccess(final SessionDescription origSdp) {
            if (localSdp != null) {
                Log.d(ERROR, "Multiple SDP create.");
                return;
            }
            String sdpDescription = origSdp.description;
            sdpDescription = preferCodec(sdpDescription, "H264", false);
            final SessionDescription sdp = new SessionDescription(origSdp.type, sdpDescription);
            localSdp = sdp;

            if (peerConnection != null) {
                Log.d(LOG, "Set local SDP from " + sdp.type);
                peerConnection.setLocalDescription(sdpObserver, sdp);
                if (origSdp.type == SessionDescription.Type.ANSWER)
                {
                    // Sending answer message.
                    HashMap<String, String> params = new HashMap<>();
                    params.put("type", "answer");
                    params.put("sdp", origSdp.description);
                    sendToPeer(params);

                    // Enable fullscreenRenderer for video playback.
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            try {
                                fullscreenRenderer.setVisibility(View.VISIBLE);
                            } catch (Exception e) {
                                e.printStackTrace();
                            }
                        }
                    });
                }
            }
        }

        @Override
        public void onSetSuccess() {
            if (peerConnection == null || localSdp == null) {
                return;
            }

            // For offering peer connection we first create offer and set
            // local SDP, then after receiving answer set remote SDP.
            if (peerConnection.getRemoteDescription() == null) {
                // We've just set our local SDP so time to send it.
                Log.d(LOG, "Local SDP set succesfully");
                HashMap<String, String> params = new HashMap<>();
                params.put("type", "offer");
                params.put("sdp", localSdp.description);
                sendToPeer(params);
            } else {
                // We've just set remote description, so drain remote
                // and send local ICE candidates.
                Log.d(LOG, "Remote SDP set succesfully");
                drainCandidates();
            }
        }

        @Override
        public void onCreateFailure(final String error) {
            Log.d(ERROR, "createSDP error: " + error);
        }

        @Override
        public void onSetFailure(final String error) {
            Log.d(ERROR, "setSDP error: " + error);
        }
    }
}