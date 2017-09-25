package microsoft.a3dtoolkitandroid;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

import org.webrtc.DataChannel;
import org.webrtc.MediaStream;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.VideoRenderer;
import org.webrtc.VideoTrack;

import java.util.ArrayList;
import java.util.Timer;

public class StreamActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_stream);
    }

    ArrayList<String> otherPeers = new ArrayList<>();
    int myId = -1;
    int heartBeatTimer;
    boolean heartBeatTimerIsRunning = false;
    int heartbeatIntervalInSecs = 5;
    let server = Config.server;
    String localName = "android_client";
    int messageCounter = 0;
    PeerConnection peerConnection;
    PeerConnectionFactory PeerConnectionFactory;
    int newIceCandidatePeerId = -1;
    var peerListDisplay = ["Select"];
    URLSessionTask hangingGet;
    URLSessionDataTask request;
    VideoRenderer renderView;
    VideoTrack videoTrack;
    MediaStream videoStream;
    DataChannel inputChannel;
    lazy var navTransform: [CGFloat] = {
        [unowned self] in
        self.matCreate()
    }();
    var navHeading: CGFloat = 0.0;
    var navPitch: CGFloat = 0.0;
    var navLocation: [CGFloat] = [ 0.0, 0.0, 0.0 ];
    var isFingerDown = false;
    var fingerDownX: CGFloat = 0;
    var fingerDownY: CGFloat = 0;
    var downPitch: CGFloat = 0.0;
    var downHeading: CGFloat = 0.0;
    var downLocation: [CGFloat] = [ 0.0, 0.0, 0.0 ];

    override func viewDidLoad() {
        super.viewDidLoad()

        RTCInitializeSSL()
        renderView.delegate = self

        pickerView.dataSource = self
        pickerView.delegate = self

        initGestureRecognizer()
        connect()
    }

    private void joinPeer(int peerId) {
        createPeerConnection(peerId);
        inputChannel = peerConnection.createDataChannel("inputDataChannel", new DataChannel.Init());
        inputChannel.registerObserver(new PeerConnection.Observer(){
            void onAddStream(MediaStream var1){

            }

            void onRemoveStream(MediaStream var1){

            }
        });

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

    private void createPeerConnection(int peerId) {
        newIceCandidatePeerId = peerId;

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
