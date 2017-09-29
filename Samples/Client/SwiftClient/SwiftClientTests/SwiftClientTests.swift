import XCTest
@testable import SwiftClient
import WebRTC

class SwiftClientTests: XCTestCase {
    var videoStreamVC: VideoStreamViewController!
    var timeoutSecs: TimeInterval! = TimeInterval(50)
    let dispatchGroup = DispatchGroup() //Force only one test to run at a time
    let randomPeerId = 10000
    
    override func setUp() {
        super.setUp()
        let storyboard = UIStoryboard(name: "Main", bundle: nil)
        videoStreamVC = storyboard.instantiateViewController(withIdentifier: "VideoStreamViewController") as! VideoStreamViewController
    }
    
    override func tearDown() {
        super.tearDown()
        videoStreamVC.disconnectAsync(completionHandler: nil)
        videoStreamVC = nil
        timeoutSecs = nil
    }
    
    func testSignInAsync() {
        dispatchGroup.enter()
        let signInExpectation = expectation(description: "signInExpectation")
        self.videoStreamVC.signInAsync { (data, response, error) in
            XCTAssertNil(error)
            XCTAssertNotNil(data)
            XCTAssertNotNil(response)
            XCTAssertEqual(response!.statusCode, 200)
            self.videoStreamVC.myId = 1000 // populate to avoid crashing from not calling connectAsync
            signInExpectation.fulfill()
            self.dispatchGroup.leave()
        }
        self.dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }
    
    func testConnectAsync() {
        dispatchGroup.enter()
        let connectAsyncExpectation = expectation(description: "connectAsyncExpectation")
        videoStreamVC.connectAsync { (error) in
            XCTAssertNil(error)
            connectAsyncExpectation.fulfill()
            self.dispatchGroup.leave()
        }
        self.dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }
    
    func testDisconnectAsync() {
        dispatchGroup.enter()
        let disconnectAsyncExpectation = expectation(description: "disconnectAsyncExpectation")
        videoStreamVC.connectAsync { (error) in
            XCTAssertNil(error)
            self.videoStreamVC.disconnectAsync(completionHandler: { (error) in
                XCTAssertNil(error)
                XCTAssertTrue(self.videoStreamVC.myId == -1)
                disconnectAsyncExpectation.fulfill()
                self.dispatchGroup.leave()
            })
        }
        self.dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }

    func testCreateOfferAsync() {
        dispatchGroup.enter()
        let createOfferAsyncExpectation = expectation(description: "createOfferAsyncExpectation")
        videoStreamVC.connectAsync { (error) in
            self.videoStreamVC.createPeerConnection()
            self.videoStreamVC.inputChannel = self.videoStreamVC.peerConnection.dataChannel(forLabel: "inputDataChannel", configuration: RTCDataChannelConfiguration())
            XCTAssertNil(error)
            self.videoStreamVC.createOfferAsync { (data, error) in
                XCTAssertNil(error)
                self.verifySdp(data: data, type: "offer")
                createOfferAsyncExpectation.fulfill()
                self.dispatchGroup.leave()
            }
        }
        self.dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }

    func testCreateAnswerAsync() {
        dispatchGroup.enter()
        let createAnswerAsyncExpectation = expectation(description: "createAnswerAsyncExpectation")
        videoStreamVC.connectAsync { (error) in
            self.videoStreamVC.createPeerConnection()
            XCTAssertNil(error)
            let path = Bundle.main.path(forResource: "server_offer", ofType:".txt")!
            let offer = try! String(contentsOfFile: path, encoding: .utf8)
            let sdpOffer = RTCSessionDescription(type: .offer, sdp: offer)
            self.videoStreamVC.createAnswerAsync(offerSdp: sdpOffer) { (data, error) in
                XCTAssertNil(error)
                self.verifySdp(data: data, type: "answer")
                createAnswerAsyncExpectation.fulfill()
                self.dispatchGroup.leave()
            }
        }
        self.dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }

    func testHandleOfferMessage() {
        dispatchGroup.enter()
        let path = Bundle.main.path(forResource: "server_offer", ofType:".txt")!
        let offerMessageString = try! String(contentsOfFile: path, encoding: .utf8)
        let offerMessageData = ["type": "offer", "sdp": offerMessageString]
        let handleOfferMessageExpectation = expectation(description: "handleOfferMessageExpectation")
        videoStreamVC.connectAsync { (error) in
            XCTAssertNil(error)
            self.videoStreamVC.handleOfferMessage(peerId: self.randomPeerId, messageData: offerMessageData, completionHandler: { (sdp, error) in
                XCTAssertNil(error)
                XCTAssertNotNil(sdp)
                handleOfferMessageExpectation.fulfill()
                self.dispatchGroup.leave()
            })
        }
        self.dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }
    
    func testHandleAnswerMessage() {
        dispatchGroup.enter()
        let handleAnswerMessageExpectation = expectation(description: "handleAnswerMessageExpectation")
        let path = Bundle.main.path(forResource: "server_answer", ofType:".txt")!
        let answerMessageString = try! String(contentsOfFile: path, encoding: .utf8)
        let answerMessageData = ["type": "answer", "sdp": answerMessageString]
        videoStreamVC.connectAsync { (error) in
            XCTAssertNil(error)
            self.videoStreamVC.createPeerConnection()
            self.videoStreamVC.inputChannel = self.videoStreamVC.peerConnection.dataChannel(forLabel: "inputDataChannel", configuration: RTCDataChannelConfiguration())
            self.videoStreamVC.createOfferAsync(completionHandler: { (data, error) in
                XCTAssertNil(error)
                self.videoStreamVC.handleAnswerMessage(messageData: answerMessageData) { (sdp, error) in
                    XCTAssertNil(error)
                    handleAnswerMessageExpectation.fulfill()
                    self.dispatchGroup.leave()
                }
            })
        }
        self.dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }
    
    func testControlFlowHandlePeerMessageAsyncAddIceCandidate() {
        dispatchGroup.enter()
        let addIceCandidateMessageData: [String: Any] = ["sdpMid": "video", "sdpMLineIndex": 0, "candidate": "candidate:337435484 1 udp 41885439 13.89.232.16 54812 typ relay raddr 0.0.0.0 rport 0 generation 0 ufrag ONII network-id 1 network-cost 50"]
        let data = try! JSONSerialization.data(withJSONObject: addIceCandidateMessageData, options: .prettyPrinted)
        let addIceCandidateMessageDataExpectation = expectation(description: "addIceCandidateMessageDataExpectation")
        videoStreamVC.connectAsync { (error) in
            self.videoStreamVC.createPeerConnection()
            XCTAssertNil(error)
            self.videoStreamVC.handlePeerMessageAsync(peerId: self.randomPeerId, data: data) { (data, sdp, candidate, error) in
                XCTAssertNotNil(candidate)
                addIceCandidateMessageDataExpectation.fulfill()
                self.dispatchGroup.leave()
            }
        }
        self.dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }
    
    func testControlFlowHandlePeerMessageByeMessageFromServer() {
        dispatchGroup.enter()
        let byeExpectation = expectation(description: "byeExpectation")
        videoStreamVC.connectAsync { (error) in
            self.videoStreamVC.createPeerConnection()
            XCTAssertNil(error)
            let serverBye = "BYE"
            let serverByeData = serverBye.data(using: .utf8)!
            let prevId = self.videoStreamVC.myId
            self.videoStreamVC.handlePeerMessageAsync(peerId: self.randomPeerId, data: serverByeData) { (data, sdp, candidate, error) in
                XCTAssertNotEqual(prevId, self.videoStreamVC.myId)
                XCTAssertNotNil(data)
                byeExpectation.fulfill()
                self.dispatchGroup.leave()
            }
        }
        self.dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }
    
    func testControlFlowHandlePeerMessageAsyncOffer() {
        dispatchGroup.enter()
        let path = Bundle.main.path(forResource: "server_offer", ofType:".txt")!
        let offerMessageString = try! String(contentsOfFile: path, encoding: .utf8)
        let offerMessageData = ["type": "offer", "sdp": offerMessageString]
        let data = try! JSONSerialization.data(withJSONObject: offerMessageData, options: .prettyPrinted)
        let offerMessageExpectation = expectation(description: "offerMessageExpectation")
        videoStreamVC.connectAsync { (error) in
            self.videoStreamVC.handlePeerMessageAsync(peerId: 200, data: data) { (data, sdp, candidate, error) in
                XCTAssertNotNil(data)
                offerMessageExpectation.fulfill()
                self.dispatchGroup.leave()
            }
        }
        self.dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }

    func testControlFlowHandlePeerMessageAsyncAnswer() {
        dispatchGroup.enter()
        let path = Bundle.main.path(forResource: "server_answer", ofType:".txt")!
        let answerMessageString = try! String(contentsOfFile: path, encoding: .utf8)
        let answerMessageData = ["type": "answer", "sdp": answerMessageString]
        let data = try! JSONSerialization.data(withJSONObject: answerMessageData, options: .prettyPrinted)
        let answerMessageExpectation = expectation(description: "answerMessageExpectation")
        videoStreamVC.connectAsync { (error) in
            self.videoStreamVC.createPeerConnection()
            self.videoStreamVC.handlePeerMessageAsync(peerId: 2000, data: data) { (data, sdp, candidate, error) in
                XCTAssertNotNil(sdp)
                answerMessageExpectation.fulfill()
                self.dispatchGroup.leave()
            }
        }
        self.dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }
    
    func testHandleServerNotification() {
        dispatchGroup.enter()
        let handleServerNotificationExpectation = expectation(description: "handleServerNotificationExpectation")
        let unique_id = -100000
        let test_server_name = "test_server"
        let newPotentialPeerString = "\(test_server_name),\(unique_id),1"
        let newPotentialPeerData = newPotentialPeerString.data(using: .utf8)!
        self.videoStreamVC.connectAsync { (error) in
            let previousPeers = self.videoStreamVC.otherPeers
            self.videoStreamVC.handleServerNotification(data: newPotentialPeerData)
            XCTAssertTrue(previousPeers.count + 1 == self.videoStreamVC.otherPeers.count)
            XCTAssertNotNil(self.videoStreamVC.otherPeers[unique_id])
            XCTAssertTrue(self.videoStreamVC.otherPeers[unique_id]! == test_server_name)
            handleServerNotificationExpectation.fulfill()
            self.dispatchGroup.leave()
        }
        dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }
    
    func testStartHeartBeatAsync() {
        dispatchGroup.enter()
        let startHeartBeatAsyncExpectation = expectation(description: "heartBeatExpectation")
        videoStreamVC.signInAsync { (data, response, error) in
            self.videoStreamVC.myId = 1000 // populate to avoid crashing from not calling connectAsync
            self.videoStreamVC.startHeartBeatAsync(completionHandler: { error in
                XCTAssertNil(error)
                startHeartBeatAsyncExpectation.fulfill()
                self.dispatchGroup.leave()
            })
        }
        dispatchGroup.wait()
        waitForExpectations(timeout: timeoutSecs, handler: nil)
    }
    
    func verifySdp(data: Data, type: String) {
        var sdpJson = try! JSONSerialization.jsonObject(with: data, options: .allowFragments) as! [String: String]
        guard let sdpType = sdpJson["type"], let sdp = sdpJson["sdp"] else {
            print("error")
            return
        }
        XCTAssertEqual(sdpType, type)
        let videoOfferIndex = sdp.range(of: "a=group:BUNDLE video data", options: .literal)
        let inputChannelOfferIndex = sdp.range(of: "a=sctpmap:5000 webrtc-datachannel 1024", options: .literal)
        XCTAssertNotNil(videoOfferIndex)
        XCTAssertNotNil(inputChannelOfferIndex)
    }
}
