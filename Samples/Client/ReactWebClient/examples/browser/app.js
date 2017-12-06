$(function(){

  URL = window.webkitURL || window.URL;

  var pcConfigStatic = {
    'iceServers': [{
        'urls': 'turn server goes here',
        'username': 'username goes here',
        'credential': 'password goes here',
        'credentialType': 'password'
    }],
    'iceTransportPolicy': 'relay'
  };

  var pcConfigDynamic = {
    'iceServers': [{
        'urls': 'turn:turnserveruri:5349',
        'credentialType': 'password'
    }],
    'iceTransportPolicy': 'relay'
  };

  var defaultSignalingServerUrl = 'http://localhost:3001'

  var aadConfig = {
    clientID: 'clientid',
    authority: 'https://login.microsoftonline.com/tfp/tenant.onmicrosoft.com/b2c_1_signup',
    b2cScopes: ['openid']
  };

  var identityManagementConfig = {
    turnCredsUrl: 'https://identitymanagementapi'
  };

  var streamingClient;

  var navTransform = matCreate();
  var navHeading = 0.0;
  var navPitch = 0.0;
  var navLocation = [ 0.0, 0.0, 0.0 ];

  var isMouseDown = false;
  var mouseDownX = 0;
  var mouseDownY = 0;

  var downPitch = 0.0;
  var downHeading = 0.0;
  var downLocation = [ 0.0, 0.0, 0.0 ];

  var pcOptions = {
    optional: [
        { DtlsSrtpKeyAgreement: true }
    ]
  }
  var mediaConstraints = {
    'mandatory': {
        'OfferToReceiveAudio': false,
        'OfferToReceiveVideo': true
    }
  };
  var remoteStream;
  var accessToken;

  var pcConfig = pcConfigStatic;

  RTCPeerConnection = window.mozRTCPeerConnection || window.webkitRTCPeerConnection || RTCPeerConnection;
  RTCSessionDescription = window.mozRTCSessionDescription || window.RTCSessionDescription || RTCSessionDescription;
  RTCIceCandidate = window.mozRTCIceCandidate || window.RTCIceCandidate || RTCIceCandidate;
  getUserMedia = navigator.mozGetUserMedia || navigator.webkitGetUserMedia;
  URL = window.webkitURL || window.URL;


  document.getElementById('server').value = defaultSignalingServerUrl;

  var clientApplication = new Msal.UserAgentApplication(aadConfig.clientID, aadConfig.authority, function (errorDesc, token, error, tokenType) {
  });

  function login() {
    clientApplication.loginPopup(aadConfig.b2cScopes).then(function (idToken) {
        clientApplication.acquireTokenSilent(aadConfig.b2cScopes).then(function (token) {
            accessToken = token;
            var userName = clientApplication.getUser().name;
            console.log(clientApplication.getUser());
            console.log("User '" + userName + "' logged-in");
            document.getElementById('authlabel').innerText = 'Hello ' + userName;

            if (document.getElementById('turnTempPasswordEnabled').checked) {
                var loginRequest = new XMLHttpRequest();
                loginRequest.onreadystatechange = function (event) {
                    if (loginRequest.readyState == 4 && loginRequest.status == 200) {
                        var data = JSON.parse(event.target.response);
                        console.log('Identity management returned', data);

                        pcConfig = pcConfigDynamic;
                        pcConfig.iceServers[0].username = data.username;
                        pcConfig.iceServers[0].credential = data.password;
                    }
                };
                loginRequest.open('GET', identityManagementConfig.turnCredsUrl, true);
                loginRequest.setRequestHeader('Authorization', 'Bearer ' + accessToken);
                loginRequest.send();
            }
        }, function (error) {
            clientApplication.acquireTokenPopup(aadConfig.b2cScopes).then(function (accessToken) {
                //updateUI();
            }, function (error) {
                console.log('Error acquiring the popup:\n' + error);
            });
        })

    }, function (error) {
        console.log('Error during login:\n' + error);
    });
  }

  function onRemoteStreamAdded(event) {
    console.log('Remote stream added:', URL.createObjectURL(event.stream));
    var remoteVideoElement = document.getElementById('remote-video');
    remoteVideoElement.src = URL.createObjectURL(event.stream);
    remoteVideoElement.play();
  }

  function joinPeer() {
    try {
        // get the currently selected peer from the peerList
        var list = document.getElementById('peerList');
        var peerName = list.value;

        var pc = streamingClient.joinPeer(streamingClient.getPeerIdByName(peerName));

    } catch (e) {
        trace('error ' + e.description);
    }
  }

  function navOnMouseDown(event)
  {
    isMouseDown = true;

    mouseDownX = event.clientX;
    mouseDownY = event.clientY;

    downPitch = navPitch;
    downHeading = navHeading;
    downLocation[0] = navLocation[0];
    downLocation[1] = navLocation[1];
    downLocation[2] = navLocation[2];

    event.preventDefault();
  }

  function navOnMouseUp()
  {
    isMouseDown = false;

    event.preventDefault();
  }

  function navOnMouseMove(event)
  {
    if (isMouseDown)
    {
        var dx = event.clientX - mouseDownX;
        var dy = event.clientY - mouseDownY;

        if (event.ctrlKey == 0)
        {
            // calc delta pitch & heading change
            var dpitch = 0.005 * dy;
            var dheading = 0.005 * dx;

            navHeading = downHeading - dheading;
            navPitch = downPitch + dpitch;

            // calc delta rotation matrix
            var locTransform =  matMultiply(matRotateY(navHeading), matRotateZ(navPitch));
            navTransform = matMultiply(matTranslate(navLocation), locTransform);

            // send transform message to server
            sendTransform();
        }
        else
        {
             // calculate distance to travel
            var dist = -dy;

            // update the navigation origin
            navLocation[0] = downLocation[0] + dist * navTransform[0];
            navLocation[1] = downLocation[1] + dist * navTransform[1];
            navLocation[2] = downLocation[2] + dist * navTransform[2];

            navTransform[12] = navLocation[0];
            navTransform[13] = navLocation[1];
            navTransform[14] = navLocation[2];

            // send transform message to server
            sendTransform();
        }

        event.preventDefault();
    }
  }

  function sendTransform()
  {
    if (streamingClient)
    {
        var eye = [ navTransform[12], navTransform[13], navTransform[14]];
        var lookat = [ navTransform[12] + navTransform[0],
                                navTransform[13] + navTransform[1],
                                navTransform[14] + navTransform[2]];
        var up = [ navTransform[4], navTransform[5], navTransform[6]];

        var data = eye[0] + ', ' + eye[1] + ', ' + eye[2] + ', ' +
                    lookat[0] + ', ' + lookat[1] + ', ' + lookat[2] + ', ' +
                    up[0] + ', ' + up[1] + ', ' + up[2];

        var msg =
        {
            'type' : 'camera-transform-lookat',
            'body' : data
        }
        streamingClient.sendInputChannelData(msg);
    }
  }

  function trace(txt) {
    var elem = document.getElementById('debug');
    elem.innerHTML += txt + '<br>';
  }

  function connect() {
    var localName = document.getElementById('local').value.toLowerCase();
    var server = document.getElementById('server').value.toLowerCase();
    if (localName.length == 0) {
        alert('I need a name please.');
        document.getElementById('local').focus();
    } else {
        document.getElementById('connect').style.display = 'none';
        document.getElementById('cred').style.display = 'none';
        document.getElementById('disconnect').style.display = 'block';
        document.getElementById('renderers').style.display = 'block';

        streamingClient = new ThreeDToolkit.ThreeDStreamingClient({
          'serverUrl': server,
          'peerConnectionConfig': pcConfigStatic
        }, {
          RTCPeerConnection: window.mozRTCPeerConnection || window.webkitRTCPeerConnection,
          RTCSessionDescription: window.mozRTCSessionDescription || window.RTCSessionDescription,
          RTCIceCandidate: window.mozRTCIceCandidate || window.RTCIceCandidate,
          getUserMedia: navigator.mozGetUserMedia || navigator.webkitGetUserMedia
        });

        streamingClient.signIn(localName, 
        {
          onaddstream: onRemoteStreamAdded.bind(this),
          onremovestream: onRemoteStreamRemoved,
          onopen: onSessionOpened,
          onconnecting: onSessionConnecting,
          onupdatepeers: updatePeerList.bind(this)
        })
          .then(streamingClient.startHeartbeat.bind(streamingClient))
          .then(streamingClient.pollSignalingServer.bind(streamingClient, true));
    }
  }

  function disconnect()
  {
    streamingClient.disconnect();

    document.getElementById('connect').style.display = 'block';
    document.getElementById('cred').style.display = 'block';
    document.getElementById('disconnect').style.display = 'none';
    document.getElementById('renderers').style.display = 'none';
  }

  function updatePeerList() {
    try {
        var list = document.getElementById('peerList');

        list.innerHTML = '';

        for (var peerId in streamingClient.otherPeers) {
            var peer = streamingClient.otherPeers[peerId];

            var option = document.createElement('option');

            option.text = peer;
            list.add(option);
        }

    } catch (e) {
        trace('error ' + e.description);
    }
  }

  window.onbeforeunload = disconnect;

  /*
  function send() {
    var text = document.getElementById("message").value;
    var peer_id = parseInt(document.getElementById("peer_id").value);
    if (!text.length || peer_id == 0) {
        alert("No text supplied or invalid peer id");
    } else {
        sendToPeer(peer_id, text);
    }
  }

  function toggleMe(obj) {
    var id = obj.id.replace("toggle", "msg");
    var t = document.getElementById(id);
    if (obj.innerText == "+") {
        obj.innerText = "-";
        t.style.display = "block";
    } else {
        obj.innerText = "+";
        t.style.display = "none";
    }
  }
  */
  function onSessionConnecting(message) {
    console.log('Session connecting.');
  }

  function onSessionOpened(message) {
    console.log('Session opened.');
  }

  function onRemoteStreamRemoved(event) {
    console.log('Remote stream removed.');
  }

  function onRemoteSdpError(event) {
    console.error('onRemoteSdpError', event.name, event.message);
  }

  function onRemoteSdpSucces() {
    console.log('onRemoteSdpSucces');
  }

  $('#connect').click(connect);
  $('#disconnect').click(disconnect);
  $('#join').click(joinPeer);

  $('#remote-video').mousedown(navOnMouseDown);
  $('#remote-video').mouseup(navOnMouseUp);
  $('#remote-video').click(navOnMouseUp);
  $('#remote-video').mousemove(navOnMouseMove);
  $('#remote-video').mouseout(navOnMouseUp);

});
