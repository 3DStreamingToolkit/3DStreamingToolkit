import UIKit
import CoreMotion
import WebRTC

class VideoStreamViewController: UIViewController {
    @IBOutlet weak var connectDisconnectButton: UIButton!
    @IBOutlet weak var pickerView: UIPickerView!
    @IBOutlet weak var renderView: RTCEAGLVideoView!
    static let peerListDisplayInitialMessage = "Select peer to join"
    static let connectButtonTitle = "Connect"
    static let disconnectButtonTitle = "Disconnect"
    static let connectDisconnectButtonErrorTitle = "Error: Please restart app"
    static let accelerometerButtonEnableTitle = "Enable Accelerometer"
    static let accelerometerButtonDisableTitle = "Disable Accelerometer"
    var isAccelerometerEnabled = true
    let heartbeatIntervalInSecs = 5
    let server = Config.signalingServer
    let localName = "ios_client"
    var fingerGestureRecognizer: UIPanGestureRecognizer!
    var accelerometerButton: UIButton!
    var mathMatrix = MathMatrix()
    var request: URLSessionDataTask!
    var hangingGet: URLSessionDataTask!
    var otherPeers = [Int: String]()
    var myId = -1
    var heartBeatTimer: Timer!
    var heartBeatTimerIsRunning = false
    var peerConnection: RTCPeerConnection!
    var peerConnectionFactory = RTCPeerConnectionFactory()
    var newIceCandidatePeerId = -1
    var peerListDisplay = [VideoStreamViewController.peerListDisplayInitialMessage]
    var videoStream: RTCMediaStream!
    var remoteVideoTrack: RTCVideoTrack!
    var inputChannel: RTCDataChannel!
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
    var roll: CGFloat = 0.0
    var prevYaw: CGFloat = 0.0
    var prevPitch: CGFloat = 0.0
    var prevRoll: CGFloat = 0.0
    lazy var navTransform: [CGFloat] = {
        [unowned self] in
        self.mathMatrix.matCreate()
        }()
    
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
                        self.connectDisconnectButton.setTitle(VideoStreamViewController.connectDisconnectButtonErrorTitle, for: .normal)
                    }
                }
            })
        } else {
            disconnectAsync(completionHandler: { (error) in
                if error != nil {
                    DispatchQueue.main.async {
                        self.connectDisconnectButton.setTitle(VideoStreamViewController.connectDisconnectButtonErrorTitle, for: .normal)
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
                self.startHeartBeatAsync(completionHandler: { (err) in
                    if let completionHandler = completionHandler {
                        completionHandler(err)
                    }
                })
            } else {
                self.disconnectAsync(completionHandler: { (err) in
                    if let completionHandler = completionHandler {
                        completionHandler(error ?? err)
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
                if index >= 1 && peer.count > 0 {
                    let parsed = peer.components(separatedBy: ",")
                    self.otherPeers[Int(parsed[1])!] = parsed[0]
                }
            }
        }
    }
    
    func createOfferAsync(completionHandler: @escaping (Data, Error?) -> Void) {
        let offerOptions = RTCMediaConstraints(mandatoryConstraints: ["OfferToReceiveAudio": "false","OfferToReceiveVideo": "true"], optionalConstraints: nil)
        peerConnection.offer(for: offerOptions) { (sdp, error) in
            let sdpH264 = sdp!.sdp.replacingOccurrences(of: "96 98 100 101", with: "101 96 98 100")
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
        prevRoll = roll
        yaw = CGFloat(attitude.yaw)
        pitch = CGFloat(attitude.pitch)
        roll = CGFloat(attitude.roll)
        let dheading = yaw - prevYaw
        let dpitch = pitch - prevPitch
        let droll = roll - prevRoll
        // do not send movement that is too small
        if isAccelerometerEnabled {
            if abs(dheading) < 0.005 && abs(dpitch) < 0.005 {
                return
            }
            prevNavHeading = navHeading
            prevNavPitch = navPitch
	    if UIDevice.current.orientation == .landscapeLeft {
            	navPitch = prevNavPitch - droll
            	navHeading = prevNavHeading - dheading
            } else if UIDevice.current.orientation == .landscapeRight {
            	navPitch = prevNavPitch + droll
            	navHeading = prevNavHeading - dheading
            } else {
            	navPitch = prevNavPitch + dpitch
            	navHeading = prevNavHeading - dheading
            }
            let locTransform =  mathMatrix.matMultiply(a: mathMatrix.matRotateY(rad: navHeading), b: mathMatrix.matRotateZ(rad: navPitch))
            navTransform = mathMatrix.matMultiply(a: mathMatrix.matTranslate(v: navLocation), b: locTransform)
            sendTransform()
        }
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
    
    @objc func handlePan(_ recognizer: UIPanGestureRecognizer) {
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
    
    @objc func handleAccelerometerButton(button: UIButton) {
        if isAccelerometerEnabled {
            button.setTitle(VideoStreamViewController.accelerometerButtonEnableTitle, for: .normal)
            button.backgroundColor = .green
            isAccelerometerEnabled = false
            self.fingerGestureRecognizer.minimumNumberOfTouches = 1
        } else {
            button.setTitle(VideoStreamViewController.accelerometerButtonDisableTitle, for: .normal)
            button.backgroundColor = .red
            isAccelerometerEnabled = true
            self.fingerGestureRecognizer.minimumNumberOfTouches = 2
        }
    }
    
    func initGestureRecognizer() {
        fingerGestureRecognizer = UIPanGestureRecognizer(target: self, action: #selector(VideoStreamViewController.handlePan(_:)))
        fingerGestureRecognizer.minimumNumberOfTouches = 1
        fingerGestureRecognizer.maximumNumberOfTouches = 2
        fingerGestureRecognizer.delegate = self
        self.renderView.addGestureRecognizer(fingerGestureRecognizer)
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
            let locTransform =  mathMatrix.matMultiply(a: mathMatrix.matRotateY(rad: navHeading), b: mathMatrix.matRotateZ(rad: navPitch))
            navTransform = mathMatrix.matMultiply(a: mathMatrix.matTranslate(v: navLocation), b: locTransform)
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
    
    func addAccelerometerButton() {
        self.isAccelerometerEnabled = true
        self.fingerGestureRecognizer.minimumNumberOfTouches = 2
        self.accelerometerButton = UIButton()
        self.accelerometerButton.translatesAutoresizingMaskIntoConstraints = false
        self.accelerometerButton.backgroundColor = .red
        self.accelerometerButton.setTitle(VideoStreamViewController.accelerometerButtonDisableTitle, for: .normal)
        self.accelerometerButton.isUserInteractionEnabled = true
        self.accelerometerButton.addTarget(self, action: #selector(self.handleAccelerometerButton), for: .touchUpInside)
        DispatchQueue.main.async {
            self.renderView.addSubview(self.accelerometerButton)
            let views: [String: Any] = ["button": self.accelerometerButton]
            let horizontalConstraint = NSLayoutConstraint.constraints(
                withVisualFormat: "H:|[button]|",
                options: [],
                metrics: nil,
                views: views)
            let verticalConstraint = NSLayoutConstraint.constraints(
                withVisualFormat: "V:[button(50)]|",
                options: [],
                metrics: nil,
                views: views)
            var allConstraints = [NSLayoutConstraint]()
            allConstraints += horizontalConstraint
            allConstraints += verticalConstraint
            NSLayoutConstraint.activate(allConstraints)
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
        if let val = val, val.count > 0 {
            return Int(val)!
        } else {
            return -1
        }
    }
    
    func pollServerAsync(completionHandler: ((Error?) -> Void)?) {
        let urlString = URL(string: "\(server)/wait?peer_id=\(myId)")!
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
    
    @objc func heartBeatTimerFunc() {
        startHeartBeatAsync(completionHandler: nil)
    }
    
    func startHeartBeatAsync(completionHandler: ((Error?) -> Void)?) {
        if !heartBeatTimerIsRunning {
            DispatchQueue.main.async {
                self.heartBeatTimer = Timer.scheduledTimer(timeInterval: TimeInterval(self.heartbeatIntervalInSecs), target: self, selector: #selector(VideoStreamViewController.heartBeatTimerFunc), userInfo: nil, repeats: true)
            self.heartBeatTimerIsRunning = true
            }
        }
        let url = URL(string: "\(server)/heartbeat?peer_id=\(myId)")!
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
        let urlString = URL(string: "\(server)/message?peer_id=\(myId)&to=\(peerId)")!
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
    
    func resetLocationInformation () {
        twoFingersWereUsed = false
        navHeading = 0.0
        navPitch = 0.0
        navLocation = [0.0, 0.0, 0.0]
        fingerDownX = 0
        fingerDownY = 0
        downPitch = 0.0
        downHeading = 0.0
        downLocation = [0.0, 0.0, 0.0]
        prevNavHeading = 0.0
        prevNavPitch = 0.0
        yaw = 0.0
        pitch = 0.0
        prevYaw = 0.0
        prevPitch = 0.0
    }
    
    func disconnectAsync(completionHandler: ((Error?) -> Void)?) {
        if myId != -1 {
            let urlString = URL(string: "\(server)/sign_out?peer_id=\(myId)")!
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
                DispatchQueue.main.async {
                    if self.accelerometerButton != nil {
                        self.accelerometerButton.removeConstraints(self.accelerometerButton.constraints)
                        self.accelerometerButton.removeFromSuperview()
                        self.accelerometerButton = nil
                        self.isAccelerometerEnabled = false
                    }
                    if self.renderView != nil && self.videoStream != nil && self.remoteVideoTrack != nil {
                        self.videoStream.removeVideoTrack(self.remoteVideoTrack)
                        self.remoteVideoTrack.remove(self.renderView)
                        self.remoteVideoTrack = nil
                        self.renderView.renderFrame(nil)
                        self.videoStream = nil
                    }
                }
                if self.fingerGestureRecognizer != nil {
                    self.fingerGestureRecognizer.minimumNumberOfTouches = 1
                }
                if self.inputChannel != nil {
                    self.inputChannel.close()
                    self.inputChannel = nil
                }
                if self.peerConnection != nil {
                    self.peerConnection.close()
                    self.peerConnection = nil
                }
                self.resetLocationInformation()
                self.otherPeers = [:]
                self.updatePeerList()
                DispatchQueue.main.async {
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
        if renderView != nil && videoStream.videoTracks.last != nil {
            remoteVideoTrack = videoStream.videoTracks.last
            remoteVideoTrack.add(renderView)
            if accelerometerButton == nil {
                addAccelerometerButton()
            }
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

extension VideoStreamViewController : UIGestureRecognizerDelegate {
    func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldReceive touch: UITouch) -> Bool {
        if let touchView = touch.view, let button = self.accelerometerButton {
            if touchView.isDescendant(of: button) {
                return false
            }
        }
        return true
    }
}
