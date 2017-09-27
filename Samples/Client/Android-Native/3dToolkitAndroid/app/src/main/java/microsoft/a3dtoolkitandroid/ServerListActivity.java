package microsoft.a3dtoolkitandroid;


import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.util.ArrayMap;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Toast;

import com.android.volley.AuthFailureError;
import com.android.volley.NetworkResponse;
import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.Response;
import com.android.volley.VolleyError;
import com.android.volley.toolbox.JsonObjectRequest;
import com.android.volley.toolbox.StringRequest;
import com.android.volley.toolbox.Volley;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.webrtc.DataChannel;
import org.webrtc.IceCandidate;
import org.webrtc.MediaConstraints;
import org.webrtc.MediaStream;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.RtpReceiver;
import org.webrtc.SdpObserver;
import org.webrtc.SessionDescription;

import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;

import microsoft.a3dtoolkitandroid.util.CustomStringRequest;

import static java.lang.Integer.parseInt;

public class ServerListActivity extends AppCompatActivity {

    public static final String SERVER_NAME = "com.microsoft.a3dtoolkitandroid.SERVER_NAME";
    private static final String LOG = "ServerListLog";
    private static final String ERROR = "ServerListLogError";
    private final int heartbeatIntervalInMs = 5000;
    private Intent intent;
    RequestQueue queue;
    private HashMap<Integer, String> otherPeers = new HashMap<>();
    private List<String> peers;
    private int myID;
    private String server;
    private String port;
    private int messageCounter = 0;
    private PeerConnection pc;
    private PeerConnectionFactory peerConnectionFactory;
    private RtcListener mListener;

    /**
     * Implement this interface to be notified of events.
     */
    public interface RtcListener{
        void onCallReady(String callId);

        void onStatusChanged(String newStatus);

        void onLocalStream(MediaStream localStream);

        void onAddRemoteStream(MediaStream remoteStream, int endPoint);

        void onRemoveRemoteStream(int endPoint);
    }


    //alert dialog
    private AlertDialog.Builder builder;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_server_list);
        final ListView listview = (ListView) findViewById(R.id.ServerListView);

        queue = Volley.newRequestQueue(this);

        intent = getIntent();
        server = intent.getStringExtra(ConnectActivity.SERVER_SERVER);
        port = intent.getStringExtra(ConnectActivity.SERVER_PORT);

        final Intent nextIntent = new Intent(this, StreamActivity.class);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            builder = new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert);
        } else {
            builder = new AlertDialog.Builder(this);
        }

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
        final ArrayAdapter adapter = new ArrayAdapter(this, android.R.layout.simple_list_item_1, peers);
        listview.setAdapter(adapter);

        startHangingGet();
        startHeartBeat();
        updatePeerList();



        listview.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, final View view, int position, long id) {



//                final String item = (String) parent.getItemAtPosition(position);
//
//                String serverName = getServerName(item);
//                nextIntent.putExtra(SERVER_NAME, serverName);
//
//                startActivity(nextIntent);
            }
        });

//        //show the alert
//        AlertDialog alertDialog = builder.create();
//        alertDialog.show();
    }


    private String getServerName(String serverUrl) {
        int renderingServerPrefixLength = "renderingserver_".length();
        int indexOfAt = serverUrl.indexOf('@');
        String serverName = serverUrl.substring(renderingServerPrefixLength, indexOfAt);
        return serverName;
    }

    private void createPeerConnection(int peer_id) {
        try {
            Log.d(LOG, "createPeerConnection: ");
            MediaConstraints defaultPeerConnectionConstraints = new MediaConstraints();
            defaultPeerConnectionConstraints.optional.add(new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", "true"));

            PeerConnection.IceServer iceServer = new PeerConnection.IceServer("turn:turnserveruri:5349", "user", "3Dtoolkit072017", PeerConnection.TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK);
            List<PeerConnection.IceServer> iceServerList = new ArrayList<>();
            iceServerList.add(iceServer);

            PeerConnection.Observer observer = new PeerConnection.Observer() {
                @Override
                public void onSignalingChange(PeerConnection.SignalingState signalingState) {

                }

                @Override
                public void onIceConnectionChange(PeerConnection.IceConnectionState iceConnectionState) {

                }

                @Override
                public void onIceConnectionReceivingChange(boolean b) {

                }

                @Override
                public void onIceGatheringChange(PeerConnection.IceGatheringState iceGatheringState) {

                }

                @Override
                public void onIceCandidate(IceCandidate iceCandidate) {

                }

                @Override
                public void onIceCandidatesRemoved(IceCandidate[] iceCandidates) {

                }

                @Override
                public void onAddStream(MediaStream mediaStream) {

                }

                @Override
                public void onRemoveStream(MediaStream mediaStream) {

                }

                @Override
                public void onDataChannel(DataChannel dataChannel) {

                }

                @Override
                public void onRenegotiationNeeded() {

                }

                @Override
                public void onAddTrack(RtpReceiver rtpReceiver, MediaStream[] mediaStreams) {

                }
            };

            PeerConnectionFactory.initializeAndroidGlobals(this, false, true, true);
            peerConnectionFactory = new PeerConnectionFactory();

            pc = peerConnectionFactory.createPeerConnection(iceServerList, defaultPeerConnectionConstraints, observer);
        } catch (Throwable error) {
            Log.d(ERROR, "Failed to create PeerConnection, exception: " + error.toString());
        }
    }

    private void sendToPeer(int peer_id, SessionDescription data) {
        try {
            Log.d(LOG, peer_id + " Send " + data);
            if (myID == -1) {
                Log.d(ERROR, "sendToPeer: Not Connected");
                return;
            }
            if (peer_id == myID) {
                Log.d(ERROR, "sendToPeer: Can't send a message to oneself :)");
                return;
            }

            // Request a string response from the server
            StringRequest getRequest = new StringRequest(Request.Method.POST, server + "/message?peer_id=" + myID + "&to=" + peer_id, new Response.Listener<String>(){
                @Override
                public void onResponse(String response) {
                    // we don't really care what the response looks like here, so we don't observe it
                }
            },
                    new Response.ErrorListener() {
                        @Override
                        public void onErrorResponse(VolleyError error) {
                            builder.setTitle(ERROR).setMessage("SendToPeer Failure!");
                            Log.d(ERROR, "onErrorResponse: SendToPeer");
                        }
                    }){
                @Override
                public Map<String, String> getHeaders() throws AuthFailureError {
                    Map<String, String>  params = new HashMap<>();
                    params.put("Peer-Type", "Client");
                    params.put("Content-Type", "text/plain");
                    return params;
                }
            };
            // Add the request to the RequestQueue.
            queue.add(getRequest);
        } catch (Throwable e) {
            Log.d(ERROR, "send to peer error: " + e.toString());
        }
    }

    private void handlePeerMessage(final int peer_id, JSONObject data) throws JSONException {
        Log.d(LOG, "handlePeerMessage: START");
        messageCounter++;
        Log.d(LOG, "handlePeerMessage: Message from '" + otherPeers.get(peer_id) + ":" + data);
        final SdpObserver localObsever = new SdpObserver() {
            @Override
            public void onCreateSuccess(SessionDescription sessionDescription) {

            }

            @Override
            public void onSetSuccess() {

            }

            @Override
            public void onCreateFailure(String s) {
                Log.d(ERROR, "Set local description failure: " + s);
            }

            @Override
            public void onSetFailure(String s) {

            }
        };

        SdpObserver remoteObserver = new SdpObserver() {
            @Override
            public void onCreateSuccess(SessionDescription sessionDescription) {
                Log.d(LOG, "Create answer:" + sessionDescription.toString());
                pc.setLocalDescription(localObsever, sessionDescription);
                sendToPeer(peer_id, sessionDescription);
            }

            @Override
            public void onSetSuccess() {

            }

            @Override
            public void onCreateFailure(String s) {
                Log.d(ERROR, "Create answer error: " + s);
            }

            @Override
            public void onSetFailure(String s) {

            }
        };

        if (data.getInt("offer") != -1) {
            createPeerConnection(peer_id);

            MediaConstraints mediaConstraints = new MediaConstraints();
            mediaConstraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveAudio", "false"));
            mediaConstraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveVideo", "true"));

            pc.setRemoteDescription(remoteObserver, new SessionDescription(SessionDescription.Type.OFFER, data.getString("sdp")));
            pc.createAnswer(remoteObserver, mediaConstraints);
        }
        else if (data.getInt("answer") != -1) {
            Log.d(LOG, "Got answer " + data);
            pc.setRemoteDescription(remoteObserver, new SessionDescription(SessionDescription.Type.OFFER, data.getString("sdp")));
        }
        else {
            Log.d(LOG, "Adding ICE candiate " + data);
            IceCandidate candidate = new IceCandidate(data.getString("sdpMid"), data.getInt("sdpMLineIndex"), data.getString("candidate"));
            pc.addIceCandidate(candidate);
        }
        Log.d(LOG, "handlePeerMessage: END");
    }

    private void updatePeerList() {
        Log.d(LOG, "updatePeerList: ");
        try {
            for (Map.Entry<Integer, String> peer : otherPeers.entrySet()) {
                peers.add(peer.getValue());
            }

        } catch (Throwable error) {
            Log.d(ERROR, error.toString());
        }
    }

    private void handleServerNotification(String server) {
        Log.d(LOG, "handleServerNotification: " + server);
        String[] parsed = server.trim().split("\\s*,\\s*");;
        if (parseInt(parsed[2]) != 0) {
            otherPeers.put(parseInt(parsed[1]), parsed[0]);
        }
        updatePeerList();
    }

    /**
     *
     */
    private void startHangingGet() {
        Log.d(LOG, "Start Hanging Get Start");
        CustomStringRequest stringRequest = new CustomStringRequest(Request.Method.GET, server + "/wait?peer_id=" + myID,
                new Response.Listener<CustomStringRequest.ResponseM>() {
                    @Override
                    public void onResponse(CustomStringRequest.ResponseM result) {
                        //From here you will get headers
                        Log.d(LOG, "startHangingGet: onResponse");
                        String peer_id_string = result.headers.get("Pragma");
                        int peer_id = parseInt(peer_id_string);
                        JSONObject response = result.response;

                        Log.d(LOG, "startHangingGet: Message from:" + peer_id + ':' + response);
                        if (peer_id == myID) {
                            Log.d(LOG, "startHangingGet: peer if = myif");
                            try {
                                handleServerNotification(response.getString("Server"));
                            } catch (JSONException e) {
                                e.printStackTrace();
                            }
                        } else {
                            try {
                                Log.d(LOG, "startHangingGet: handlePeerMessage");
                                handlePeerMessage(peer_id, response);
                            } catch (JSONException e) {
                                e.printStackTrace();
                            }
                        }
                    }
                },
                new Response.ErrorListener() {
                    @Override
                    public void onErrorResponse(VolleyError error) {
                        Log.d(ERROR, "startHangingGet: ERROR" + error.toString());
                        if (error.toString().equals("com.android.volley.TimeoutError")){
                            startHangingGet();
                        }
                        builder.setTitle(ERROR).setMessage("Sorry request did not work!");
                    }
                });
        queue.add(stringRequest);

        Log.d(LOG, "Start Hanging Get END");
    }

    /**
     * Sends heartBeat request to server at a regular interval to maintain peerlist
     */
    private void startHeartBeat() {
        Log.d(LOG, "startHeartBeat: ");
        new Timer().scheduleAtFixedRate(new TimerTask(){
            @Override
            public void run(){
                try {
                    addRequest(server + "/heartbeat?peer_id=" + myID, Request.Method.GET, new Response.Listener<String>(){
                        @Override
                        public void onResponse(String response) {
                            // we don't really care what the response looks like here, so we don't observe it
                        }
                    });
                } catch (Throwable error) {
                    Log.d(ERROR, error.toString());
                }
            }
        },0, heartbeatIntervalInMs);
    }

    /**
     * Adds a http request to volley queue.
     * @param String url: http url
     * @param int method: etc Request.Method.GET or Request.Method.POST
     * @param Response.Listener<String> listener: custom listener for responses
     */
    private void addRequest(String url, int method, Response.Listener<String> listener){
        // Request a string response from the server
        StringRequest getRequest = new StringRequest(method, url, listener,
                new Response.ErrorListener() {
                    @Override
                    public void onErrorResponse(VolleyError error) {
                        builder.setTitle(ERROR).setMessage("Sorry request did not work!");
                    }
                }){
            @Override
            public Map<String, String> getHeaders() throws AuthFailureError {
                Map<String, String>  params = new HashMap<>();
                params.put("Peer-Type", "Client");
                return params;
            }
        };
        // Add the request to the RequestQueue.
        queue.add(getRequest);
    }
}