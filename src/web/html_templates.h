#ifndef HTML_TEMPLATES_H
#define HTML_TEMPLATES_H

#include <Arduino.h>

// ============================================================================
// HTML TEMPLATES FOR WEB INTERFACE
// ============================================================================
// These templates are stored in PROGMEM to save RAM
// They are referenced by the functions in html_pages.cpp

const char MAIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>FluidDash</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #1a1a1a; color: #fff; }
    .container { max-width: 800px; margin: 0 auto; }
    h1 { color: #00bfff; }
    .card { background: #2a2a2a; padding: 20px; margin: 15px 0; border-radius: 8px; }
    .status-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
    .status-item { padding: 10px; background: #333; border-radius: 5px; }
    .status-label { color: #888; font-size: 12px; }
    .status-value { font-size: 24px; font-weight: bold; color: #00bfff; }
    .temp-ok { color: #00ff00; }
    .temp-warn { color: #ffaa00; }
    .temp-hot { color: #ff0000; }
    button { background: #00bfff; color: #fff; border: none; padding: 12px 24px;
             border-radius: 5px; cursor: pointer; font-size: 16px; margin: 5px; }
    button:hover { background: #0099cc; }
    .link-button { display: inline-block; text-decoration: none; }
  </style>
</head>
<body>
  <div class='container'>
    <h1>🛡️ FluidDash</h1>

    <div class='card'>
      <h2>System Status</h2>
      <div class='status-grid' id='status'>
        <div class='status-item'>
          <div class='status-label'>CNC Status</div>
          <div class='status-value' id='cnc_status'>Loading...</div>
        </div>
        <div class='status-item'>
          <div class='status-label'>Max Temperature</div>
          <div class='status-value' id='max_temp'>--°C</div>
        </div>
        <div class='status-item'>
          <div class='status-label'>Fan Speed</div>
          <div class='status-value' id='fan_speed'>--%</div>
        </div>
        <div class='status-item'>
          <div class='status-label'>PSU Voltage</div>
          <div class='status-value' id='psu_volt'>--V</div>
        </div>
      </div>
    </div>

    <div class='card'>
      <h2>Configuration</h2>
      <a href='/settings' class='link-button'><button>⚙️ User Settings</button></a>
      <a href='/admin' class='link-button'><button>🔧 Admin/Calibration</button></a>
      <a href='/wifi' class='link-button'><button>📡 WiFi Setup</button></a>
      <button onclick='restart()'>🔄 Restart Device</button>
    </div>

    <div class='card'>
      <h2>Information</h2>
      <p><strong>Device Name:</strong> %DEVICE_NAME%.local</p>
      <p><strong>IP Address:</strong> %IP_ADDRESS%</p>
      <p><strong>FluidNC:</strong> %FLUIDNC_IP%</p>
      <p><strong>Version:</strong> v0.7</p>
    </div>
  </div>

  <script>
    function updateStatus() {
      fetch('/api/status')
        .then(r => r.json())
        .then(data => {
          document.getElementById('cnc_status').textContent = data.machine_state;

          let maxTemp = Math.max(...data.temperatures);
          let tempEl = document.getElementById('max_temp');
          tempEl.textContent = maxTemp.toFixed(1) + '°C';
          tempEl.className = 'status-value ' +
            (maxTemp > 50 ? 'temp-hot' : maxTemp > 35 ? 'temp-warn' : 'temp-ok');

          document.getElementById('fan_speed').textContent = data.fan_speed + '%';
          document.getElementById('psu_volt').textContent = data.psu_voltage.toFixed(1) + 'V';
        });
    }

    function restart() {
      if (confirm('Restart device?')) {
        fetch('/api/restart', {method: 'POST'})
          .then(() => alert('Restarting... Reconnect in 30 seconds'));
      }
    }

    function resetWiFi() {
      if (confirm('Reset WiFi settings? Device will restart in AP mode.')) {
        fetch('/api/reset-wifi', {method: 'POST'})
          .then(() => alert('WiFi reset. Connect to FluidDash-Setup network.'));
      }
    }

    updateStatus();
    setInterval(updateStatus, 2000);
  </script>
</body>
</html>
)rawliteral";


const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Settings - FluidDash</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #1a1a1a; color: #fff; }
    .container { max-width: 600px; margin: 0 auto; }
    h1, h2 { color: #00bfff; }
    .card { background: #2a2a2a; padding: 20px; margin: 15px 0; border-radius: 8px; }
    label { display: block; margin: 15px 0 5px; color: #aaa; }
    input, select { width: 100%; padding: 10px; background: #333; color: #fff;
                    border: 1px solid #555; border-radius: 5px; box-sizing: border-box; }
    button { background: #00bfff; color: #fff; border: none; padding: 12px 24px;
             border-radius: 5px; cursor: pointer; font-size: 16px; margin: 10px 5px 0 0; }
    button:hover { background: #0099cc; }
    .back-btn { background: #666; }
    .back-btn:hover { background: #555; }
    .success { background: #00ff00; color: #000; padding: 10px; border-radius: 5px;
               margin: 10px 0; display: none; }
  </style>
</head>
<body>
  <div class='container'>
    <h1>⚙️ User Settings</h1>

    <form id='settingsForm'>
      <div class='card'>
        <h2>Temperature Control</h2>
        <label>Low Threshold (°C) - Fan starts ramping up</label>
        <input type='number' name='temp_low' value='%TEMP_LOW%' step='0.5' min='20' max='50'>

        <label>High Threshold (°C) - Fan at 100%</label>
        <input type='number' name='temp_high' value='%TEMP_HIGH%' step='0.5' min='30' max='80'>
      </div>

      <div class='card'>
        <h2>Fan Control</h2>
        <label>Minimum Fan Speed (%)</label>
        <input type='number' name='fan_min' value='%FAN_MIN%' min='0' max='100'>
      </div>

      <div class='card'>
        <h2>PSU Monitoring</h2>
        <label>Low Voltage Alert (V)</label>
        <input type='number' name='psu_low' value='%PSU_LOW%' step='0.1' min='18' max='24'>

        <label>High Voltage Alert (V)</label>
        <input type='number' name='psu_high' value='%PSU_HIGH%' step='0.1' min='24' max='30'>
      </div>

      <div class='card'>
        <h2>Temperature Graph</h2>
        <label>Graph Timespan (seconds)</label>
        <select name='graph_time'>
          <option value='60' %GRAPH_TIME_60%>1 minute</option>
          <option value='300' %GRAPH_TIME_300%>5 minutes</option>
          <option value='600' %GRAPH_TIME_600%>10 minutes</option>
          <option value='1800' %GRAPH_TIME_1800%>30 minutes</option>
          <option value='3600' %GRAPH_TIME_3600%>60 minutes</option>
        </select>

        <label>Update Interval (seconds)</label>
        <select name='graph_interval'>
          <option value='1' %GRAPH_INT_1%>1 second</option>
          <option value='5' %GRAPH_INT_5%>5 seconds</option>
          <option value='10' %GRAPH_INT_10%>10 seconds</option>
          <option value='30' %GRAPH_INT_30%>30 seconds</option>
          <option value='60' %GRAPH_INT_60%>60 seconds</option>
        </select>
      </div>

      <div class='card'>
        <h2>Display</h2>
        <label>Coordinate Decimal Places</label>
        <select name='coord_decimals'>
          <option value='2' %COORD_DEC_2%>2 decimals (0.00)</option>
          <option value='3' %COORD_DEC_3%>3 decimals (0.000)</option>
        </select>
      </div>

      <div class='success' id='success'>Settings saved successfully!</div>

      <button type='submit'>💾 Save Settings</button>
      <button type='button' class='back-btn' onclick='location.href="/"'>← Back</button>
    </form>
  </div>

  <script>
    document.getElementById('settingsForm').addEventListener('submit', function(e) {
      e.preventDefault();

      let formData = new FormData(this);

      fetch('/api/save', {
        method: 'POST',
        body: new URLSearchParams(formData)
      })
      .then(r => r.text())
      .then(msg => {
        document.getElementById('success').style.display = 'block';
        setTimeout(() => {
          document.getElementById('success').style.display = 'none';
        }, 3000);
      });
    });
  </script>
</body>
</html>
)rawliteral";


const char ADMIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Admin - FluidDash</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #1a1a1a; color: #fff; }
    .container { max-width: 600px; margin: 0 auto; }
    h1, h2 { color: #ff6600; }
    .card { background: #2a2a2a; padding: 20px; margin: 15px 0; border-radius: 8px; }
    .warning { background: #ff6600; color: #000; padding: 15px; border-radius: 5px;
               margin: 15px 0; font-weight: bold; }
    label { display: block; margin: 15px 0 5px; color: #aaa; }
    input { width: 100%; padding: 10px; background: #333; color: #fff;
            border: 1px solid #555; border-radius: 5px; box-sizing: border-box; }
    button { background: #ff6600; color: #fff; border: none; padding: 12px 24px;
             border-radius: 5px; cursor: pointer; font-size: 16px; margin: 10px 5px 0 0; }
    button:hover { background: #cc5200; }
    .back-btn { background: #666; }
    .back-btn:hover { background: #555; }
    .success { background: #00ff00; color: #000; padding: 10px; border-radius: 5px;
               margin: 10px 0; display: none; }
    .current-reading { color: #00bfff; font-size: 18px; margin: 5px 0; }
  </style>
</head>
<body>
  <div class='container'>
    <h1>🔧 Admin & Calibration</h1>

    <div class='warning'>
      ⚠️ Warning: These settings affect measurement accuracy.
      Only change if you have calibration equipment.
    </div>

    <div class='card'>
      <h2>Current Readings (Uncalibrated)</h2>
      <div id='readings'>Loading...</div>
    </div>

    <form id='adminForm'>
      <div class='card'>
        <h2>Temperature Calibration</h2>
        <p style='color:#aaa'>Enter offset to add/subtract from each sensor</p>

        <label>X-Axis Offset (°C)</label>
        <input type='number' name='cal_x' value='%CAL_X%' step='0.1'>

        <label>YL-Axis Offset (°C)</label>
        <input type='number' name='cal_yl' value='%CAL_YL%' step='0.1'>

        <label>YR-Axis Offset (°C)</label>
        <input type='number' name='cal_yr' value='%CAL_YR%' step='0.1'>

        <label>Z-Axis Offset (°C)</label>
        <input type='number' name='cal_z' value='%CAL_Z%' step='0.1'>
      </div>

      <div class='card'>
        <h2>PSU Voltage Calibration</h2>
        <p style='color:#aaa'>Voltage divider multiplier (measure with multimeter)</p>

        <label>Calibration Factor</label>
        <input type='number' name='psu_cal' value='%PSU_CAL%' step='0.01' min='5' max='10'>
      </div>

      <div class='success' id='success'>Calibration saved successfully!</div>

      <button type='submit'>💾 Save Calibration</button>
      <button type='button' class='back-btn' onclick='location.href="/"'>← Back</button>
    </form>

    <div class='card' style='margin-top:30px;border-top:3px solid #ff6600'>
      <h2>🕒 Real-Time Clock (RTC)</h2>
      <p style='color:#aaa'>Set the current date and time for the DS3231 RTC module</p>

      <div id='currentTime' class='current-reading' style='margin-bottom:20px'>Loading...</div>

      <form id='rtcForm'>
        <label>Date (YYYY-MM-DD)</label>
        <input type='date' id='rtcDate' required>

        <label>Time (HH:MM:SS)</label>
        <input type='time' id='rtcTime' step='1' required>

        <div class='success' id='rtcSuccess'>RTC time set successfully!</div>

        <button type='submit'>⏰ Set RTC Time</button>
        <button type='button' onclick='setRTCNow()'>📅 Use Browser Time</button>
      </form>
    </div>
  </div>

  <script>
    function updateReadings() {
      fetch('/api/status')
        .then(r => r.json())
        .then(data => {
          let html = '';
          ['X', 'YL', 'YR', 'Z'].forEach((name, i) => {
            html += `<div class='current-reading'>${name}: ${data.temperatures[i].toFixed(2)}°C</div>`;
          });
          html += `<div class='current-reading'>PSU: ${data.psu_voltage.toFixed(2)}V</div>`;
          document.getElementById('readings').innerHTML = html;
        });
    }

    document.getElementById('adminForm').addEventListener('submit', function(e) {
      e.preventDefault();

      let formData = new FormData(this);

      fetch('/api/admin/save', {
        method: 'POST',
        body: new URLSearchParams(formData)
      })
      .then(r => r.text())
      .then(msg => {
        document.getElementById('success').style.display = 'block';
        setTimeout(() => {
          document.getElementById('success').style.display = 'none';
        }, 3000);
      });
    });

    function updateRTCTime() {
      fetch('/api/rtc')
        .then(r => r.json())
        .then(data => {
          if (data.timestamp) {
            document.getElementById('currentTime').innerHTML =
              `Current RTC Time: ${data.timestamp}`;
          }
        });
    }

    function setRTCNow() {
      const now = new Date();
      const date = now.toISOString().split('T')[0];
      const time = now.toTimeString().split(' ')[0];
      document.getElementById('rtcDate').value = date;
      document.getElementById('rtcTime').value = time;
    }

    document.getElementById('rtcForm').addEventListener('submit', function(e) {
      e.preventDefault();

      const date = document.getElementById('rtcDate').value;
      const time = document.getElementById('rtcTime').value;

      const formData = new URLSearchParams();
      formData.append('date', date);
      formData.append('time', time);

      fetch('/api/rtc/set', {
        method: 'POST',
        body: formData
      })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          document.getElementById('rtcSuccess').style.display = 'block';
          setTimeout(() => {
            document.getElementById('rtcSuccess').style.display = 'none';
          }, 3000);
          updateRTCTime();
        }
      });
    });

    updateReadings();
    updateRTCTime();
    setInterval(updateReadings, 2000);
    setInterval(updateRTCTime, 5000);
  </script>
</body>
</html>
)rawliteral";


const char WIFI_CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>WiFi Setup - FluidDash</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #1a1a1a; color: #fff; }
    .container { max-width: 600px; margin: 0 auto; }
    h1 { color: #00bfff; }
    h2 { color: #00ff00; }
    .card { background: #2a2a2a; padding: 20px; margin: 15px 0; border-radius: 8px; }
    .status { padding: 15px; border-radius: 5px; margin: 15px 0; font-weight: bold; }
    .status-connected { background: #00ff00; color: #000; }
    .status-ap { background: #ff9900; color: #000; }
    .status-disconnected { background: #ff0000; color: #fff; }
    label { display: block; margin: 15px 0 5px; color: #aaa; }
    input, select { width: 100%; padding: 10px; background: #333; color: #fff;
            border: 1px solid #555; border-radius: 5px; box-sizing: border-box;
            font-size: 16px; }
    button { background: #00bfff; color: #fff; border: none; padding: 12px 24px;
             border-radius: 5px; cursor: pointer; font-size: 16px; margin: 10px 5px 0 0; }
    button:hover { background: #0099cc; }
    .back-btn { background: #666; }
    .back-btn:hover { background: #555; }
    .message { padding: 10px; border-radius: 5px; margin: 10px 0; display: none; }
    .success { background: #00ff00; color: #000; }
    .error { background: #ff0000; color: #fff; }
    #password { -webkit-text-security: disc; }
    .info-box { background: #1a3a5a; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #00bfff; }
  </style>
</head>
<body>
  <div class='container'>
    <h1>📡 WiFi Configuration</h1>

%WIFI_STATUS%

    <div class='info-box'>
      <strong>ℹ️ Manual WiFi Configuration</strong><br>
      Enter your WiFi network name (SSID) and password below. The device will restart and attempt to connect.
    </div>

    <form id='wifiForm'>
      <div class='card'>
        <h2>WiFi Credentials</h2>

        <label>Network Name (SSID)</label>
        <input type='text' id='ssid' name='ssid' value='%CURRENT_SSID%' required
               placeholder='Enter WiFi network name'>

        <label>Password</label>
        <input type='password' id='password' name='password' required
               placeholder='Enter WiFi password'>

        <div class='message' id='message'></div>

        <button type='submit'>💾 Save & Connect</button>
        <button type='button' class='back-btn' onclick='location.href="/"'>← Back</button>
      </div>
    </form>
  </div>

  <script>

    document.getElementById('wifiForm').addEventListener('submit', function(e) {
      e.preventDefault();

      let ssid = document.getElementById('ssid').value;
      let password = document.getElementById('password').value;
      let msgDiv = document.getElementById('message');

      msgDiv.style.display = 'block';
      msgDiv.className = 'message';
      msgDiv.textContent = 'Connecting to ' + ssid + '...';

      let formData = new FormData();
      formData.append('ssid', ssid);
      formData.append('password', password);

      fetch('/api/wifi/connect', {
        method: 'POST',
        body: new URLSearchParams(formData)
      })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          msgDiv.className = 'message success';
          msgDiv.innerHTML = '✅ Connected successfully!<br>Device will restart in 3 seconds...';
          setTimeout(() => {
            window.location.href = '/';
          }, 3000);
        } else {
          msgDiv.className = 'message error';
          msgDiv.textContent = '❌ Connection failed: ' + data.message;
        }
      })
      .catch(err => {
        msgDiv.className = 'message error';
        msgDiv.textContent = '❌ Request failed. Check connection.';
      });
    });
  </script>
</body>
</html>
)rawliteral";


#endif // HTML_TEMPLATES_H