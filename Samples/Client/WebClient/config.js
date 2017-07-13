var pcConfig = {
    "iceServers": [{
        "url": "turn:turnserver3dstreaming.centralus.cloudapp.azure.com:3478",
        "username": "user",
        "credential": "3Dstreaming0317",
        "credentialType": "password"
    }],
    "iceTransportPolicy": "relay"
};
var tempPasswordsTurnUrl = "turn:52.173.196.112:3478"

var defaultSignalingServerUrl = "http://3dtoolkit-signaling-server-auth.azurewebsites.net:80"

var aadConfig = {
    clientID: 'aacf1b7a-104c-4efe-9ca7-9f4916d6b66a',
    authority: "https://login.microsoftonline.com/tfp/3dtoolkit.onmicrosoft.com/b2c_1_signup",
    b2cScopes: ["openid"],
};

var identityManagementConfig = {
    turnCredsUr: "https://3dtoolkit-identity-management.azurewebsites.net/turnCreds"
}