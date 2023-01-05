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
    var data = {messageId:4,messageType:'display',payload:{d_text:"hello", d_color:"red",d_bright:"low"} };
    var jsonData = JSON.stringify(data);
    console.log("button press, data");
    console.log( jsonData );
    websocket.send(jsonData);
}

function onLoad(event) {
    initWebSocket();
    document.querySelector("#btn_update").addEventListener("click", handleButtonPress );
}
window.addEventListener('load', onLoad);
