#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <FS.h>

// ==== WiFi Config ====
const char* ssid = "Myrr";
const char* password = "alanbiju01";

// ==== Telegram Config ====
#define BOT_TOKEN ""  // Enter your bot token here
String chat_id = ""; // Your chat ID (get it from Telegram)
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// ==== LCD Config ====
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ==== Energy Meter Variables ====
float voltage = 0;
float current = 0;
float power = 0;
float energy = 0;
float pf = 0.95;
bool sendReadings = false;
bool simulateReadings = false;
unsigned long deviceStartTime = 0;

// ==== Thresholds ====
float VOLTAGE_HIGH = 240.0;
float VOLTAGE_LOW = 180.0;
float CURRENT_HIGH = 10.0;

// ==== Simulation ====
struct SimReading { float voltage; float current; };
SimReading simSequence[] = {
  {170.0, 1.0},
  {190.0, 2.0},
  {245.0, 5.0},
  {220.0, 11.0},
  {200.0, 0.5}
};
const int simLength = sizeof(simSequence)/sizeof(simSequence[0]);
int simIndex = 0;
unsigned long lastSimStep = 0;
unsigned long SIM_STEP_INTERVAL = 5000; // default 5s per step

// ==== Timing ====
unsigned long lastSendTelegram = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastLoopTime = 0;
const unsigned long TELEGRAM_INTERVAL = 10000; // 10s
const unsigned long LCD_INTERVAL = 2000;       // 2s

// ==== Alert Cooldown ====
unsigned long lastVoltageAlert = 0;
unsigned long lastCurrentAlert = 0;
const unsigned long ALERT_COOLDOWN = 30000; // 30s

// ==== Sensor Pins & Calibration ====
#define VOLTAGE_PIN A0
#define CURRENT_PIN A0
#define ACS_OFFSET 512
#define ACS_SENSITIVITY 66.0
#define ZMPT_CALIBRATION 234.0

// NOTE: ESP8266 has only one analog input (A0). To measure voltage and current simultaneously
// you need external hardware (multiplexer) or time-multiplex the sensors. For simulation this is fine.

// ==== D1 / D2 Output Pins (NodeMCU) ====
// On NodeMCU: D1 is GPIO5, D2 is GPIO4. The Arduino environment usually defines D1/D2 constants.
const uint8_t PIN_D1 = D1; // used for voltage alert (LED/relay)
const uint8_t PIN_D2 = D2; // used for current alert (LED/relay)

// Manual/auto control flags
bool d1_auto = true;
bool d2_auto = true;
bool d1_state = LOW;
bool d2_state = LOW;

// ==== Daily & Weekly Energy ====
#define HOURS_IN_DAY 24
#define DAYS_IN_WEEK 7
float dailyEnergy[HOURS_IN_DAY] = {0};
float weeklyEnergy[DAYS_IN_WEEK] = {0};
int currentHour = 0;
int currentDay = 0;
unsigned long lastHourUpdate = 0;
unsigned long HOUR_INTERVAL = 3600000; // 1 hour

// ==== Helper Functions ====
void updateEnergyHistory(){
  unsigned long now = millis();
  if(now - lastHourUpdate >= HOUR_INTERVAL){
    lastHourUpdate = now;
    dailyEnergy[currentHour] = energy;
    currentHour = (currentHour + 1) % HOURS_IN_DAY;
    if(currentHour==0){
      weeklyEnergy[currentDay] = 0;
      for(int i=0;i<HOURS_IN_DAY;i++) weeklyEnergy[currentDay]+=dailyEnergy[i];
      currentDay = (currentDay + 1) % DAYS_IN_WEEK;
    }
  }
}

void sendASCIIChart(float* array,int length,String title){
  String msg=title+"\n";
  for(int i=0;i<length;i++){
    int bars = map((int)round(array[i]*10),0,100,0,10); // map with some sensitivity (kWh *10)
    msg += String(i)+": ";
    for(int b=0;b<bars;b++) msg += "|";
    msg += "\n";
  }
  bot.sendMessage(chat_id,msg,"");
}

void setD1(bool on){
  d1_state = on ? HIGH : LOW;
  digitalWrite(PIN_D1, d1_state);
}
void setD2(bool on){
  d2_state = on ? HIGH : LOW;
  digitalWrite(PIN_D2, d2_state);
}

// readValues handles both simulated and real reads
void readValues(){
  unsigned long currentMillis = millis();
  if(simulateReadings){
    if(currentMillis - lastSimStep > SIM_STEP_INTERVAL){
      lastSimStep = currentMillis;
      voltage = simSequence[simIndex].voltage;
      current = simSequence[simIndex].current;
      pf = random(60,100)/100.0;
      simIndex = (simIndex+1) % simLength;
    }
  } else if(sendReadings){
    // Simple single-A0 sampling - note practical limitations above
    int currentRaw = analogRead(CURRENT_PIN);
    float currentVoltage = (currentRaw - ACS_OFFSET)*(5.0/1023.0)*1000;
    current = currentVoltage/ACS_SENSITIVITY;
    int voltageRaw = analogRead(VOLTAGE_PIN);
    voltage = (voltageRaw*(5.0/1023.0))*ZMPT_CALIBRATION;
    // If using real sensors: scale/calibrate properly
  }
  power = voltage*current*pf;
  updateEnergyHistory();
}

void handleNewMessages(int numNewMessages){
  for(int i=0;i<numNewMessages;i++){
    String text=bot.messages[i].text;
    String from_id=bot.messages[i].chat_id;
    if(from_id!=chat_id) continue;

    // ---- Basic Commands ----
    if(text=="/start"){ sendReadings=true; simulateReadings=false; bot.sendMessage(chat_id,"✅ Energy meter readings started.",""); }
    else if(text=="/stop"){ sendReadings=false; bot.sendMessage(chat_id,"🛑 Energy meter readings stopped.",""); }
    else if(text=="/simulate"){ simulateReadings=true; sendReadings=false; simIndex=0; lastSimStep=0; SIM_STEP_INTERVAL=5000; bot.sendMessage(chat_id,"⚡ Smart simulation mode started.",""); }
    else if(text=="/stop_simulate"){ simulateReadings=false; bot.sendMessage(chat_id,"🛑 Simulation mode stopped.",""); }
    else if(text=="/sim_fast"){ SIM_STEP_INTERVAL=2000; bot.sendMessage(chat_id,"⚡ Fast simulation started (2s per step).",""); }
    else if(text=="/sim_slow"){ SIM_STEP_INTERVAL=5000; bot.sendMessage(chat_id,"⚡ Slow simulation started (5s per step).",""); }
    else if(text=="/reset_energy"){ energy=0; bot.sendMessage(chat_id,"🔄 Energy counter reset to 0 kWh.",""); }

    // ---- Status ----
    else if(text=="/status"){
      String msg="📊 Current Readings\n";
      msg+="V: "+String(voltage,1)+" V\nI: "+String(current,2)+" A\nP: "+String(power,1)+" W\nE: "+String(energy,3)+" kWh\nPF: "+String(pf,2);
      msg += "\n\nD1 (voltage alert) auto:"+String(d1_auto? "ON":"OFF") + " state:" + String(d1_state? "ON":"OFF");
      msg += "\nD2 (current alert) auto:"+String(d2_auto? "ON":"OFF") + " state:" + String(d2_state? "ON":"OFF");
      bot.sendMessage(chat_id,msg,"Markdown");
    }

    // ---- Charts ----
    else if(text=="/daily_chart"){ sendASCIIChart(dailyEnergy,HOURS_IN_DAY,"📈 Daily kWh"); }
    else if(text=="/weekly_chart"){ sendASCIIChart(weeklyEnergy,DAYS_IN_WEEK,"📈 Weekly kWh"); }

    // ---- Uptime ----
    else if(text=="/uptime"){
      unsigned long seconds = (millis() - deviceStartTime)/1000;
      unsigned long h = seconds/3600;
      unsigned long m = (seconds%3600)/60;
      unsigned long s = seconds%60;
      bot.sendMessage(chat_id,"⏱ Uptime: "+String(h)+"h "+String(m)+"m "+String(s)+"s","");
    }

    // ---- Set Alerts Dynamically ----
    else if(text.startsWith("/set_high_voltage")){
      int val = text.substring(17).toInt();
      if(val>0){ VOLTAGE_HIGH=val; bot.sendMessage(chat_id,"⚡ High voltage alert set to "+String(VOLTAGE_HIGH)+" V",""); }
    }
    else if(text.startsWith("/set_low_voltage")){
      int val = text.substring(16).toInt();
      if(val>0){ VOLTAGE_LOW=val; bot.sendMessage(chat_id,"⚡ Low voltage alert set to "+String(VOLTAGE_LOW)+" V",""); }
    }
    else if(text.startsWith("/set_high_current")){
      int val = text.substring(17).toInt();
      if(val>0){ CURRENT_HIGH=val; bot.sendMessage(chat_id,"⚡ High current alert set to "+String(CURRENT_HIGH)+" A",""); }
    }

    // ---- D1 / D2 Commands (manual & auto) ----
    else if(text=="/d1_on"){ d1_auto=false; setD1(true); bot.sendMessage(chat_id,"D1 turned ON (manual). Auto disabled.",""); }
    else if(text=="/d1_off"){ d1_auto=false; setD1(false); bot.sendMessage(chat_id,"D1 turned OFF (manual). Auto disabled.",""); }
    else if(text=="/d1_auto_on"){ d1_auto=true; bot.sendMessage(chat_id,"D1 auto-control ENABLED (voltage alerts will set D1).",""); }
    else if(text=="/d1_auto_off"){ d1_auto=false; bot.sendMessage(chat_id,"D1 auto-control DISABLED.",""); }

    else if(text=="/d2_on"){ d2_auto=false; setD2(true); bot.sendMessage(chat_id,"D2 turned ON (manual). Auto disabled.",""); }
    else if(text=="/d2_off"){ d2_auto=false; setD2(false); bot.sendMessage(chat_id,"D2 turned OFF (manual). Auto disabled.",""); }
    else if(text=="/d2_auto_on"){ d2_auto=true; bot.sendMessage(chat_id,"D2 auto-control ENABLED (current alerts will set D2).",""); }
    else if(text=="/d2_auto_off"){ d2_auto=false; bot.sendMessage(chat_id,"D2 auto-control DISABLED.",""); }

    // ---- Help ----
    else if(text=="/help"){
      String helpMsg = "/start - Start real readings\n";
      helpMsg += "/stop - Stop readings\n";
      helpMsg += "/status - Current readings\n";
      helpMsg += "/simulate - Start smart simulation mode\n";
      helpMsg += "/stop_simulate - Stop simulated readings\n";
      helpMsg += "/reset_energy - Reset kWh counter\n";
      helpMsg += "/uptime - Show device uptime\n";
      helpMsg += "/sim_fast - Fast simulation (2s per step)\n";
      helpMsg += "/sim_slow - Slow simulation (5s per step)\n";
      helpMsg += "/set_high_voltage <value> - Set high voltage alert\n";
      helpMsg += "/set_low_voltage <value> - Set low voltage alert\n";
      helpMsg += "/set_high_current <value> - Set high current alert\n";
      helpMsg += "/daily_chart - Show daily energy chart\n";
      helpMsg += "/weekly_chart - Show weekly energy chart\n";
      helpMsg += "/d1_on / d1_off - Manually turn D1 on/off (disables auto)\n";
      helpMsg += "/d1_auto_on / d1_auto_off - Enable/Disable automatic control of D1\n";
      helpMsg += "/d2_on / d2_off - Manually turn D2 on/off (disables auto)\n";
      helpMsg += "/d2_auto_on / d2_auto_off - Enable/Disable automatic control of D2\n";
      helpMsg += "/help - Show this message";
      bot.sendMessage(chat_id, helpMsg,"");
    }
  }
}

void setup(){
  Serial.begin(9600);
  lcd.init(); lcd.backlight();
  lcd.setCursor(0,0); lcd.print("   Energy Meter   ");
  lcd.setCursor(0,1); lcd.print("  Monitor Local   ");
  lcd.setCursor(0,2); lcd.print(" Sending to TG Bot");
  delay(2000); lcd.clear();

  pinMode(PIN_D1, OUTPUT);
  pinMode(PIN_D2, OUTPUT);
  setD1(false);
  setD2(false);

  WiFi.begin(ssid,password);
  lcd.print("Connecting WiFi...");
  int attempts=0;
  while(WiFi.status()!=WL_CONNECTED && attempts<20){ delay(500); Serial.print("."); attempts++; }
  lcd.clear();
  if(WiFi.status()==WL_CONNECTED){ lcd.print("WiFi Connected!"); Serial.println("WiFi Connected!"); }
  else { lcd.print("WiFi Failed!"); delay(3000); ESP.restart(); }

  secured_client.setInsecure();
  lastLoopTime=millis();
  deviceStartTime = millis();
}

void loop(){
  unsigned long currentMillis=millis();
  int numNewMessages=bot.getUpdates(bot.last_message_received+1);
  if(numNewMessages) handleNewMessages(numNewMessages);

  bool active=sendReadings||simulateReadings;
  if(active){
    float deltaTimeHours=(currentMillis-lastLoopTime)/3600000.0;
    lastLoopTime=currentMillis;
    readValues();
    energy+=power*deltaTimeHours;

    // ---- LCD Update ----
    if(currentMillis-lastLCDUpdate>LCD_INTERVAL){
      lastLCDUpdate=currentMillis;
      lcd.setCursor(0,0); lcd.print("V: "); lcd.print(voltage,1); lcd.print("V   ");
      lcd.setCursor(11,0); lcd.print("PF:"); lcd.print(pf,2); lcd.print(" ");
      lcd.setCursor(0,1); lcd.print("I: "); lcd.print(current,2); lcd.print("A   ");
      lcd.setCursor(11,1); lcd.print("P: "); lcd.print(power,1); lcd.print("W ");
      lcd.setCursor(0,2); lcd.print("E: "); lcd.print(energy,3); lcd.print("kWh ");
    }

    // ---- Telegram Update ----
    if(currentMillis-lastSendTelegram>TELEGRAM_INTERVAL){
      lastSendTelegram=currentMillis;
      String message="📊 Energy Meter Readings\n";
      message+="🔌 Voltage: "+String(voltage,1)+" V\n⚡ Current: "+String(current,2)+" A\n💡 Power: "+String(power,1)+" W\n🔋 Energy: "+String(energy,3)+" kWh\n📐 PF: "+String(pf,2);
      bot.sendMessage(chat_id,message,"Markdown");
    }

    // ---- Alerts & D1/D2 Auto-Control ----
    bool voltageAlert = (voltage>VOLTAGE_HIGH || voltage<VOLTAGE_LOW);
    bool currentAlert = (current>CURRENT_HIGH);

    if(voltageAlert && (currentMillis-lastVoltageAlert>ALERT_COOLDOWN)){
      String msg = String(voltage>VOLTAGE_HIGH ? "⚠ High Voltage" : "⚠ Low Voltage") + " (" + String(voltage,1) + " V)";
      bot.sendMessage(chat_id,msg,""); lastVoltageAlert=currentMillis;
    }
    if(currentAlert && (currentMillis-lastCurrentAlert>ALERT_COOLDOWN)){
      bot.sendMessage(chat_id,"⚠ High Current! ("+String(current,2)+" A)",""); lastCurrentAlert=currentMillis;
    }

    // Auto control behavior (respect manual override if auto=false)
    if(d1_auto){
      if(voltageAlert) setD1(true);
      else setD1(false);
    }
    if(d2_auto){
      if(currentAlert) setD2(true);
      else setD2(false);
    }
  }
}
