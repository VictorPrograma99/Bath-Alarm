/*Este codigo contiene la lectura de un 
 * sensor de proximidad y apagar y encender un led con un servidor web
 */
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFiClient.h>


// Red WiFi
const char* ssid = "MI GATO ESTIBEN_2.4GHZ";
const char* password = "Colombia2025";

// Pines del sensor ultrasónico
#define TRIGGER_PIN 3
#define ECHO_PIN 2

// Pin del LED
#define LED_PIN 8

WebServer server(80);

bool mensajeEnviado = false;
bool ledManual = false;
unsigned long lastBlink = 0;
bool ledState = false;

unsigned long presenceStartTime = 0;
bool objectDetected = false;
bool ledShouldBlink = false;

// Función para medir distancia
long readDistance() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  long distance = duration * 0.034 / 2;
  return distance;
}

// Página principal
void handleRoot() {
  String html = R"=====( 
  <!DOCTYPE html>
  <html lang='es'>
  <head>
    <meta charset='UTF-8'>
    <title>Sensor Ultrasónico</title>
    <style>
      body {
        font-family: Arial, sans-serif;
        background: #f4f4f4;
        text-align: center;
        padding: 40px;
      }
      .container {
        background: #fff;
        border-radius: 15px;
        box-shadow: 0 0 10px rgba(0,0,0,0.1);
        padding: 20px;
        display: inline-block;
      }
      h1 { color: #333; }
      p { font-size: 1.2em; }
      #distancia, #tiempo {
        font-weight: bold;
        font-size: 1.5em;
        color: #007BFF;
      }
      button {
        padding: 10px 20px;
        font-size: 1em;
        margin-top: 15px;
        border: none;
        border-radius: 5px;
        background-color: #007BFF;
        color: white;
        cursor: pointer;
      }
      button:hover {
        background-color: #0056b3;
      }
    </style>
    <script>
      function actualizarDatos() {
        fetch('/data')
          .then(response => response.json())
          .then(data => {
            document.getElementById('distancia').innerText = data.distancia + ' cm';
            document.getElementById('tiempo').innerText = data.tiempo + ' s';
          });
      }

      setInterval(actualizarDatos, 1000);
      window.onload = actualizarDatos;
    </script>
  </head>
  <body>
    <div class='container'>
      <h1>Distancia Detectada</h1>
      <p id='distancia'>Cargando...</p>
      <p>Tiempo desde que se detectó presencia:</p>
      <p id='tiempo'>0 s</p>
      <form action="/led/on" method="get">
        <button>Encender LED</button>
      </form>
      <form action="/led/off" method="get">
        <button style="background-color:red;">Apagar LED</button>
      </form>
    </div>
  </body>
  </html>
  )=====";

  server.send(200, "text/html", html);
}

// Endpoint JSON con distancia y tiempo
void handleData() {
  long distance = readDistance();

  if (distance > 0 && distance < 60) {
    if (!objectDetected) {
      presenceStartTime = millis();
      objectDetected = true;
    }
  } else {
    objectDetected = false;
    presenceStartTime = 0;
    ledShouldBlink = false;
    mensajeEnviado = false;  // Reinicia cuando ya no hay persona
  }

  unsigned long elapsedTime = objectDetected ? (millis() - presenceStartTime) / 1000 : 0;

  if (elapsedTime >= 10 && !mensajeEnviado) {
    sendWhatsAppMessage("¡Se detectó una persona durante más de 10 segundos!");
    mensajeEnviado = true;
  }

  String json = "{\"distancia\":" + String(distance) + ",\"tiempo\":" + String(elapsedTime) + "}";
  server.send(200, "application/json", json);
}

// Encender LED manualmente
void handleLedOn() {
  ledManual = true;
  digitalWrite(LED_PIN, HIGH);
  server.sendHeader("Location", "/");
  server.send(303);
}

// Apagar LED manualmente
void handleLedOff() {
  ledManual = true;
  digitalWrite(LED_PIN, LOW);
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  if (!MDNS.begin("sensor")) {
    Serial.println("Error iniciando mDNS");
    while (true);
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/led/on", handleLedOn);
  server.on("/led/off", handleLedOff);

  server.begin();
  MDNS.addService("http", "tcp", 80);
}

void loop() {
  server.handleClient();

  if (!ledManual) {
    if (ledShouldBlink) {
      if (millis() - lastBlink > 300) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        lastBlink = millis();
      }
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  }
}
String urlencode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}
void sendWhatsAppMessage(const String& message) {
  HTTPClient http;

  String phoneNumber = "573135381900";  // Reemplaza con tu número (sin +)
  String apiKey = "4009828";         // Reemplaza con tu API Key
  String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&text=" + urlencode(message) + "&apikey=" + apiKey;

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.println("Mensaje enviado con éxito.");
  } else {
    Serial.println("Error al enviar mensaje. Código: " + String(httpCode));
  }

  http.end();
}
