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

function tempClassAdd( selector, className, timeOut ){
    let targetElement = document.querySelector(selector)
    targetElement.classList.add(className)
    setTimeout( function(){
        targetElement.classList.remove(className)
    }, timeOut )
}

function handleButtonPress(evt){
    const oTxt = document.querySelector("#d_text")
    const dTxt = String(oTxt.value).trim();
    const dBright = document.querySelector("input:checked[name*='bright'").value
    const dColor = document.querySelector("input:checked[name*='color'").value
    tempClassAdd( "#btn_update","isClicked",1500)
    var data = {messageType:'display',payload:{dTxt:dTxt, dBright:dBright,dColor:dColor} }
    var jsonData = JSON.stringify(data)
    const tLen = oTxt.value.length
    oTxt.setSelectionRange(tLen, tLen)
    oTxt.focus()
    console.log( jsonData )
    websocket.send(jsonData)
}

function onLoad(event) {
    initWebSocket();
    document.querySelector("#btn_update").addEventListener("click", handleButtonPress );
    document.addEventListener("keyup", (event) => {
        if( event.code == 'Enter' ) document.querySelector("#btn_update").click()
        }, true)
}

window.addEventListener('load', onLoad)

