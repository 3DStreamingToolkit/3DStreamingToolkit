import UIKit
import CoreMotion
import WebRTC

class VideoStreamViewController: UIViewController {
    @IBOutlet weak var connectDisconnectButton: UIButton!
    @IBOutlet weak var pickerView: UIPickerView!
    var request: URLSessionDataTask!
    var hangingGet: URLSessionDataTask!
    var otherPeers = [Int: String]()
    var myId: Int!
    var heartBeatTimer: Timer!
    var heartBeatTimerIsRunning = false
    let heartbeatIntervalInSecs = 5
    let server = Config.signalingServer
    let localName = "ios_client"
    var peerConnection: RTCPeerConnection!
    var peerConnectionFactory = RTCPeerConnectionFactory()
    var newIceCandidatePeerId = -1
    static let peerListDisplayInitialMessage = "Select peer to join"
    static let connectButtonTitle = "Connect"
    static let disconnectButtonTitle = "Disconnect"
    static let connectDisconnectButtonErrorInstructions = "Error: Please restart app"
    var peerListDisplay = [VideoStreamViewController.peerListDisplayInitialMessage]
    @IBOutlet weak var renderView: RTCEAGLVideoView!
    var videoStream: RTCMediaStream!
    var inputChannel: RTCDataChannel!
    lazy var navTransform: [CGFloat] = {
        [unowned self] in
        self.matCreate()
        }()
    var navHeading: CGFloat = 0.0
    var navPitch: CGFloat = 0.0
    var navLocation: [CGFloat] = [0.0, 0.0, 0.0]
    var twoFingersWereUsed = false
    var fingerDownX: CGFloat = 0
    var fingerDownY: CGFloat = 0
    var downPitch: CGFloat = 0.0
    var downHeading: CGFloat = 0.0
    var downLocation: [CGFloat] = [0.0, 0.0, 0.0]
    var motionManager = CMMotionManager()
    var prevNavHeading: CGFloat = 0.0
    var prevNavPitch: CGFloat = 0.0
    var yaw: CGFloat = 0.0
    var pitch: CGFloat = 0.0
    var prevYaw: CGFloat = 0.0
    var prevPitch: CGFloat = 0.0
    
    override func viewDidLoad() {
        super.viewDidLoad()
        renderView.delegate = self
        pickerView.dataSource = self
        pickerView.delegate = self
        pickerView.isUserInteractionEnabled = false
        connectDisconnectButton.setTitle(VideoStreamViewController.connectButtonTitle, for: .normal)
        initGestureRecognizer()
        initMotion()
    }
    
    @IBAction func handleConnectDisconnectButtonPressed(_ sender: UIButton) {
        if connectDisconnectButton.currentTitle == VideoStreamViewController.connectButtonTitle {
            connectAsync(completionHandler: { (error) in
                if error != nil {
                    DispatchQueue.main.async {
                        self.connectDisconnectButton.setTitle(VideoStreamViewController.connectDisconnectButtonErrorInstructions, for: .normal)
                    }
                }
            })
        } else {
            disconnectAsync(completionHandler: { (error) in
                if error != nil {
                    DispatchQueue.main.async {
                        self.connectDisconnectButton.setTitle(VideoStreamViewController.connectDisconnectButtonErrorInstructions, for: .normal)
                    }
                }
            })
        }
    }
    
    func connectAsync(completionHandler: ((Error?) -> Void)?) {
        signInAsync { (data, response, error) in
            if response?.statusCode == 200 && error == nil, let data = data {
                DispatchQueue.main.async {
                    if self.pickerView != nil {
                        self.pickerView.isUserInteractionEnabled = true
                    }
                    if self.connectDisconnectButton != nil {
                        self.connectDisconnectButton.setTitle(VideoStreamViewController.disconnectButtonTitle, for: .normal)
                    }
                }
                self.initIdsFromPeerList(data: data)
                self.updatePeerList()
                self.pollServerAsync(completionHandler: nil)
                self.startHeartBeatAsync(completionHandler: { (error) in
                    if let completionHandler = completionHandler {
                        completionHandler(error)
                    }
                })
            } else {
                self.disconnectAsync(completionHandler: { (err) in
                    if let completionHandler = completionHandler {
                        completionHandler(error)
                    }
                })
            }
        }
    }
    
    func initIdsFromPeerList(data: Data) {
        if let stringData = String(data: data, encoding: .utf8) {
            let peers = stringData.components(separatedBy: "\n")
            self.myId = Int(peers[0].components(separatedBy: ",")[1])!
            for (index, peer) in peers.enumerated() {
                if index >= 1 && peer.characters.count > 0 {
                    let parsed = peer.components(separatedBy: ",")
                    self.otherPeers[Int(parsed[1])!] = parsed[0]
                }
            }
        }
    }
    
    func createOfferAsync(completionHandler: @escaping (Data, Error?) -> Void) {
        let offerOptions = RTCMediaConstraints(mandatoryConstraints: ["OfferToReceiveAudio": "false","OfferToReceiveVideo": "true"], optionalConstraints: nil)
        peerConnection.offer(for: offerOptions) { (sdp, error) in
            let sdpH264 = sdp!.sdp.replacingOccurrences(of: "96 98 100 102", with: "100 96 98 102")
            let offer = RTCSessionDescription(type: .offer, sdp: sdpH264)
            self.peerConnection.setLocalDescription(offer, completionHandler: { (error) in
                let jsonData = [
                    "type": "offer",
                    "sdp": offer.sdp
                ]
                completionHandler(self.convertDictionaryToData(dict: jsonData)!, error)
            })
        }
    }
    
    func createAnswerAsync(offerSdp: RTCSessionDescription, completionHandler: @escaping (Data, Error?) -> Void) {
        peerConnection.setRemoteDescription(offerSdp, completionHandler: { (error) in
            let mediaConstraints = RTCMediaConstraints(mandatoryConstraints: ["OfferToReceiveAudio": "false","OfferToReceiveVideo": "true"], optionalConstraints: nil)
            self.peerConnection.answer(for: mediaConstraints, completionHandler: { (answerSdp, error) in
                self.peerConnection.setLocalDescription(answerSdp!, completionHandler: { (error) in
                    var jsonData = ["sdp" : answerSdp!.sdp]
                    var answerType: String
                    switch answerSdp!.type {
                    case .offer:
                        answerType = "offer"
                    case .answer, .prAnswer:
                        answerType = "answer"
                    }
                    jsonData["type"] = answerType
                    let data = self.convertDictionaryToData(dict: jsonData)!
                    completionHandler(data, error)
                })
            })
        })
    }
    
    func initMotion() {
        motionManager.deviceMotionUpdateInterval = 0.1
        motionManager.startDeviceMotionUpdates(to: OperationQueue.current!) {
            (deviceMotion, error) -> Void in
            self.handleDeviceMotionUpdate(deviceMotion: deviceMotion!)
        }
    }
    
    func handleDeviceMotionUpdate(deviceMotion: CMDeviceMotion) {
        let attitude = deviceMotion.attitude
        prevYaw = yaw
        prevPitch = pitch
        yaw = CGFloat(attitude.yaw)
        pitch = CGFloat(attitude.pitch)
        let dheading = yaw - prevYaw
        let dpitch = pitch - prevPitch
        // do not send movement that is too small
        if abs(dheading) < 0.005 && abs(dpitch) < 0.005 {
            return
        }
        prevNavHeading = navHeading
        prevNavPitch = navPitch
        navHeading = prevNavHeading - dheading
        navPitch = prevNavPitch + dpitch
        let locTransform =  matMultiply(a: matRotateY(rad: navHeading), b: matRotateZ(rad: navPitch))
        navTransform = matMultiply(a: matTranslate(v: navLocation), b: locTransform)
        sendTransform()
    }
    
    func joinPeer(peerId: Int, completionHandler: ((Data, Error?) -> Void)?) {
        newIceCandidatePeerId = peerId
        createPeerConnection()
        inputChannel = self.peerConnection.dataChannel(forLabel: "inputDataChannel", configuration: RTCDataChannelConfiguration())
        inputChannel.delegate = self
        createOfferAsync { (offer, error) in
            self.sendToPeerAsync(peerId: peerId, data: offer, completionHandler: { err in
                if let completionHandler = completionHandler {
                    completionHandler(offer, error)
                }
            })
        }
    }
    
    func createPeerConnection() {
        let defaultPeerConnectionConstraints = RTCMediaConstraints(mandatoryConstraints: nil,
                                                                   optionalConstraints: ["DtlsSrtpKeyAgreement": "true"])
        let rtcConfig = RTCConfiguration()
        let iceServer = RTCIceServer(urlStrings: [Config.turnServer],
                                     username: Config.username,
                                     credential: Config.credential,
                                     tlsCertPolicy: Config.tlsCertPolicy)
        rtcConfig.iceServers = [iceServer]
        rtcConfig.iceTransportPolicy = .relay
        peerConnection = peerConnectionFactory.peerConnection(with: rtcConfig, constraints: defaultPeerConnectionConstraints, delegate: self)
    }
    
    func handlePan(_ recognizer: UIPanGestureRecognizer) {
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
    
    func beginTouch(tappedPoint: CGPoint) {
        fingerDownX = tappedPoint.x
        fingerDownY = tappedPoint.y
        downPitch = navPitch
        downHeading = navHeading
        downLocation[0] = navLocation[0]
        downLocation[1] = navLocation[1]
        downLocation[2] = navLocation[2]
    }
    
    func navOnFingerDown(recognizer: UIPanGestureRecognizer) {
        let tappedPoint = recognizer.location(in: self.view)
        beginTouch(tappedPoint: tappedPoint)
    }
    
    func navOnFingerUp() {
        twoFingersWereUsed = false
    }
    
    func navOnFingerMove(recognizer: UIPanGestureRecognizer) {
        if recognizer.numberOfTouches == 1 {
            if twoFingersWereUsed {
                twoFingersWereUsed = false
                let tappedPoint = recognizer.location(in: self.view)
                beginTouch(tappedPoint: tappedPoint)
                return
            }
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
            let tappedPoint = recognizer.location(ofTouch: 1, in: self.view)
            if !twoFingersWereUsed {
                twoFingersWereUsed = true
                fingerDownY = tappedPoint.y
            }
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
    
    func sendTransform() {
        if inputChannel != nil {
            var eye = [navTransform[12], navTransform[13], navTransform[14]]
            var lookat = [navTransform[12] + navTransform[0],
                           navTransform[13] + navTransform[1],
                           navTransform[14] + navTransform[2]]
            var up = [navTransform[4], navTransform[5], navTransform[6]]
            let data = "\(eye[0]), \(eye[1]), \(eye[2]), \(lookat[0]), \(lookat[1]), \(lookat[2]), \(up[0]), \(up[1]), \(up[2])"
            let msg = [
                "type" : "camera-transform-lookat",
                "body" : data
            ]
            let buffer = RTCDataBuffer(data: convertDictionaryToData(dict: msg)!, isBinary: false)
            inputChannel.sendData(buffer)
        }
    }
    
    func addIceCandidateToPeerConnection(peerId: Int, messageData: [String: Any]) -> RTCIceCandidate {
        newIceCandidatePeerId = peerId
        let candidate = RTCIceCandidate(sdp: messageData["candidate"] as! String,
                                        sdpMLineIndex: Int32(messageData["sdpMLineIndex"] as! Int),
                                        sdpMid: (messageData["sdpMid"] as! String))
        peerConnection.add(candidate)
        return candidate
    }
    
    func handleOfferMessage(peerId: Int, messageData: [String: Any], completionHandler: ((Data, Error?) -> Void)?) {
        newIceCandidatePeerId = peerId
        createPeerConnection()
        let sdp = RTCSessionDescription(type: .offer, sdp: messageData["sdp"] as! String)
        createAnswerAsync(offerSdp: sdp, completionHandler: { (sdpAnswer, error) in
            self.sendToPeerAsync(peerId: peerId, data: sdpAnswer, completionHandler: { err in
                if let completionHandler = completionHandler {
                    completionHandler(sdpAnswer, error)
                }
            })
        })
    }

    func handleAnswerMessage(messageData: [String: Any], completionHandler: ((RTCSessionDescription, Error?) -> Void)?) {
        let sdp = RTCSessionDescription(type: .answer, sdp: messageData["sdp"] as! String)
        self.peerConnection.setRemoteDescription(sdp, completionHandler: { (error) in
            if let completionHandler = completionHandler {
                completionHandler(sdp, error)
            }
        })
    }
    
    func handlePeerMessageAsync(peerId: Int, data: Data, completionHandler: ((Data?, RTCSessionDescription?, RTCIceCandidate?, Error?) -> Void)?) {
        var messageData: [String: Any]!
        do {
            messageData = try JSONSerialization.jsonObject(with: data) as! [String: Any]
        } catch {
            let stringData = String(data: data, encoding: .utf8)!
            if stringData == "BYE", let completionHandler = completionHandler {
                disconnectAsync(completionHandler: { (error) in
                    self.connectAsync(completionHandler: { err in
                        completionHandler(data, nil, nil, error ?? err)
                    })
                })
            } else if let completionHandler = completionHandler {
                let error = NSError(domain: "Unaccounted for message from server", code: 500, userInfo: nil)
                completionHandler(nil, nil, nil, error)
            }
            return
        }
        guard let stringType = messageData["type"] as? String else {
            let candidate = addIceCandidateToPeerConnection(peerId: peerId, messageData: messageData)
            if let completionHandler = completionHandler {
                completionHandler(nil, nil, candidate, nil)
            }
            return
        }
        if stringType == "offer" {
            handleOfferMessage(peerId: peerId, messageData: messageData, completionHandler: { (sdpAnswer, error) in
                if let completionHandler = completionHandler {
                    completionHandler(sdpAnswer, nil, nil, error)
                }
            })
        } else {
            handleAnswerMessage(messageData: messageData, completionHandler: { (sdpAnswer, error) in
                if let completionHandler = completionHandler {
                    completionHandler(nil, sdpAnswer, nil, error)
                }
            })
        }
    }
    
    func updatePeerList() {
        self.peerListDisplay = [VideoStreamViewController.peerListDisplayInitialMessage]
        for (peerId, name) in self.otherPeers {
            self.peerListDisplay.append("\(peerId), Name: \(name)")
        }
        DispatchQueue.main.async {
            if self.pickerView != nil {
                self.pickerView.reloadAllComponents()
            }
        }
    }
    
    func handleServerNotification(data: Data) {
        let stringData = String(data: data, encoding: .utf8)!
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
    
    func pollServerAsync(completionHandler: ((Error?) -> Void)?) {
        let urlString = URL(string: "\(server)/wait?peer_id=\(myId!)")!
        let urlRequest = URLRequest(url: urlString)
        let hangingGet = URLSession.shared.dataTask(with: urlRequest, completionHandler: { (data, response, error) in
            if let response = response as? HTTPURLResponse, let data = data, response.statusCode == 200 {
                let peerId = self.parseIntHeader(r: response, name: "Pragma")
                if (peerId == self.myId) {
                    self.handleServerNotification(data: data)
                    if let completionHandler = completionHandler {
                        completionHandler(nil)
                    }
                } else {
                    self.handlePeerMessageAsync(peerId: peerId, data: data, completionHandler: { (_, _, _, error) in
                        if let completionHandler = completionHandler {
                            completionHandler(error)
                        }
                    })
                }
            } else if let error = error as NSError?, error.code != NSURLErrorTimedOut {
                // disconnect and then reconnect on all errors except for timeouts
                self.disconnectAsync(completionHandler: { _ in
                    self.connectAsync(completionHandler: { _ in
                        if let completionHandler = completionHandler {
                            completionHandler(error)
                        }
                    })
                })
            }
            if self.myId != -1 {
                self.pollServerAsync(completionHandler: nil)
            }
        })
        hangingGet.resume()
    }
    
    func heartBeatTimerFunc() {
        startHeartBeatAsync(completionHandler: nil)
    }
    
    func startHeartBeatAsync(completionHandler: ((Error?) -> Void)?) {
        if !heartBeatTimerIsRunning {
            DispatchQueue.main.async {
                self.heartBeatTimer = Timer.scheduledTimer(timeInterval: TimeInterval(self.heartbeatIntervalInSecs), target: self, selector: #selector(VideoStreamViewController.heartBeatTimerFunc), userInfo: nil, repeats: true)
            self.heartBeatTimerIsRunning = true
            }
        }
        let url = URL(string: "\(server)/heartbeat?peer_id=\(myId!)")!
        let urlRequest = URLRequest(url: url)
        let heartbeatGet = URLSession.shared.dataTask(with: urlRequest) { (data, response, error) in
            if let completionHandler = completionHandler {
                completionHandler(error)
            }
        }
        heartbeatGet.resume()
    }
    
    func signInAsync(completionHandler: ((Data?, HTTPURLResponse?, Error?) -> Void)?) {
        let urlString = URL(string: "\(server)/sign_in?peer_name=\(localName)")!
        let urlRequest = URLRequest(url: urlString)
        request = URLSession.shared.dataTask(with: urlRequest, completionHandler: { (data, response, error) in
            if let completionHandler = completionHandler {
                completionHandler(data, response as? HTTPURLResponse, error)
            }
        })
        request.resume()
    }
    
    func sendToPeerAsync(peerId: Int, data: Data, completionHandler: ((Error?) -> Void)?) {
        if myId == -1 {
            if let completionHandler = completionHandler {
                let error = NSError(domain: "Not connected", code: 500, userInfo: nil)
                completionHandler(error)
            }
            return
        }
        if peerId == myId {
            if let completionHandler = completionHandler {
                let error = NSError(domain: "Can't send a message to oneself :)", code: 500, userInfo: nil)
                completionHandler(error)
            }
            return
        }
        let urlString = URL(string: "\(server)/message?peer_id=\(myId!)&to=\(peerId)")!
        var urlRequest = URLRequest(url: urlString)
        urlRequest.httpMethod = "POST"
        urlRequest.httpBody = data
        urlRequest.setValue("text/plain", forHTTPHeaderField: "Content-Type")
        let task = URLSession.shared.dataTask(with: urlRequest) { (data, response, error) in
            if let completionHandler = completionHandler {
                completionHandler(error)
            }
        }
        task.resume()
    }
    
    func disconnectAsync(completionHandler: ((Error?) -> Void)?) {
        if myId != -1 {
            let urlString = URL(string: "\(server)/sign_out?peer_id=\(myId!)")!
            let urlRequest = URLRequest(url: urlString)
            let task = URLSession.shared.dataTask(with: urlRequest, completionHandler: { (_, _, error) in
                self.myId = -1
                if let request = self.request {
                    request.cancel()
                }
                if let hangingGet = self.hangingGet {
                    hangingGet.cancel()
                }
                if self.heartBeatTimerIsRunning {
                    self.heartBeatTimerIsRunning = false
                    self.heartBeatTimer.invalidate()
                }
                if self.renderView != nil && self.videoStream != nil {
                    self.videoStream.videoTracks.last?.remove(self.renderView)
                }

                DispatchQueue.main.async {
                    self.otherPeers = [:]
                    self.updatePeerList()
                    if self.pickerView != nil {
                        self.pickerView.isUserInteractionEnabled = false
                    }
                    if self.connectDisconnectButton != nil {
                        self.connectDisconnectButton.setTitle(VideoStreamViewController.connectButtonTitle, for: .normal)
                    }
                }
                if let completionHandler = completionHandler {
                    completionHandler(error)
                }
            })
            task.resume()
        }
    }
    
    deinit {
        disconnectAsync(completionHandler: nil)
    }
    
    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
    }
    
    func convertDictionaryToData(dict: [String: String]) -> Data? {
        return (try! JSONSerialization.data(withJSONObject: dict, options: JSONSerialization.WritingOptions.prettyPrinted))
    }
}

extension VideoStreamViewController : RTCDataChannelDelegate {
    func dataChannel(_ dataChannel: RTCDataChannel, didReceiveMessageWith buffer: RTCDataBuffer) {}
    func dataChannelDidChangeState(_ dataChannel: RTCDataChannel) {}
}

extension VideoStreamViewController : RTCEAGLVideoViewDelegate {
    func videoView(_ videoView: RTCEAGLVideoView, didChangeVideoSize size: CGSize) {}
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
            let peerListDisplayString = peerListDisplay[row]
            let endPeerIdIndex = peerListDisplayString.range(of: ",", options: .literal)!.lowerBound
            let startPeerIdIndex = peerListDisplayString.startIndex
            let peerId = Int(peerListDisplayString[startPeerIdIndex..<endPeerIdIndex])!
            joinPeer(peerId: peerId, completionHandler: nil)
        }
    }
    
    func pickerViewConnect(row: Int) {
        let peerListDisplayString = peerListDisplay[row]
        let endPeerIdIndex = peerListDisplayString.range(of: ",", options: .literal)!.lowerBound
        let startPeerIdIndex = peerListDisplayString.startIndex
        let peerId = Int(peerListDisplayString[startPeerIdIndex..<endPeerIdIndex])!
        joinPeer(peerId: peerId, completionHandler: nil)
    }
}

extension VideoStreamViewController : RTCPeerConnectionDelegate {
    func peerConnection(_ peerConnection: RTCPeerConnection, didChange stateChanged: RTCSignalingState) {}
    func peerConnection(_ peerConnection: RTCPeerConnection, didChange newState: RTCIceGatheringState) {}
    func peerConnection(_ peerConnection: RTCPeerConnection, didRemove stream: RTCMediaStream) {}
    func peerConnectionShouldNegotiate(_ peerConnection: RTCPeerConnection) {}
    func peerConnection(_ peerConnection: RTCPeerConnection, didChange newState: RTCIceConnectionState) {}
    func peerConnection(_ peerConnection: RTCPeerConnection, didRemove candidates: [RTCIceCandidate]) {}
    
    func peerConnection(_ peerConnection: RTCPeerConnection, didAdd stream: RTCMediaStream) {
        videoStream = stream
        if renderView != nil && stream.videoTracks.last != nil {
            videoStream.videoTracks.last?.add(renderView)
        }
    }
    
    func peerConnection(_ peerConnection: RTCPeerConnection, didGenerate candidate: RTCIceCandidate) {
        let jsonData: [String: String] = [
            "sdpMLineIndex" : String(candidate.sdpMLineIndex),
            "sdpMid": candidate.sdpMid!,
            "candidate": candidate.sdp
        ]
        self.sendToPeerAsync(peerId: newIceCandidatePeerId, data: self.convertDictionaryToData(dict: jsonData)!, completionHandler: nil)
    }
    
    func peerConnection(_ peerConnection: RTCPeerConnection, didOpen dataChannel: RTCDataChannel) {
        inputChannel = dataChannel
        inputChannel.delegate = self
    }
}

extension VideoStreamViewController {
    //  This source was inspired by http://glmatrix.net/docs/mat4.js.html and the
    //  following notice / credit is due. The code has been slightly modified
    //  to work in Swift 3
    
    /* Copyright (c) 2015, Brandon Jones, Colin MacKenzie IV.
     Permission is hereby granted, free of charge, to any person obtaining a copy
     of this software and associated documentation files (the "Software"), to deal
     in the Software without restriction, including without limitation the rights
     to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
     copies of the Software, and to permit persons to whom the Software is
     furnished to do so, subject to the following conditions:
     The above copyright notice and this permission notice shall be included in
     all copies or substantial portions of the Software.
     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
     IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
     AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
     LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
     OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
     THE SOFTWARE. */
    
    func matCreate() -> [CGFloat] {
        let out: [CGFloat] = [1.0, 0.0, 0.0, 0.0,   0.0, 1.0, 0.0, 0.0,  0.0, 0.0, 1.0, 0.0,  0.0, 0.0, 0.0, 1.0]
        return out
    }
    
    func matClone(a: [CGFloat]) -> [CGFloat] {
        var out: [CGFloat] = [1.0, 0.0, 0.0, 0.0,   0.0, 1.0, 0.0, 0.0,  0.0, 0.0, 1.0, 0.0,  0.0, 0.0, 0.0, 1.0]
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
        var out: [CGFloat] = [0.0, 0.0, 0.0, 0.0,   0.0, 0.0, 0.0, 0.0,  0.0, 0.0, 0.0, 0.0,  0.0, 0.0, 0.0, 0.0]
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
        let out = [1.0, 0.0, 0.0, 0.0,   0.0, 1.0, 0.0, 0.0,  0.0, 0.0, 1.0, 0.0,  v[0], v[1], v[2], 1.0]
        return out
    }
    
    func matRotateX(rad: CGFloat) -> [CGFloat] {
        let s = sin(rad)
        let c = cos(rad)
        let out = [1.0, 0.0, 0.0, 0.0,   0.0, c, s, 0.0,  0.0, -s, c, 0.0,  0.0, 0.0, 0.0, 1.0]
        return out
    }
    
    func matRotateY(rad: CGFloat) -> [CGFloat] {
        let s = sin(rad)
        let c = cos(rad)
        let out = [c, 0.0, -s, 0.0,   0.0, 1.0, 0.0, 0.0,  s, 0.0, c, 0.0,  0.0, 0.0, 0.0, 1.0]
        return out
    }
    
    func matRotateZ(rad: CGFloat) -> [CGFloat] {
        let s = sin(rad)
        let c = cos(rad)
        let out = [c, s, 0.0, 0.0,   -s, c, 0.0, 0.0,  0.0, 0.0, 1.0, 0.0,  0.0, 0.0, 0.0, 1.0]
        return out
    }
}
