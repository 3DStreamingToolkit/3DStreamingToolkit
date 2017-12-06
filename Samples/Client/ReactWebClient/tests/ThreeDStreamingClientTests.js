const test = require('tape');
const sinon = require('sinon');
const fetchMock = require('fetch-mock');

const ThreeDStreamingClient = require('../src/ThreeDStreamingClient');

function setupMockFetch(){
  const peer_id = 1;
  const peer_name = 'TEST';
  const server_id = 905;

  fetchMock.get(`glob:https://*/sign_in?peer_name=${peer_name}`,{
    body: `${peer_name},${peer_id},1\nrenderingserver_phcherne@PHCHERNE-PR04,${server_id},1\n\n`,
    headers: {
      'Pragma': peer_id
    }
  });
  fetchMock.get(`glob:https://*/heartbeat?peer_id=${peer_id}`, {
    body: 'OK',
    status: 200,
    headers: {
      'Content-Type': 'text/plain; charset=utf-8'
    }
  });

  var step = 0;
  fetchMock.get(`glob:https://*/wait?peer_id=${peer_id}`, (url, opts) => {
    if (step === 0){
      step += 1;
      return {
        status: 200,
        headers: {
          'Content-Type': 'text/html; charset=utf-8', // For some reason the server does not say it's json...
          'Pragma': server_id
        },
        body: '{"sdp" : "v=0\\r\\no=- 7607571057346371734 2 IN IP4 127.0.0.1\\r\\ns=-\\r\\nt=0 0\\r\\na=group:BUNDLE video data\\r\\na=msid-semantic: WMS stream_label\\r\\nm=video 9 UDP/TLS/RTP/SAVPF 100 96 98 102 127 97 99 101 125\\r\\nc=IN IP4 0.0.0.0\\r\\na=rtcp:9 IN IP4 0.0.0.0\\r\\na=ice-ufrag:U+gx\\r\\na=ice-pwd:ff8MT2oVnTSZup6YYxEDa/2A\\r\\na=fingerprint:sha-256 DC:AA:44:35:60:7A:77:57:5A:1C:A4:3F:E0:BC:E7:56:FA:AD:6C:1A:C2:3C:14:49:D5:73:BC:2C:39:92:B7:3B\\r\\na=setup:active\\r\\na=mid:video\\r\\na=extmap:2 urn:ietf:params:rtp-hdrext:toffset\\r\\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\\r\\na=extmap:4 urn:3gpp:video-orientation\\r\\na=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\\r\\na=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\\r\\na=sendonly\\r\\na=rtcp-mux\\r\\na=rtcp-rsize\\r\\na=rtpmap:100 H264/90000\\r\\na=rtcp-fb:100 ccm fir\\r\\na=rtcp-fb:100 nack\\r\\na=rtcp-fb:100 nack pli\\r\\na=rtcp-fb:100 goog-remb\\r\\na=rtcp-fb:100 transport-cc\\r\\na=fmtp:100 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\\r\\na=rtpmap:96 VP8/90000\\r\\na=rtcp-fb:96 ccm fir\\r\\na=rtcp-fb:96 nack\\r\\na=rtcp-fb:96 nack pli\\r\\na=rtcp-fb:96 goog-remb\\r\\na=rtcp-fb:96 transport-cc\\r\\na=rtpmap:98 VP9/90000\\r\\na=rtcp-fb:98 ccm fir\\r\\na=rtcp-fb:98 nack\\r\\na=rtcp-fb:98 nack pli\\r\\na=rtcp-fb:98 goog-remb\\r\\na=rtcp-fb:98 transport-cc\\r\\na=rtpmap:102 red/90000\\r\\na=rtpmap:127 ulpfec/90000\\r\\na=rtpmap:97 rtx/90000\\r\\na=fmtp:97 apt=96\\r\\na=rtpmap:99 rtx/90000\\r\\na=fmtp:99 apt=98\\r\\na=rtpmap:101 rtx/90000\\r\\na=fmtp:101 apt=100\\r\\na=rtpmap:125 rtx/90000\\r\\na=fmtp:125 apt=102\\r\\na=ssrc-group:FID 2954506509 4189327995\\r\\na=ssrc:2954506509 cname:ul/zQJpI26h4wV3i\\r\\na=ssrc:2954506509 msid:stream_label video_label\\r\\na=ssrc:2954506509 mslabel:stream_label\\r\\na=ssrc:2954506509 label:video_label\\r\\na=ssrc:4189327995 cname:ul/zQJpI26h4wV3i\\r\\na=ssrc:4189327995 msid:stream_label video_label\\r\\na=ssrc:4189327995 mslabel:stream_label\\r\\na=ssrc:4189327995 label:video_label\\r\\nm=application 9 DTLS/SCTP 5000\\r\\nc=IN IP4 0.0.0.0\\r\\nb=AS:30\\r\\na=ice-ufrag:U+gx\\r\\na=ice-pwd:ff8MT2oVnTSZup6YYxEDa/2A\\r\\na=fingerprint:sha-256 DC:AA:44:35:60:7A:77:57:5A:1C:A4:3F:E0:BC:E7:56:FA:AD:6C:1A:C2:3C:14:49:D5:73:BC:2C:39:92:B7:3B\\r\\na=setup:active\\r\\na=mid:data\\r\\na=sctpmap:5000 webrtc-datachannel 1024\\r\\n","type" : "answer"}'
      };
    } else if (step === 1) {
      step = 0;
      return {
        status: 200,
        headers: {
          'Content-Type': 'text/html; charset=utf-8',
          'Pragma': server_id
        },
        body: '{"candidate" : "candidate:337435484 1 udp 41754367 13.89.232.16 50838 typ relay raddr 0.0.0.0 rport 0 generation 0 ufrag U+gx network-id 3 network-cost 50","sdpMLineIndex" : 0,"sdpMid" : "video"}'
      };
    }
  });

  // Server always sends back OK to any message sent to /message
  fetchMock.post(`glob:https://*/message?peer_id=${peer_id}&to=${server_id}`, {
    status: 200,
    body: 'OK',
    headers: {
      'Content-Type': 'text/plain; charset=utf-8'
    }
  });
}

function printParameter(param) {
  console.log(param);
}
const MockWebRTC = {
    RTCSessionDescription: printParameter,
    RTCIceCandidate: printParameter,
    RTCPeerConnection: (param) => {
      console.log(param);
      return {
        setRemoteDescription: printParameter,
        createAnswer: printParameter,
        setLocalDescription: printParameter,
        addIceCandidate: printParameter,
        createOffer: printParameter
      };
    }
};


var serverConfig = {
  serverUrl: 'https://turnserver.azurewebsites.net',
  peerConnectionConfig: {
    'iceServers': [{
    'urls': 'turn:turn.centralus.cloudapp.azure.com:5349',
    'username': 'user',
    'credential': 'password',
    'credentialType': 'password'
    }],
    'iceTransportPolicy': 'relay'
  }
};

test('Test Constructor', function (t) {
  const client = new ThreeDStreamingClient(serverConfig, MockWebRTC);

  t.equal(client.myId, -1);
  t.equal(client.serverUrl, 'https://turnserver.azurewebsites.net');
  t.deepEqual(client.pcConfig, {
    'iceServers': [{
      'urls': 'turn:turn.centralus.cloudapp.azure.com:5349',
      'username': 'user',
      'credential': 'password',
      'credentialType': 'password'
      }],
      'iceTransportPolicy': 'relay'
    }
  );
  t.deepEqual(client.otherPeers, {});
  t.notOk(client.peerConnection);
  t.notOk(client.inputChannel);
  t.end();
});

test('Test SignIn', function(t) {
  setupMockFetch();

  const client = new ThreeDStreamingClient(serverConfig, MockWebRTC);
  client.signIn('TEST').then(() => {
    t.equal(client.myId, 1);
    t.deepEqual(client.otherPeers, {905: 'renderingserver_phcherne@PHCHERNE-PR04'});

    t.end();

    fetchMock.restore();
  });
});

test('Test Hanging Get', function(t){
  setupMockFetch();

  const client = new ThreeDStreamingClient(serverConfig, MockWebRTC);
  client.signIn('TEST').then(() => {
    t.equal(client.myId, 1);
    client.pollSignalingServer(false);
    t.end();
  }).catch((e) => {
    console.log(e);
  });
});
