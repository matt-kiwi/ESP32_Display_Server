var gateway = `ws://192.168.4.1/ws`;
  var websocket;
  function initWebSocket() {
    console.log('WS: Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage; // <-- add this line
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

  window.addEventListener('load', onLoad);
  function onLoad(event) {
    initWebSocket();
  }