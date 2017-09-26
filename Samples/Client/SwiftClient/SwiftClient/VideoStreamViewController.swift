//
//  VideoStreamViewController.swift
//  SwiftClient
//
//  Created by Michael Perel on 9/24/17.
//  Copyright © 2017 Michael Perel. All rights reserved.
//

import UIKit
import WebRTC

class VideoStreamViewController: UIViewController {
    
    @IBOutlet weak var pickerView: UIPickerView!
    var otherPeers = [Int: String]()
    var myId: Int!
    var heartBeatTimer: Timer!
    var heartBeatTimerIsRunning = false
    let heartbeatIntervalInSecs = 5
    let server = Config.server
    let localName = "ios_client"
    var messageCounter = 0
    var peerConnection: RTCPeerConnection!
    var peerConnectionFactory = RTCPeerConnectionFactory()
    var newIceCandidatePeerId = -1
    var peerListDisplay = ["Select"]
    var hangingGet: URLSessionTask!
    var request: URLSessionDataTask!
    @IBOutlet weak var renderView: RTCEAGLVideoView!
    var videoTrack: RTCVideoTrack!
    var videoStream: RTCMediaStream!
    var inputChannel: RTCDataChannel!
    lazy var navTransform: [CGFloat] = {
        [unowned self] in
        self.matCreate()
        }()
    var navHeading: CGFloat = 0.0
    var navPitch: CGFloat = 0.0
    var navLocation: [CGFloat] = [ 0.0, 0.0, 0.0 ]
    var isFingerDown = false
    var fingerDownX: CGFloat = 0
    var fingerDownY: CGFloat = 0
    var downPitch: CGFloat = 0.0
    var downHeading: CGFloat = 0.0
    var downLocation: [CGFloat] = [ 0.0, 0.0, 0.0 ]
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        RTCInitializeSSL()
        renderView.delegate = self
        
        pickerView.dataSource = self
        pickerView.delegate = self
        
        initGestureRecognizer()
        connect()
    }
    
    func joinPeer(peerId: Int) {
        createPeerConnection(peerId: peerId)
        inputChannel = self.peerConnection.dataChannel(forLabel: "inputDataChannel", configuration: RTCDataChannelConfiguration())
        inputChannel.delegate = self
        
        let offerOptions = RTCMediaConstraints(mandatoryConstraints: ["OfferToReceiveAudio": "false","OfferToReceiveVideo": "true"], optionalConstraints: nil)
        
        peerConnection.offer(for: offerOptions) { (sdp, error) in
            let sdpH264 = sdp!.sdp.replacingOccurrences(of: "96 98 100 102", with: "100 96 98 102")
            let receivedOffer = RTCSessionDescription(type: .offer, sdp: sdpH264)
            self.peerConnection.setLocalDescription(receivedOffer, completionHandler: { (error) in
                
                let jsonData = [
                    "type": "offer",
                    "sdp": receivedOffer.sdp
                ]
                
                self.sendToPeer(peerId: peerId, data: self.convertDictionaryToData(dict: jsonData)!)
            })
        }
    }
    
    func createPeerConnection(peerId: Int) {
        newIceCandidatePeerId = peerId
        
        let defaultPeerConnectionConstraints = RTCMediaConstraints(mandatoryConstraints: nil,
                                                                   optionalConstraints: ["DtlsSrtpKeyAgreement": "true"])
        let rtcConfig = RTCConfiguration()
        let iceServer = RTCIceServer(urlStrings: ["turn:turnserver3dstreaming.centralus.cloudapp.azure.com:5349"],
                                     username: "user",
                                     credential: "3Dtoolkit072017",
                                     tlsCertPolicy: .insecureNoCheck)
        rtcConfig.iceServers = [iceServer]
        rtcConfig.iceTransportPolicy = .relay
        
        peerConnection = peerConnectionFactory.peerConnection(with: rtcConfig, constraints: defaultPeerConnectionConstraints, delegate: self)
    }
    
    func handlePan(_ recognizer: UIPanGestureRecognizer) {
        
        print(recognizer.numberOfTouches)
        
        switch recognizer.state {
        case .began:
            navOnFingerDown(recognizer: recognizer)
        case .changed:
            navOnFingerMove(recognizer: recognizer)
        case .ended:
            navOnFingerUp()
        default:
            break
        }
    }
    
    func initGestureRecognizer() {
        let panGestureRecognizer = UIPanGestureRecognizer(target: self, action: #selector(VideoStreamViewController.handlePan(_:)))
        panGestureRecognizer.minimumNumberOfTouches = 1
        panGestureRecognizer.maximumNumberOfTouches = 2
        
        self.renderView.addGestureRecognizer(panGestureRecognizer)
    }
    
    func navOnFingerDown(recognizer: UIPanGestureRecognizer) {
        
        let tappedPoint = recognizer.location(in: self.view)
        
        isFingerDown = true
        
        fingerDownX = tappedPoint.x
        fingerDownY = tappedPoint.y
        
        downPitch = navPitch
        downHeading = navHeading
        downLocation[0] = navLocation[0]
        downLocation[1] = navLocation[1]
        downLocation[2] = navLocation[2]
    }
    
    func navOnFingerUp() {
        isFingerDown = false
    }
    
    func navOnFingerMove(recognizer: UIPanGestureRecognizer) {
        if (isFingerDown) {
            if recognizer.numberOfTouches == 1 {
                // location of first finger
                let tappedPoint = recognizer.location(ofTouch: 0, in: self.view)
                
                let dx = tappedPoint.x - fingerDownX
                let dy = tappedPoint.y - fingerDownY
                
                let dpitch = 0.005 * dy
                let dheading = 0.005 * dx
                
                navHeading = downHeading - dheading
                navPitch = downPitch + dpitch
                
                let locTransform =  matMultiply(a: matRotateY(rad: navHeading), b: matRotateZ(rad: navPitch))
                navTransform = matMultiply(a: matTranslate(v: navLocation), b: locTransform)
                
                sendTransform()
            }
            
            if recognizer.numberOfTouches == 2 {
                // location of second finger
                let tappedPoint = recognizer.location(ofTouch: 1, in: self.view)
                
                let dy = tappedPoint.y - fingerDownY
                
                let dist = -dy
                
                navLocation[0] = downLocation[0] + dist * navTransform[0]
                navLocation[1] = downLocation[1] + dist * navTransform[1]
                navLocation[2] = downLocation[2] + dist * navTransform[2]
                
                navTransform[12] = navLocation[0]
                navTransform[13] = navLocation[1]
                navTransform[14] = navLocation[2]
                
                sendTransform()
            }
        }
    }
    
    
    func sendTransform() {
        if inputChannel != nil {
            var eye = [ navTransform[12], navTransform[13], navTransform[14]]
            var lookat = [ navTransform[12] + navTransform[0],
                           navTransform[13] + navTransform[1],
                           navTransform[14] + navTransform[2]]
            var up = [ navTransform[4], navTransform[5], navTransform[6]]
            
            let data = "\(eye[0]), \(eye[1]), \(eye[2]), \(lookat[0]), \(lookat[1]), \(lookat[2]), \(up[0]), \(up[1]), \(up[2])"
            
            let msg = [
                "type" : "camera-transform-lookat",
                "body" : data
            ]
            
            let buffer = RTCDataBuffer(data: convertDictionaryToData(dict: msg)!, isBinary: false)
            inputChannel.sendData(buffer)
        }
    }
    
    func handlePeerMessage(peerId: Int, data: Data) {
        messageCounter += 1
        let stringData = String(data: data, encoding: .utf8)!
        print("Message from \(otherPeers[peerId]!): \(stringData)")
        
        var dataJson = [String: Any]()
        do {
            dataJson = try JSONSerialization.jsonObject(with: data) as! [String: Any]
        } catch {
            print("Error deserializing JSON: \(error)")
        }
        
        guard let stringType = dataJson["type"] as? String else {
            newIceCandidatePeerId = peerId
            let candidate = RTCIceCandidate(sdp: dataJson["candidate"] as! String,
                                            sdpMLineIndex: Int32(dataJson["sdpMLineIndex"] as! Int),
                                            sdpMid: (dataJson["sdpMid"] as! String))
            peerConnection.add(candidate)
            return
        }
        
        var type: RTCSdpType
        
        if stringType == "offer" {
            type = .offer
        } else if stringType == "answer" {
            type = .answer
        } else {
            type = .prAnswer
        }
        
        let sdp = RTCSessionDescription(type: type, sdp: dataJson["sdp"] as! String)
        
        switch type {
        case .offer:
            createPeerConnection(peerId: peerId)
            peerConnection.setRemoteDescription(sdp, completionHandler: { (error) in
                let mediaConstraints = RTCMediaConstraints(mandatoryConstraints: ["OfferToReceiveAudio": "false","OfferToReceiveVideo": "true"], optionalConstraints: nil)
                self.peerConnection.answer(for: mediaConstraints, completionHandler: { (sdp, error) in
                    print("Create answer: \(String(describing: sdp))")
                    self.peerConnection.setLocalDescription(sdp!, completionHandler: { (error) in
                        var jsonData = ["sdp" : sdp!.sdp]
                        var answerType: String
                        switch sdp!.type {
                        case .offer:
                            answerType = "offer"
                        case .answer, .prAnswer:
                            answerType = "answer"
                        }
                        
                        jsonData["type"] = answerType
                        
                        self.sendToPeer(peerId: peerId, data: self.convertDictionaryToData(dict: jsonData)!)
                    })
                })
            })
        case .answer, .prAnswer:
            print("Got answer \(dataJson)")
            self.peerConnection.setRemoteDescription(sdp, completionHandler: { (error) in
                // If you get rid of this completion handler, the app crashes. This may be a problem with WebRTC, but it could also be a problem on our end.
                if let error = error {
                    print(error)
                }
            })
        }
    }
    
    func updatePeerList() {
        for (peerId, name) in self.otherPeers {
            self.peerListDisplay.append("\(peerId), Name: \(name)")
        }
        DispatchQueue.main.async {
            self.pickerView.reloadAllComponents()
        }
        print(self.otherPeers)
    }
    
    func handleServerNotification(data: Data) {
        let stringData = String(data: data, encoding: .utf8)!
        print("Server notification: \(stringData)")
        
        var parsed = stringData.components(separatedBy: ",")
        if parsed.count > 2 {
            let trimmedParsed = parsed[2].trimmingCharacters(in: .whitespacesAndNewlines)
            if Int(trimmedParsed) != 0 {
                otherPeers[Int(parsed[1])!] = parsed[0]
            }
            updatePeerList()
        }
    }
    
    func parseIntHeader(r: HTTPURLResponse, name: String) -> Int {
        let val = r.allHeaderFields[name] as? String
        if let val = val, val.characters.count > 0 {
            return Int(val)!
        } else {
            return -1
        }
    }
    
    func hangingGetCallback(data: Data?, response: URLResponse?, error: Error?) {
        if let error = error as NSError?, error.code != NSURLErrorTimedOut {
            print("server error: \(error)")
            disconnect()
        }
        
        if let response = response as? HTTPURLResponse, let data = data {
            
            if response.statusCode != 200 {
                print("server error: \(response.statusCode)")
                disconnect()
            } else {
                let peerId = parseIntHeader(r: response, name: "Pragma")
                print("Message from: \(peerId): \(data)")
                
                if (peerId == myId) {
                    handleServerNotification(data: data)
                } else {
                    handlePeerMessage(peerId: peerId, data: data)
                }
            }
        }
        
        if hangingGet != nil {
            hangingGet.cancel()
            hangingGet = nil
        }
        
        if myId != -1 {
            startHangingGet()
        }
    }
    
    func startHangingGet() {
        let urlString = URL(string: "\(server)/wait?peer_id=\(myId!)")!
        var urlRequest = URLRequest(url: urlString)
        urlRequest.setValue("Client", forHTTPHeaderField: "Peer-Type")
        hangingGet = URLSession.shared.dataTask(with: urlRequest, completionHandler: hangingGetCallback)
        hangingGet.resume()
    }
    
    func startHeartBeat() {
        if !heartBeatTimerIsRunning {
            DispatchQueue.main.async {
                self.heartBeatTimer = Timer.scheduledTimer(timeInterval: TimeInterval(self.heartbeatIntervalInSecs), target: self, selector: #selector(VideoStreamViewController.startHeartBeat), userInfo: nil, repeats: true)
            }
            heartBeatTimerIsRunning = true
        }
        let url = URL(string: "\(server)/heartbeat?peer_id=\(myId!)")!
        var urlRequest = URLRequest(url: url)
        urlRequest.setValue("Client", forHTTPHeaderField: "Peer-Type")
        let heartbeatGet = URLSession.shared.dataTask(with: urlRequest)
        heartbeatGet.resume()
    }
    
    func signInCallback(data: Data?, response: URLResponse?, error: Error?) {
        let response = response as! HTTPURLResponse
        if response.statusCode == 200 && error == nil {
            if let data = data, let stringData = String(data: data, encoding: String.Encoding.utf8) {
                let peers = stringData.components(separatedBy: "\n")
                myId = Int(peers[0].components(separatedBy: ",")[1])!
                print("My id: \(myId)")
                
                for (index, peer) in peers.enumerated() {
                    if index >= 1 && peer.characters.count > 0 {
                        print("Peer \(index): \(peer)")
                        let parsed = peer.components(separatedBy: ",")
                        otherPeers[Int(parsed[1])!] = parsed[0]
                    }
                }
                startHangingGet()
                startHeartBeat()
                updatePeerList()
                
                request = nil
            }
        }
    }
    
    func signIn() {
        let urlString = URL(string: "\(server)/sign_in?peer_name=\(localName)")!
        var urlRequest = URLRequest(url: urlString)
        urlRequest.setValue("Client", forHTTPHeaderField: "Peer-Type")
        request = URLSession.shared.dataTask(with: urlRequest, completionHandler: signInCallback)
        request.resume()
    }
    
    func sendToPeer(peerId: Int, data: Data) {
        print(peerId, " Send ", data)
        if myId == -1 {
            print("Not connected")
            return
        }
        if peerId == myId {
            print("Can't send a message to oneself :)")
            return
        }
        
        let urlString = URL(string: "\(server)/message?peer_id=\(myId!)&to=\(peerId)")!
        var urlRequest = URLRequest(url: urlString)
        urlRequest.httpMethod = "POST"
        urlRequest.httpBody = data
        urlRequest.setValue("Client", forHTTPHeaderField: "Peer-Type")
        urlRequest.setValue("text/plain", forHTTPHeaderField: "Content-Type")
        let task = URLSession.shared.dataTask(with: urlRequest)
        task.resume()
    }
    
    
    func connect() {
        signIn()
    }
    
    func disconnect() {
        
        if request != nil {
            request.cancel()
            request = nil
        }
        if hangingGet != nil {
            hangingGet.cancel()
            hangingGet = nil
        }
        
        if heartBeatTimerIsRunning {
            heartBeatTimerIsRunning = false
            heartBeatTimer.invalidate()
        }
        
        if myId != -1 {
            let urlString = URL(string: "\(server)/sign_out?peer_id=\(myId!)")!
            var urlRequest = URLRequest(url: urlString)
            urlRequest.setValue("Client", forHTTPHeaderField: "Peer-Type")
            request = URLSession.shared.dataTask(with: urlRequest)
            request.resume()
            myId = -1
        }
    }
    
    deinit {
        disconnect()
        RTCCleanupSSL()
    }
    
    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }
    
    func convertDictionaryToData(dict: [String: String]) -> Data? {
        var jsonData: Data? = nil
        do {
            jsonData = try JSONSerialization.data(withJSONObject: dict, options: JSONSerialization.WritingOptions.prettyPrinted)
        } catch {
            print(error)
        }
        return jsonData
    }
}

extension VideoStreamViewController : RTCDataChannelDelegate {
    func dataChannel(_ dataChannel: RTCDataChannel, didReceiveMessageWith buffer: RTCDataBuffer) {
        print("received buffer")
    }
    
    func dataChannelDidChangeState(_ dataChannel: RTCDataChannel) {
        print("channel changed state \(dataChannel.readyState.rawValue)")
    }
}

extension VideoStreamViewController : RTCEAGLVideoViewDelegate {
    func videoView(_ videoView: RTCEAGLVideoView, didChangeVideoSize size: CGSize) {
        print("didchangevideosize")
    }
}

extension VideoStreamViewController : UIPickerViewDelegate, UIPickerViewDataSource {
    func numberOfComponents(in pickerView: UIPickerView) -> Int {
        return 1
    }
    
    func pickerView(_ pickerView: UIPickerView, numberOfRowsInComponent component: Int) -> Int {
        return peerListDisplay.count
    }
    func pickerView(_ pickerView: UIPickerView, titleForRow row: Int, forComponent component: Int) -> String? {
        return peerListDisplay[row]
    }
    
    func pickerView(_ pickerView: UIPickerView, didSelectRow row: Int, inComponent component: Int) {
        if peerListDisplay.count > 1 && row != 0 {
            print(peerListDisplay[row])
            let peerListDisplayString = peerListDisplay[row]
            let endPeerIdIndex = peerListDisplayString.range(of: ",", options: .literal)!.lowerBound
            let startPeerIdIndex = peerListDisplayString.startIndex
            
            let peerId = Int(peerListDisplayString[startPeerIdIndex..<endPeerIdIndex])!
            joinPeer(peerId: peerId)
        }
    }
}

extension VideoStreamViewController : RTCPeerConnectionDelegate {
    public func peerConnection(_ peerConnection: RTCPeerConnection, didChange stateChanged: RTCSignalingState) {}
    public func peerConnection(_ peerConnection: RTCPeerConnection, didChange newState: RTCIceGatheringState) {}
    public func peerConnection(_ peerConnection: RTCPeerConnection, didRemove stream: RTCMediaStream) {}
    public func peerConnectionShouldNegotiate(_ peerConnection: RTCPeerConnection) {}
    public func peerConnection(_ peerConnection: RTCPeerConnection, didChange newState: RTCIceConnectionState) {}
    public func peerConnection(_ peerConnection: RTCPeerConnection, didRemove candidates: [RTCIceCandidate]) {}
    
    public func peerConnection(_ peerConnection: RTCPeerConnection, didAdd stream: RTCMediaStream) {
        print("new stream for a remote peer")
        videoStream = stream
        if let videoTrack = stream.videoTracks.last {
            videoTrack.add(renderView)
            
            DispatchQueue.main.async {
                self.view.addSubview(self.renderView)
            }
        }
    }
    
    public func peerConnection(_ peerConnection: RTCPeerConnection, didGenerate candidate: RTCIceCandidate) {
        print("new ice candidate found")
        let jsonData: [String: String] = [
            "sdpMLineIndex" : String(candidate.sdpMLineIndex),
            "sdpMid": candidate.sdpMid!,
            "candidate": candidate.sdp
        ]
        self.sendToPeer(peerId: newIceCandidatePeerId, data: self.convertDictionaryToData(dict: jsonData)!)
    }
    
    public func peerConnection(_ peerConnection: RTCPeerConnection, didOpen dataChannel: RTCDataChannel) {
        print("didOpenChannel has been called, (js equiv = ondatachannel")
        inputChannel = dataChannel
        inputChannel.delegate = self
    }
}

extension VideoStreamViewController {
    func matCreate() -> [CGFloat] {
        let out: [CGFloat] = [ 1.0, 0.0, 0.0, 0.0,   0.0, 1.0, 0.0, 0.0,  0.0, 0.0, 1.0, 0.0,  0.0, 0.0, 0.0, 1.0]
        return out
    }
    
    func matClone(a: [CGFloat]) -> [CGFloat] {
        var out: [CGFloat] = [ 1.0, 0.0, 0.0, 0.0,   0.0, 1.0, 0.0, 0.0,  0.0, 0.0, 1.0, 0.0,  0.0, 0.0, 0.0, 1.0]
        
        out[0] = a[0]
        out[1] = a[1]
        out[2] = a[2]
        out[3] = a[3]
        out[4] = a[4]
        out[5] = a[5]
        out[6] = a[6]
        out[7] = a[7]
        out[8] = a[8]
        out[9] = a[9]
        out[10] = a[10]
        out[11] = a[11]
        out[12] = a[12]
        out[13] = a[13]
        out[14] = a[14]
        out[15] = a[15]
        
        return out
    }
    
    func matMultiply(a: [CGFloat], b: [CGFloat]) -> [CGFloat] {
        var out: [CGFloat] = [ 0.0, 0.0, 0.0, 0.0,   0.0, 0.0, 0.0, 0.0,  0.0, 0.0, 0.0, 0.0,  0.0, 0.0, 0.0, 0.0]
        
        let a00 = a[0], a01 = a[1], a02 = a[2], a03 = a[3]
        let a10 = a[4], a11 = a[5], a12 = a[6], a13 = a[7]
        let a20 = a[8], a21 = a[9], a22 = a[10], a23 = a[11]
        let a30 = a[12], a31 = a[13], a32 = a[14], a33 = a[15]
        
        var b0  = b[0], b1 = b[1], b2 = b[2], b3 = b[3]
        out[0] = b0*a00 + b1*a10 + b2*a20 + b3*a30
        out[1] = b0*a01 + b1*a11 + b2*a21 + b3*a31
        out[2] = b0*a02 + b1*a12 + b2*a22 + b3*a32
        out[3] = b0*a03 + b1*a13 + b2*a23 + b3*a33
        
        b0 = b[4]; b1 = b[5]; b2 = b[6]; b3 = b[7]
        out[4] = b0*a00 + b1*a10 + b2*a20 + b3*a30
        out[5] = b0*a01 + b1*a11 + b2*a21 + b3*a31
        out[6] = b0*a02 + b1*a12 + b2*a22 + b3*a32
        out[7] = b0*a03 + b1*a13 + b2*a23 + b3*a33
        
        b0 = b[8]; b1 = b[9]; b2 = b[10]; b3 = b[11]
        out[8] = b0*a00 + b1*a10 + b2*a20 + b3*a30
        out[9] = b0*a01 + b1*a11 + b2*a21 + b3*a31
        out[10] = b0*a02 + b1*a12 + b2*a22 + b3*a32
        out[11] = b0*a03 + b1*a13 + b2*a23 + b3*a33
        
        b0 = b[12]; b1 = b[13]; b2 = b[14]; b3 = b[15]
        out[12] = b0*a00 + b1*a10 + b2*a20 + b3*a30
        out[13] = b0*a01 + b1*a11 + b2*a21 + b3*a31
        out[14] = b0*a02 + b1*a12 + b2*a22 + b3*a32
        out[15] = b0*a03 + b1*a13 + b2*a23 + b3*a33
        
        return out
    }
    
    func matTranslate(v: [CGFloat]) -> [CGFloat] {
        let out = [ 1.0, 0.0, 0.0, 0.0,   0.0, 1.0, 0.0, 0.0,  0.0, 0.0, 1.0, 0.0,  v[0], v[1], v[2], 1.0]
        
        return out
    }
    
    func matRotateX(rad: CGFloat) -> [CGFloat] {
        let s = sin(rad)
        let c = cos(rad)
        
        let out = [ 1.0, 0.0, 0.0, 0.0,   0.0, c, s, 0.0,  0.0, -s, c, 0.0,  0.0, 0.0, 0.0, 1.0]
        
        return out
    }
    
    func matRotateY(rad: CGFloat) -> [CGFloat] {
        let s = sin(rad)
        let c = cos(rad)
        
        let out = [ c, 0.0, -s, 0.0,   0.0, 1.0, 0.0, 0.0,  s, 0.0, c, 0.0,  0.0, 0.0, 0.0, 1.0]
        
        return out
    }
    
    func matRotateZ(rad: CGFloat) -> [CGFloat]
    {
        let s = sin(rad)
        let c = cos(rad)
        
        let out = [ c, s, 0.0, 0.0,   -s, c, 0.0, 0.0,  0.0, 0.0, 1.0, 0.0,  0.0, 0.0, 0.0, 1.0]
        
        return out
    }
}
