var pcConfigStatic = {
    "iceServers": [{
        "urls": "turn:turnserveruri:5349",
        "username": "username",
        "credential": "password",
        "credentialType": "password"
    }],
    "iceTransportPolicy": "relay"
};

var pcConfigDynamic = {
    "iceServers": [{
        "urls": "turn:turnserveruri:5349",
        "credentialType": "password"
    }],
    "iceTransportPolicy": "relay"
};


var defaultSignalingServerUrl = "http://localhost:3001"

var aadConfig = {
    clientID: 'clientid',
    authority: "https://login.microsoftonline.com/tfp/tenant.onmicrosoft.com/b2c_1_signup",
    b2cScopes: ["openid"],
};

var identityManagementConfig = {
    turnCredsUrl: "https://identitymanagementapi"
};

var heartbeatIntervalInMs = 5000;