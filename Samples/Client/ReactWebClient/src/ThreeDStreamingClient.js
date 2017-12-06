const validUrl = require('valid-url');
const _ = require('lodash');
const sdputils = require('./sdputils');

//TODO: READD AUTH TOKEN
class ThreeDStreamingClient {
  constructor({serverUrl, peerConnectionConfig, platform}, WebRTC) {
    if (!validUrl.isUri(serverUrl)) {
      throw new Error('Invalid url');
    }
    if (_.isUndefined(WebRTC) || !_.isObject(WebRTC)) {
      throw new Error('Invalid WebRTC object');
    }
    if (_.isUndefined(platform) || !_.isString(platform) ||
      !_.includes(['browser', 'node', 'react-native'], platform)) {
        this.platform = 'browser'; // Default to browser
    } else {
      this.platform = platform;
    }
    this.serverUrl = serverUrl;
    this.pcConfig = peerConnectionConfig;
    this.WebRTC = WebRTC;

    this.myId = -1;
    this.otherPeers = {};
    this.heartBeatIntervalId = null;
    this.peerConnection = null;
    this.inputChannel = null;
    this.repeatLongPoll = false;
    this.onconnecting = null;
    this.onopen = null;
    this.onaddstream = null;
    this.onremovestream = null;
    this.onupdatepeers = null;
  }

  signIn(peerName, {onconnecting = null, onopen = null, onaddstream = null, onremovestream = null, onupdatepeers = null}) {
    // First part of the hand shake
    const fetchOptions = {
      method: 'GET',
      headers: {
        'Peer-Type': 'Client' // Apparently this is useless by @bengreenier
      }
    };

    this.onconnecting = onconnecting;
    this.onopen = onopen;
    this.onaddstream = onaddstream;
    this.onremovestream = onremovestream;
    this.onupdatepeers = onupdatepeers;

    return fetch(`${this.serverUrl}/sign_in?peer_name=${peerName}`, fetchOptions)
      .then((response) => response.text())
      .then((responseText) => {
        //TODO: rewrite this parser.
        var peers = responseText.split('\n');
        // parse my id from the sign in response.
        this.myId = parseInt(peers[0].split(',')[1], 10);
        // Parse the existing list of peers and update map
        for (var i = 1; i < peers.length; ++i) {
          if (peers[i].length > 0) {
            var parsed = peers[i].split(',');
            this.otherPeers[parseInt(parsed[1], 10)] = parsed[0];

          }
        }

        this.onupdatepeers();
      });
  }

  disconnect() {
    this.stopHeartBeat();
    this.repeatLongPoll = false;

    if (this.myId !== -1) {
      fetch(`${this.serverUrl}/sign_out?peer_id=${this.myId}`, {
        method: 'GET',
        headers: {
          'Peer-Type': 'Client'
        }
      });
    }
  }

  // PRIVATE
  _heartbeatFunc() {
    const fetchOptions = {
      method: 'GET',
      headers: {
        // 'Peer-Type': 'Client' // Apparently this is useless by @bengreenier
      }
    };
    // note: we don't really care what the response looks like here, so we don't observe it
    fetch(`${this.serverUrl}/heartbeat?peer_id=${this.myId}`, fetchOptions);
    /*if (accessToken) {
        heartbeatGet.setRequestHeader("Authorization", 'Bearer ' + accessToken);
    } */
  }

  startHeartbeat() {
    // Issue heartbeats indefinitely
    this.heartBeatIntervalId = setInterval(this._heartbeatFunc.bind(this), 5000);
  }

  stopHeartBeat() {
    if (this.heartBeatIntervalId){
      clearInterval(this.heartBeatIntervalId);
      this.heartBeatIntervalId = null;
    }
  }

  getPeerById(id) {
    if (!_.isNumber(id)){
      throw new Error('Invalid Id parameter.');
    }

    if (id in this.otherPeers){
      return this.otherPeers[id];
    }
    else {
      return null;
    }
  }

  getPeerIdByName(name){
    if (!_.isString(name)){
      throw new Error('Invalid paramter, not a string.');
    }
    return _.findKey(this.otherPeers, function(o) { return o === name; });
  }

  stopPollingSignalingServer() {
    this.repeatLongPoll = false;
  }

  pollSignalingServer(repeat) {
    if (!_.isUndefined(repeat) && _.isBoolean(repeat)){
      this.repeatLongPoll = repeat;
    }
    // If repeat long poll is set to false, stop polling.
    if (this.repeatLongPoll === false) {
      return;
    }

    const fetchOptions = {
      method: 'GET',
      headers: {
        'Peer-Type': 'Client' // Apparently this is useless by @bengreenier
      }
    };
    return fetch(`${this.serverUrl}/wait?peer_id=${this.myId}`, fetchOptions)
      .then((response) => {
        if (!response.ok) {
          // console.error(response.statusText);
          // Disconnect on Internal Server Errors?
          if (response.status === 500){
            // TODO: handle this case.
            //this.disconnect();
            return;
          } else {
            this.pollSignalingServer();
          }
        }
        let pragma = response.headers.get('pragma');
        let peer_id = pragma != null && pragma.length ? parseInt(pragma, 10) : null;

        response.text().then((text) => this._handleMessage(peer_id, text))
        .then(() => {
          this.pollSignalingServer();
        });
      }).catch((error) => {
        // Also, this catches any and all errors including errors in the response handler
        // On all other errors (e.g. timeout), restart the long poll.
        console.error(error);
        // restart long poll on timeout.
        if (this.myId !== -1) {
          this.pollSignalingServer();
        }
      });
  }

  // PRIVATE
  _handlePeerListUpdate(peer_id, body) {
    console.log('Handling PEERLIST_UPDATE message');
    var parsed = body.split(',');
    if (parseInt(parsed[2], 10) !== 0) {
      console.log('New peer added.');
      this.otherPeers[parseInt(parsed[1], 10)] = parsed[0];
    }

    this.onupdatepeers();
  }

  // PRIVATE
  _handleOfferMessage(peer_id, body){
    //TODO: I MIGHT NEED TO ASK FOR THE STREAM ADD CALLBACK EARLIER THAN JOIN PEER...
    console.log('Handling OFFER_MESSAGE message');
    let mediaConstraints = {
      'mandatory': {
        'OfferToReceiveAudio': false,
        'OfferToReceiveVideo': true
      }
    };
    let dataJson = JSON.parse(body);
    this._createPeerConnection(peer_id);
    this.peerConnection.setRemoteDescription(new this.WebRTC.RTCSessionDescription(dataJson),
      () => {console.log('Successfully set remote description');},
      (event) => {console.log(`Failed to set remote description on ${event.name}: ${event.message}`);}
    );
    this.peerConnection.createAnswer((sessionDescription) => {
      console.log('Create answer:', sessionDescription);
      this.peerConnection.setLocalDescription(sessionDescription);
      var dataD = JSON.stringify(sessionDescription);
      this.sendToPeer(peer_id, dataD);
    }, function (error) { // error
      console.log('Create answer error:', error);
    }, mediaConstraints); // type error  ); //}, null
  }

  // PRIVATE
  _handleAnswerMessage(peer_id, body){
    console.log('Handling ANSWER_MESSAGE message');
    var dataJson = JSON.parse(body);
    console.log('Got answer ', dataJson);
    this.peerConnection.setRemoteDescription(new this.WebRTC.RTCSessionDescription(dataJson),
      () => {console.log('Successfully set remote description');},
      (event) => {console.log(`Failed to set remote description on ${event.name}: ${event.message}`);
    });
  }

  // PRIVATE
  _handleAddIceCandidate(peer_id, body){
    console.log('BODDY OF ICE CANDIATE: ' + body);
    var dataJson = JSON.parse(body);
    console.log('Adding ICE candiate ', dataJson);
    var candidate = new this.WebRTC.RTCIceCandidate({ sdpMLineIndex: dataJson.sdpMLineIndex, candidate: dataJson.candidate, sdpMid: dataJson.sdpMid });
    if(candidate.sdpMid != null)
    {
      this.peerConnection.addIceCandidate(candidate);
    }
    /*.then(() => {
      // Do nothing
    }).catch(e => {
      trace("Error: Failure during addIceCandidate() " + e);
    });*/
  }

  // PRIVATE
  _handleMessage(peer_id, body){
    if (!_.isString(body) || _.isEmpty(body.trim())){
      console.log('Received an invalid message');
      return;
    }
    let messageType = peer_id === this.myId ? 'PEERLIST_UPDATE' : null;
    messageType = messageType === null && _.isString(body) && body.search('offer') !== -1 ? 'OFFER_MESSAGE' : messageType;
    messageType = messageType === null && _.isString(body) && body.search('answer') !== -1 ? 'ANSWER_MESSAGE' : messageType;
    messageType = messageType === null ? 'ADD_ICE_CANDIDATE' : messageType;

    switch (messageType){
      case 'PEERLIST_UPDATE':
        this._handlePeerListUpdate(peer_id, body);
        break;
      case 'OFFER_MESSAGE':
        this._handleOfferMessage(peer_id, body);
        break;
      case 'ANSWER_MESSAGE':
        this._handleAnswerMessage(peer_id, body);
        break;
      case 'ADD_ICE_CANDIDATE':
        this._handleAddIceCandidate(peer_id, body);
        break;
    }
  }

  _createPeerConnection(peer_id) {
    try {
      // Destroy existing peer connection. This class does not support multiple streams.
      if (this.peerConnection !== null){
        this.peerConnection.close();
        this.peerConnection = null;
      }

      this.peerConnection = new this.WebRTC.RTCPeerConnection(this.pcConfig);
      this.peerConnection.onicecandidate = (event) => {
        if (event.candidate) {
          var candidate = {
            sdpMLineIndex: event.candidate.sdpMLineIndex,
            sdpMid: event.candidate.sdpMid,
            candidate: event.candidate.candidate
          };

          this.sendToPeer(peer_id, JSON.stringify(candidate));
        } else {
          console.log('End of candidates.');
        }
      };
      this.peerConnection.onconnecting = this.onconnecting;
      this.peerConnection.onopen = this.onopen;
      this.peerConnection.onaddstream = this.onaddstream;
      this.peerConnection.onremovestream = this.onremovestream;

      this.peerConnection.ondatachannel = (ev) => {
        this.inputChannel = ev.channel;
        this.inputChannel.onopen = this._handleSendChannelOpen;
        this.inputChannel.onclose = this._handleSendChannelClose;
      };
      console.log('Created RTCPeerConnnection with config: ' + JSON.stringify(this.pcConfig));
      return this.peerConnection;
    }
    catch (e) {
      console.log('Failed to create PeerConnection, exception: ' + e.message);
    }
    // Explictly set to null if we failed...
    this.peerConnection = null;
    return null;
  }

  sendToPeer(peer_id, data) {
    if (this.myId === -1) {
      // Not connected to signaling server...
      return null;
    }
    if (peer_id === this.myId) {
      // Can't send a message to myself
      return null;
    }
    /*if (accessToken) {
      r.setRequestHeader("Authorization", 'Bearer ' + accessToken);
    }*/
    const fetchOptions = {
      method: 'POST',
      headers: {
        'Peer-Type': 'Client', // Apparently this is useless by @bengreenier
        'Content-Type': 'text/plain'
      },
      body: data
    };
    return fetch(`${this.serverUrl}/message?peer_id=${this.myId}&to=${peer_id}`, fetchOptions).catch((reason) => {console.error(reason);});
  }

  _handleSendChannelOpen() {
    console.log('sendChannel opened');
  }
  _handleSendChannelClose(){
    console.log('sendChannel closed');
  }

  joinPeer(peer_id, cb) {
    if (!(peer_id in this.otherPeers)){
      throw new Error('Peer Id is not registered');
    }
    // Create peer connection
    this._createPeerConnection(peer_id, cb);

    // Create data channel
    this.inputChannel = this.peerConnection.createDataChannel('inputDataChannel');
    this.inputChannel.onopen = this._handleSendChannelOpen;
    this.inputChannel.onclose = this._handleSendChannelClose;

    // Create Offer
    let offerOptions = {
      offerToReceiveAudio: 0,
      offerToReceiveVideo: 1
    };

    var receivedOffer = '';
    this.peerConnection.createOffer(offerOptions).then((offer) => {
      offer.sdp = sdputils.maybePreferCodec(offer.sdp, 'video', 'receive', "H264");
      // Set local description
      this.peerConnection.setLocalDescription(offer);
      receivedOffer = offer;
    }).then(() => {
      // Send offer to signaling server
      this.sendToPeer(peer_id, JSON.stringify(receivedOffer));
    });
    // wait for answer & set remote desciption to data supplied by answer

    return this.peerConnection;
  }

  sendInputChannelData(data) {
    if (this.inputChannel && this.inputChannel.readyState === 'open'){
      this.inputChannel.send(JSON.stringify(data));
    }
  }
}

module.exports = ThreeDStreamingClient;
