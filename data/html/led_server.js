var gateway = `ws://192.168.4.1/ws`;
var websocket;
function initWebSocket() {
    console.log('WS: Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
}
function onOpen(event) {
    console.log('WS: Connection opened');
}

function onClose(event) {
    console.log('WS: Connection closed');
    setTimeout(initWebSocket, 2000);
}
function onMessage(event) {
    console.log("WS message:")
    console.log(event.data)
}

function handleButtonPress(evt){
    const dTxt = document.querySelector("#d_text").value;
    const dBright = document.querySelector("input:checked[name*='bright'").value;
    const dColor = document.querySelector("input:checked[name*='color'").value;
    var data = {messageType:'display',payload:{dTxt:dTxt, dBright:dBright,dColor:dColor} };
    var jsonData = JSON.stringify(data);
    console.log( jsonData );
    websocket.send(jsonData);
}

function onLoad(event) {
    initWebSocket();
    document.querySelector("#btn_update").addEventListener("click", handleButtonPress );
}
window.addEventListener('load', onLoad);
