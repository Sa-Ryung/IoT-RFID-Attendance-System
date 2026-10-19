IoT-Based Smart RFID Attendance and Security System
An IoT-based project that combines RFID attendance tracking and PIR motion detection using the NodeMCU ESP8266. Attendance records can be synchronized with Google Sheets, while local alerts help identify unauthorized access or movement.
Features
- RFID-based user authentication
- Student attendance tracking with IN/OUT status
- Google Sheets cloud logging over Wi-Fi
- PIR-based motion and intrusion alerts
- LCD, buzzer, and LED status indicators
- RFID card management: Read, Write, Update, and Delete
Hardware
- NodeMCU ESP8266
- MFRC522 RFID reader and RFID cards
- PIR motion sensor
- 16×2 I2C LCD
- Buzzer and LEDs
- Breadboard, jumper wires, and USB power
Technologies
- Arduino IDE / Embedded C++
- ESP8266 Wi-Fi
- SPI and I2C
- Google Apps Script
- Google Sheets
- Proteus 8 (for simulation)
How It Works
1. Scan an RFID card.
2. The system checks whether the card is authorized.
3. The LCD, buzzer, and LEDs show the result.
4. Attendance details are sent to Google Sheets through Google Apps Script.
5. The PIR sensor detects movement for intrusion monitoring.
6. Administrators can manage RFID card data using serial commands.
Setup
1. Assemble the hardware using the circuit diagram in the repository.
2. Install ESP8266 board support and the required Arduino libraries.
3. Configure your Wi-Fi credentials and Google Apps Script endpoint in the firmware.
4. Upload the firmware to the NodeMCU ESP8266.
5. Test RFID authentication, attendance logging, and PIR alerts.
Important: Keep Wi-Fi passwords and private deployment credentials out of public repositories. The MFRC522 reader must be powered at 3.3 V.
