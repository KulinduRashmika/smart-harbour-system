
#include <ESP8266WiFi.h>
#include "HX711.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- LCD ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- HX711 ----------
#define DT_PIN D5
#define SCK_PIN D4
HX711 scale;

// ---------- Ultrasonic ----------
#define TRIG_PIN D6
#define ECHO_PIN D7

// ---------- WiFi ----------
const char* ssid = "vivo Y29";
const char* password = "00001111";

WiFiServer server(80);

// ---------- Calibration ----------
float calibration_factor = -7050;

// ---------- History buffer ----------
const int HISTORY_SIZE = 20;
String histContainer[HISTORY_SIZE];
float histWeight[HISTORY_SIZE];
unsigned long histTime[HISTORY_SIZE];
int histIndex = 0;     // next write index
int histCount = 0;     // number of stored entries (<= HISTORY_SIZE)

// ---------- Last measured values (global) ----------
float lastWeight = 0.0;
bool lastObjectDetected = false;

// ---------- Helper: add history entry (circular) ----------
void addHistoryEntry(const String &containerId, float weight, unsigned long ts) {
  histContainer[histIndex] = containerId;
  histWeight[histIndex] = weight;
  histTime[histIndex] = ts;
  histIndex = (histIndex + 1) % HISTORY_SIZE;
  if (histCount < HISTORY_SIZE) histCount++;
}

// ---------- Ultrasonic read ----------
long readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 9999; // no echo
  long distance = duration * 0.034 / 2; // cm
  return distance;
}

// ---------- Serve JSON history ----------
void handleHistory(WiFiClient &client) {
  // Build JSON array in chronological order (oldest -> newest)
  String json = "[";
  int start = (histCount == HISTORY_SIZE) ? histIndex : 0;
  for (int i = 0; i < histCount; ++i) {
    int idx = (start + i) % HISTORY_SIZE;
    json += "{";
    json += "\"container\":\"" + histContainer[idx] + "\",";
    json += "\"weight\":" + String(histWeight[idx], 2) + ",";
    json += "\"time\":" + String(histTime[idx]);
    json += "}";
    if (i < histCount - 1) json += ",";
  }
  json += "]";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Cache-Control: no-store");
  client.println("Connection: close");
  client.println();
  client.println(json);
}

// ---------- Serve current status JSON ----------
void handleStatus(WiFiClient &client) {
  String json = "{";
  json += "\"object\":" + String(lastObjectDetected ? "true" : "false") + ",";
  json += "\"weight\":" + String(lastWeight, 2) + ",";
  json += "\"time\":" + String(millis());
  json += "}";
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Cache-Control: no-store");
  client.println("Connection: close");
  client.println();
  client.println(json);
}

// ---------- Handle Save request: expects ?container=ID[&weight=VAL] ----------
void handleSave(WiFiClient &client, const String &reqLine) {
  // parse container and weight from reqLine
  String container = "";
  String weightStr = "";
  int cPos = reqLine.indexOf("container=");
  if (cPos != -1) {
    int start = cPos + 10;
    int end = reqLine.indexOf('&', start);
    if (end == -1) {
      end = reqLine.indexOf(' ', start); // until space (end of first line)
    }
    container = reqLine.substring(start, end);
    container.replace("%20", " "); // basic decode for spaces
  }
  int wPos = reqLine.indexOf("weight=");
  if (wPos != -1) {
    int start = wPos + 7;
    int end = reqLine.indexOf('&', start);
    if (end == -1) {
      end = reqLine.indexOf(' ', start);
    }
    weightStr = reqLine.substring(start, end);
  }

  float weightToSave = lastWeight;
  if (weightStr.length() > 0) {
    weightToSave = weightStr.toFloat();
  }

  if (container.length() == 0) {
    // return error page
    client.println("HTTP/1.1 400 Bad Request");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println("<html><body><h3>Missing container id</h3><a href='/'>Back</a></body></html>");
    return;
  }

  unsigned long ts = millis();
  addHistoryEntry(container, weightToSave, ts);

  // respond with a small confirm and redirect back to main page
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<html><body style='font-family:Arial;text-align:center;'>");
  client.println("<h2>Saved</h2>");
  client.print("<p>Container: ");
  client.print(container);
  client.print("<br>Weight: ");
  client.print(weightToSave, 2);
  client.print(" g<br>Time: ");
  client.print(ts);
  client.println("</p>");
  client.println("<a href='/'>Return</a>");
  client.println("</body></html>");
}

// ---------- Serve main page (HTML + JS + Chart.js) ----------
void handleMainPage(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  client.println("<title>Load Cell - History & Save</title>");
  client.println("<link href='https://fonts.googleapis.com/css2?family=Poppins:wght@300;500;700&display=swap' rel='stylesheet'>");
  client.println("<style>");
  client.println("body{font-family:'Poppins',sans-serif;background:#f0f2f5;margin:0;padding:18px;display:flex;flex-direction:column;align-items:center;} ");
  client.println(".card{background:#fff;width:920px;max-width:95%;border-radius:12px;padding:14px;margin-bottom:12px;box-shadow:0 8px 30px rgba(0,0,0,0.08);} ");
  client.println(".row{display:flex;gap:12px;flex-wrap:wrap;} .col{flex:1;} ");
  client.println(".title{font-size:20px;font-weight:700;margin-bottom:6px;} .big{font-size:44px;font-weight:700;color:#333;} ");
  client.println("input[type=text]{padding:10px;font-size:16px;width:220px;border-radius:8px;border:1px solid #ddd;} ");
  client.println(".btn{background:#2d8cff;color:#fff;padding:10px 18px;border-radius:8px;text-decoration:none;cursor:pointer;border:none;font-size:16px;} ");
  client.println("table{width:100%;border-collapse:collapse;margin-top:8px;} th,td{padding:8px;border-bottom:1px solid #eee;text-align:left;font-size:14px;} ");
  client.println("#chartBox{width:100%;height:300px;} .search{margin-left:12px;padding:8px;border-radius:8px;border:1px solid #ddd;width:180px;} ");
  client.println(".small{font-size:12px;color:#666;} ");
  client.println("</style>");
  client.println("</head><body>");

  // top card: current weight & save input
  client.println("<div class='card'>");
  client.println("<div class='row'>");
  client.println("<div class='col' style='min-width:260px;'>");
  client.println("<div class='title'>Current Weight</div>");
  client.println("<div id='currentWeight' class='big'>-- g</div>");
  client.println("<div class='small'>Object: <span id='objState'>--</span> &nbsp; Time: <span id='curTime'>--</span></div>");
  client.println("</div>");
  client.println("<div style='min-width:260px;display:flex;flex-direction:column;justify-content:center;'>");
  client.println("<div style='margin-bottom:8px;font-weight:600'>Save with Container ID</div>");
  client.println("<div style='display:flex;align-items:center;gap:8px;'>");
  client.println("<input id='containerId' type='text' placeholder='Enter container ID'/>");
  client.println("<button class='btn' id='saveBtn'>SAVE</button>");
  client.println("</div>");
  client.println("<div style='margin-top:8px;color:#555;font-size:13px'>You can also include a note in the ID (e.g. CONT-01)</div>");
  client.println("</div>");
  client.println("</div>"); // row
  client.println("</div>"); // card

  // chart + search + table
  client.println("<div class='card'>");
  client.println("<div style='display:flex;align-items:center;justify-content:space-between;margin-bottom:8px;'>");
  client.println("<div class='title'>History</div>");
  client.println("<div>");
  client.println("<input id='search' class='search' type='text' placeholder='Search container id...' />");
  client.println("<button class='btn' id='refreshBtn' style='margin-left:8px'>Refresh</button>");
  client.println("</div>");
  client.println("</div>");

  client.println("<div id='chartBox'><canvas id='chart' width='800' height='300'></canvas></div>");

  client.println("<div style='margin-top:12px;overflow:auto;max-height:300px;'>");
  client.println("<table id='histTable'><thead><tr><th>#</th><th>Container ID</th><th>Weight (g)</th><th>Time (ms)</th></tr></thead><tbody></tbody></table>");
  client.println("</div>");

  client.println("</div>"); // card

  // JS: Chart.js and AJAX
  client.println("<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>");
  client.println("<script>");
  client.println("let chartCtx = document.getElementById('chart').getContext('2d');");
  client.println("let weightChart = new Chart(chartCtx, { type:'line', data:{ labels:[], datasets:[{ label:'Weight (g)', data:[], fill:true, tension:0.3, pointRadius:3 }] }, options:{ scales:{ x:{ display:true, title:{display:true,text:'Entry'} }, y:{ beginAtZero:true } }, plugins:{ legend:{display:false} } } });");

  // fetch and update current status
  client.println("async function updateStatus(){ try{ const res=await fetch('/status'); const js=await res.json(); document.getElementById('currentWeight').innerText = js.weight.toFixed(2)+' g'; document.getElementById('objState').innerText = js.object? 'YES':'NO'; document.getElementById('curTime').innerText = js.time; }catch(e){ console.error(e); } }");

  // fetch history and update table & chart
  client.println("async function updateHistory(){ try{ const res=await fetch('/history'); const arr=await res.json(); const tbl=document.querySelector('#histTable tbody'); tbl.innerHTML=''; let labels=[]; let data=[]; let idx=1; const search=document.getElementById('search').value.trim().toLowerCase(); for(let i=0;i<arr.length;i++){ const it=arr[i]; if(search && it.container.toLowerCase().indexOf(search)===-1) continue; const tr=document.createElement('tr'); tr.innerHTML = `<td>${idx}</td><td>${it.container}</td><td>${it.weight.toFixed(2)}</td><td>${it.time}</td>`; tbl.appendChild(tr); labels.push(idx); data.push(it.weight); idx++; } weightChart.data.labels = labels; weightChart.data.datasets[0].data = data; weightChart.update(); }catch(e){ console.error(e); } }");

  // save btn click: get container id, request server /save?container=...&weight=...
  client.println("document.getElementById('saveBtn').addEventListener('click', async ()=>{ const id=document.getElementById('containerId').value.trim(); if(!id){ alert('Enter Container ID'); return; } try{ // fetch current status first to get latest weight from server side");
  client.println(" const s = await fetch('/status'); const st = await s.json(); const weight = st.weight.toFixed(2); const resp = await fetch('/save?container='+encodeURIComponent(id)+'&weight='+encodeURIComponent(weight)); if(resp.ok){ document.getElementById('containerId').value=''; await updateHistory(); await updateStatus(); alert('Saved!'); }else{ alert('Save failed'); } }catch(e){ console.error(e); alert('Error'); } });");

  client.println("document.getElementById('search').addEventListener('input', ()=> updateHistory());");
  client.println("document.getElementById('refreshBtn').addEventListener('click', ()=>{ updateHistory(); updateStatus(); });");

  client.println("// initial load");
  client.println("updateStatus(); updateHistory();");
  client.println("// optional: poll status every 2s to show live weight");
  client.println("setInterval(updateStatus,2000);");
  client.println("</script>");

  client.println("</body></html>");
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  Wire.begin(D2, D1);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("System Booting");
  delay(800);
  lcd.clear();

  // init scale
  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare();

  lcd.setCursor(0,0);
  lcd.print("HX711 Ready");
  delay(800);
  lcd.clear();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  WiFi.begin(ssid, password);
  lcd.setCursor(0,0);
  lcd.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP().toString());
  delay(1500);
  lcd.clear();

  server.begin();

  // initialize history
  for (int i = 0; i < HISTORY_SIZE; ++i) {
    histContainer[i] = "";
    histWeight[i] = 0.0;
    histTime[i] = 0;
  }
}

// ---------- Loop ----------
void loop() {
  // measure
  long distance = readDistance();
  lastObjectDetected = (distance > 2 && distance < 15); // adjust threshold
  if (lastObjectDetected) {
    lastWeight = scale.get_units(5);
  } else {
    lastWeight = 0.0;
  }

  // LCD update
  lcd.clear();
  if (!lastObjectDetected) {
    lcd.setCursor(0,0); lcd.print("No Object");
  } else {
    lcd.setCursor(0,0); lcd.print("Weight:");
    lcd.setCursor(0,1); lcd.print(lastWeight, 2); lcd.print(" g");
  }

  // handle client
  WiFiClient client = server.available();
  if (client) {
    // read request line
    String req = client.readStringUntil('\r');
    client.readStringUntil('\n'); // consume LF

    // Basic routing based on the first line (e.g., "GET /save?container=... HTTP/1.1")
    if (req.indexOf("GET /history") >= 0) {
      handleHistory(client);
    } else if (req.indexOf("GET /status") >= 0) {
      handleStatus(client);
    } else if (req.indexOf("GET /save") >= 0) {
      handleSave(client, req);
    } else {
      // Serve main page
      handleMainPage(client);
    }

    // drain remaining client data then stop
    unsigned long timeout = millis() + 300;
    while (client.available() && millis() < timeout) client.read();
    client.stop();
  }

  delay(150); // loop delay
}
