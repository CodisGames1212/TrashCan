#include <WiFi.h>
#include <ESPAsyncWebServer.h> // Required: ESPAsyncWebServer & AsyncTCP libraries
#include <ESP32Servo.h>

/* * --- HARDWARE PINOUT ---
 * Ultrasonic Trig: GPIO 2
 * Ultrasonic Echo: GPIO 3
 * Servo PWM:       GPIO 10
 */

const int trigPin = 2;
const int echoPin = 3;
const int servoPin = 10;

// --- WIFI CONFIG ---
const char* ssid = "SmartBin_C3_Pro";
const char* password = NULL; // No password

// --- NETWORK CONFIG ---
IPAddress local_IP(10, 0, 0, 1);
IPAddress gateway(10, 0, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// --- GLOBAL STATE ---
Servo myServo;
AsyncWebServer server(80);
float currentDistance = 0;
bool lidOpen = false;
unsigned long lastMeasureTime = 0;
const int threshold = 20; // Trigger distance in cm

// --- HTML DASHBOARD (COLORFUL NEON THEME) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SMARTBIN // VIVID</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Outfit:wght@400;700&display=swap');
        body { 
            background: linear-gradient(135deg, #0f0c29, #302b63, #24243e); 
            color: white; 
            font-family: 'Outfit', sans-serif; 
            min-h-screen;
        }
        .neon-card {
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(12px);
            border: 1px solid rgba(255, 255, 255, 0.2);
            border-radius: 24px;
            box-shadow: 0 8px 32px 0 rgba(31, 38, 135, 0.37);
        }
        .gradient-text {
            background: linear-gradient(to right, #00f2fe, #4facfe);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .status-badge {
            transition: all 0.5s ease;
        }
        .progress-glow {
            box-shadow: 0 0 15px rgba(0, 242, 254, 0.5);
        }
    </style>
</head>
<body class="p-6">
    <div class="max-w-md mx-auto space-y-8">
        <header class="text-center space-y-2">
            <h1 class="text-4xl font-bold tracking-tight">SmartBin <span class="gradient-text">Vivid</span></h1>
            <p class="text-blue-200 text-sm opacity-80">CONNECTED VIA 10.0.0.1</p>
        </header>

        <!-- Distance Tracker -->
        <div class="neon-card p-8 text-center relative overflow-hidden">
            <div class="absolute top-0 right-0 p-4">
                <div id="signal" class="w-3 h-3 rounded-full bg-green-400 animate-pulse"></div>
            </div>
            <h2 class="text-blue-200 text-xs uppercase tracking-widest font-bold mb-2">Distance Reading</h2>
            <div class="flex items-center justify-center space-x-2">
                <span id="distance" class="text-7xl font-bold tracking-tighter">--</span>
                <span class="text-2xl font-light text-blue-300">cm</span>
            </div>
            <div class="mt-6 h-4 w-full bg-white/10 rounded-full overflow-hidden">
                <div id="dist-bar" class="h-full bg-gradient-to-right from-cyan-400 to-blue-500 transition-all duration-500 progress-glow" style="width: 0%"></div>
            </div>
        </div>

        <!-- Lid Status -->
        <div id="status-card" class="neon-card p-6 flex items-center justify-between transition-colors duration-500">
            <div>
                <p class="text-xs text-blue-200 uppercase font-bold">Lid Status</p>
                <h3 id="status-text" class="text-2xl font-bold">STANDBY</h3>
            </div>
            <div id="status-icon" class="p-4 rounded-2xl bg-white/10">
                <svg id="icon-svg" class="w-8 h-8 transition-transform duration-500" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 15v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2zm10-10V7a4 4 0 00-8 0v4h8z"></path></svg>
            </div>
        </div>

        <!-- Live Feed -->
        <div class="neon-card p-6">
            <h2 class="text-xs font-bold text-blue-200 mb-4 uppercase tracking-widest">System Events</h2>
            <div id="log-container" class="space-y-3 h-40 overflow-y-auto pr-2 scrollbar-hide text-sm">
                <div class="flex items-start space-x-3 text-blue-100/60">
                    <span class="font-bold text-cyan-400">SYS</span>
                    <p>Interface initialized successfully</p>
                </div>
            </div>
        </div>
    </div>

    <script>
        function addLog(msg, type = "INF") {
            const container = document.getElementById('log-container');
            const entry = document.createElement('div');
            entry.className = "flex items-start space-x-3 animate-fade-in";
            const colors = { "INF": "text-cyan-400", "ACT": "text-pink-400", "WRN": "text-yellow-400" };
            entry.innerHTML = `<span class="font-bold ${colors[type] || 'text-white'}">${type}</span><p>${msg}</p>`;
            container.prepend(entry);
        }

        async function updateData() {
            try {
                const response = await fetch('/api/data');
                const data = await response.json();
                
                // Update Distance
                document.getElementById('distance').innerText = Math.round(data.distance);
                document.getElementById('dist-bar').style.width = Math.min(data.distance, 100) + '%';
                
                const statusText = document.getElementById('status-text');
                const card = document.getElementById('status-card');
                const icon = document.getElementById('icon-svg');
                
                if (data.status === "OPEN") {
                    if (statusText.innerText !== "ACTIVE / OPEN") {
                        addLog("Motion detected - Opening Lid", "ACT");
                        card.style.background = "rgba(236, 72, 153, 0.2)"; // Pink tint
                        icon.innerHTML = '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 11V7a4 4 0 118 0m-4 8v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2z"></path>';
                    }
                    statusText.innerText = "ACTIVE / OPEN";
                } else {
                    if (statusText.innerText !== "STANDBY / CLOSED") {
                        addLog("Area cleared - Closing Lid", "INF");
                        card.style.background = "rgba(255, 255, 255, 0.1)";
                        icon.innerHTML = '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 15v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2zm10-10V7a4 4 0 00-8 0v4h8z"></path>';
                    }
                    statusText.innerText = "STANDBY / CLOSED";
                }
            } catch (e) {}
        }
        setInterval(updateData, 500);
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  myServo.attach(servoPin);
  myServo.write(0);

  // Set Static IP
  if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("Static IP Failed to configure");
  }

  // Initialize Wi-Fi Access Point without password
  WiFi.softAP(ssid, password);
  
  Serial.println("Vivid SmartBin Started");
  Serial.print("Access IP: ");
  Serial.println(WiFi.softAPIP());

  // Web Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"distance\":" + String(currentDistance) + 
                  ",\"status\":\"" + (lidOpen ? "OPEN" : "CLOSED") + "\"}";
    request->send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  if (millis() - lastMeasureTime > 150) {
    lastMeasureTime = millis();
    
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    
    long duration = pulseIn(echoPin, HIGH, 30000);
    if (duration > 0) {
      currentDistance = duration * 0.034 / 2;
    }

    if (currentDistance > 0 && currentDistance < threshold) {
      if (!lidOpen) {
        myServo.write(90);
        lidOpen = true;
      }
    } else {
      if (lidOpen) {
        myServo.write(0);
        lidOpen = false;
      }
    }
  }
}