# Smart Energy Meter with NodeMCU, LCD, and Telegram Bot

This project implements a smart energy meter using a NodeMCU (ESP8266), an I2C LCD display, and a Telegram bot for remote monitoring and control. The system can operate in both real and simulated modes, providing live energy readings, alerts, and historical charts via Telegram commands.

## Features
- **WiFi-enabled**: Connects to your WiFi network for remote access.
- **LCD Display**: Shows real-time voltage, current, power, energy, and power factor.
- **Telegram Bot Integration**: Receive readings, alerts, and control the device remotely.
- **Simulation Mode**: Test the system without real sensors using predefined data.
- **Alerts**: Notifies you of high/low voltage and high current via Telegram.
- **Manual & Auto Control**: Remotely control output pins (D1/D2) for relays/LEDs.
- **Energy History**: Tracks and displays daily and weekly energy usage as ASCII charts.

## Hardware Requirements
- NodeMCU (ESP8266)
- I2C LCD Display (20x4 recommended)
- Voltage Sensor (e.g., ZMPT101B)
- Current Sensor (e.g., ACS712)
- Relays/LEDs (optional, for D1/D2 alerts)
- Internet-connected WiFi network

## Libraries Used
- `Wire.h`
- `LiquidCrystal_I2C.h`
- `ESP8266WiFi.h`
- `WiFiClientSecure.h`
- `UniversalTelegramBot.h`
- `FS.h`

Install these libraries via the Arduino Library Manager.

## Setup Instructions
1. **Clone or Download** this repository.
2. **Open** `NodeMCU_energy_meter_IOT.ino` in the Arduino IDE.
3. **Configure WiFi**:
   - Set your WiFi SSID and password in the `ssid` and `password` variables.
4. **Configure Telegram Bot**:
   - Create a Telegram bot with [@BotFather](https://t.me/BotFather).
   - Set your bot token in `#define BOT_TOKEN ""`.
   - Set your chat ID in `String chat_id = "";` (get it from [@userinfobot](https://t.me/userinfobot)).
5. **Connect Hardware** as per your sensors and LCD.
6. **Upload** the code to your NodeMCU.
7. **Open Telegram** and start chatting with your bot.

## Telegram Commands
- `/start` - Start real readings
- `/stop` - Stop readings
- `/simulate` - Start simulation mode
- `/stop_simulate` - Stop simulation
- `/sim_fast` - Fast simulation (2s per step)
- `/sim_slow` - Slow simulation (5s per step)
- `/reset_energy` - Reset energy counter
- `/status` - Show current readings
- `/daily_chart` - Show daily energy chart
- `/weekly_chart` - Show weekly energy chart
- `/uptime` - Show device uptime
- `/set_high_voltage <value>` - Set high voltage alert
- `/set_low_voltage <value>` - Set low voltage alert
- `/set_high_current <value>` - Set high current alert
- `/d1_on` / `/d1_off` - Manually control D1 (voltage alert output)
- `/d1_auto_on` / `/d1_auto_off` - Enable/disable D1 auto control
- `/d2_on` / `/d2_off` - Manually control D2 (current alert output)
- `/d2_auto_on` / `/d2_auto_off` - Enable/disable D2 auto control
- `/help` - Show all commands

## Notes
- The code supports both real sensor readings and simulation for testing.
- D1 and D2 pins can be used to control relays or LEDs for alerting.
- All settings and thresholds can be adjusted via Telegram commands.

## License
This project is licensed under the MIT License. See the `LICENSE` file for details.
