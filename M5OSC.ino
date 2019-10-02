/*   
 *   Created for the M5StickC
 *   This sketch sends rotation and other sensor data 
 *   using UDP packets to an OSC receiver 
 *    
 *    Copyright 2019 Seth Persigehl 
 *    Website: http://persigehl.com/
 *    Twitter: @KK4TEE 
 *    Email: seth.persigehl@gmail.com
 *    MIT License - This copyright notice must be included.
 *    
 *    Please see the referenced libraries for their own licences.
 *    This follows the M5 tutorials and references. Please refer to
 *    them for more information.
 *    https://m5stack.com/products/stick-c
 */
 
 
#include <M5StickC.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>


////////// User Variables //////////////////////////////////////////////////
// Be sure to set these for your own WiFi network
const char * networkName = "your-ssid";
const char * networkPswd = "your-password";

int sensorIn = G36;
int nonTurboRefreshDelay = 100;

//IP address to send UDP data to:
// either use the ip address of the server or 
// a network broadcast address
const char * udpAddress = "10.42.42.119";
const int udpPort = 8001;

//Are we currently connected?
boolean connected = false;

//The udp library class
WiFiUDP udp;

////////// System Variables //////////////////////////////////////////////////
enum imu_type {
  unknown,
  SH200Q,
  MPU6886
};

imu_type imuType = unknown;
int16_t accX, accY, accZ;
int16_t gyroX, gyroY, gyroZ;
double fXg, fYg, fZg;
const float alpha = 0.5;
double pitch, roll, Xg, Yg, Zg;
float gRes, aRes;

int analogSensorValue;
int16_t temp = 0;
double vbat = 0.0;
int discharge,charge;
double bat_p = 0.0;
bool isCharging;
bool isDischarging;

bool turboModeActive;
int screenBrightness = 15; // Range 0->15; Start at max brightness

///////////// WiFi Functions //////////////////////////////////////////////////
void connectToWiFi(const char * ssid, const char * pwd){
  USE_SERIAL.println("Connecting to WiFi network: " + String(ssid));

  // delete old config
  WiFi.disconnect(true);
  //register event handler
  WiFi.onEvent(WiFiEvent);
  
  //Initiate connection
  WiFi.begin(ssid, pwd);

  USE_SERIAL.println("Waiting for WIFI connection...");
}


//wifi event handler
void WiFiEvent(WiFiEvent_t event){
    switch(event) {
      case SYSTEM_EVENT_STA_GOT_IP:
          //When connected set 
          USE_SERIAL.print("WiFi connected! IP address: ");
          USE_SERIAL.println(WiFi.localIP());  
          //initializes the UDP state
          //This initializes the transfer buffer
          udp.begin(WiFi.localIP(),udpPort);
          connected = true;
          break;
      case SYSTEM_EVENT_STA_DISCONNECTED:
          USE_SERIAL.println("WiFi lost connection");
          connected = false;
          break;
    }
}


/////// General Functions //////////////////////////////////////////////////

void setup() {
  
  M5.begin();
  DecideGyroType();
  pinMode(M5_LED, OUTPUT);
  digitalWrite(M5_LED, HIGH);
  pinMode(M5_BUTTON_HOME, INPUT);
  pinMode(M5_BUTTON_RST, INPUT);

  M5.Axp.ScreenBreath(screenBrightness);
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 0);

  USE_SERIAL.begin(115200);
  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();
  
  connectToWiFi(networkName, networkPswd);
}


void DecideGyroType(){
  // The M5StickC can have different types of IMUs
  // We need to detect which is installed and use it
  // For this we will test the accellerometer to see if
  // it reports all zeros. Since we are (probably) not in
  // outer space, this should be a decent proxy.

  imuType = unknown;
  accX, accY, accZ = 0;
  USE_SERIAL.println();
  USE_SERIAL.println("Resetting IMU");
  
  // Test for the SH200Q using a call to M5.IMU
  //Wire1.end(); // Doesn't exist in the old version of Wire used
  //pinMode(21, OUTPUT);
  //pinMode(22, OUTPUT);
  M5.IMU.sh200i_Reset();
  M5.IMU.sh200i_ADCReset();
  M5.IMU.Init();
  M5.IMU.getAccelAdc(&accX,&accY,&accZ);
  USE_SERIAL.print("X: "); USE_SERIAL.print(accX); 
  USE_SERIAL.print(" Y: "); USE_SERIAL.print(accY); 
  USE_SERIAL.print(" Z: "); USE_SERIAL.print(accZ); 
  USE_SERIAL.println();
  if ( !(abs(accX) == 0 && abs(accY) == 0 && abs(accZ) == 0) && accX != 16380){
    // Because of the way the TwoWire library works, if the IMU is an MPU6886 the 
    // byte that represents accX will return 16380, indicating it's not an SH200Q
    imuType = SH200Q;
    return;
  }

  // Test for the MPU6886 using a call to M5.MPU6886
  
  M5.MPU6886.Init();
  M5.MPU6886.getAccelAdc(&accX,&accY,&accZ);
  if ( !(abs(accX) == 0 && abs(accY) == 0 && abs(accZ) == 0)){
    imuType = MPU6886;
    return;
  }
}


void HandleButtons(){
  // POWER BUTTON
  // 0x01 long press(1s), 0x02 press
  if(M5.Axp.GetBtnPress() == 0x01) {
    USE_SERIAL.print("Button press 0x01");
    USE_SERIAL.println(" - LONG");
      M5.Lcd.fillScreen(RED);
      // Run action
      M5.Lcd.fillScreen(ORANGE);
      delay(400);
      M5.Lcd.fillScreen(BLACK);
  }
  if(M5.Axp.GetBtnPress() == 0x02) {
    //esp_restart();
    USE_SERIAL.print("Button press 0x02");
    USE_SERIAL.println(" - LONG");
      M5.Lcd.fillScreen(RED);
      // Run action
      M5.Lcd.fillScreen(BLUE);
      delay(400);
      M5.Lcd.fillScreen(BLACK);
  }

  // TOP SIDE BUTTON
  if(digitalRead(M5_BUTTON_RST) == LOW){
    USE_SERIAL.print("Button press TOP SIDE BUTTON");
    M5.Lcd.fillScreen(WHITE);
    unsigned long pressTime  = millis();
    while(digitalRead(M5_BUTTON_RST) == LOW);
    if (millis() > pressTime + 500){
      USE_SERIAL.println(" - LONG");
      //M5.Lcd.fillScreen(RED);
      // Run action
      M5.Lcd.fillScreen(BLUE);
      M5.Lcd.setCursor(0, 0, 1);
      M5.Lcd.printf("Reset IMU");
      DecideGyroType();
      delay(400);
      M5.Lcd.fillScreen(BLACK);
    }
    else{
      USE_SERIAL.println(" - SHORT");
      M5.Lcd.fillScreen(RED);
      // Run action
      turboModeActive = !turboModeActive;
      M5.Lcd.fillScreen(ORANGE);
      M5.Lcd.setCursor(0, 0, 1);
      M5.Lcd.printf("Toggled Turbo Mode");
      delay(400);
      M5.Lcd.fillScreen(BLACK);
    }
  }

  // CENTER FACE BUTTON
  if(digitalRead(M5_BUTTON_HOME) == LOW){
    USE_SERIAL.print("Button press CENTER FACE BUTTON");
    M5.Lcd.fillScreen(WHITE);
    unsigned long pressTime  = millis();
    while(digitalRead(M5_BUTTON_HOME) == LOW);
    if (millis() > pressTime + 500){
      USE_SERIAL.println(" - LONG");
      M5.Lcd.fillScreen(RED);
      // Run action
      M5.Lcd.fillScreen(BLUE);
      delay(400);
      M5.Lcd.fillScreen(BLACK);
    }
    else{
      USE_SERIAL.println(" - SHORT");
      M5.Lcd.fillScreen(RED);
      // Run action
      // Set the display's brightness
      screenBrightness += 7;
      if (screenBrightness >= 16){
        screenBrightness = 0;
      }
      M5.Axp.ScreenBreath(screenBrightness);
      
      M5.Lcd.fillScreen(ORANGE);
      delay(400);
      M5.Lcd.fillScreen(BLACK);
    }
  }
}


void HandleSensors(){
  if (imuType == SH200Q){
    M5.IMU.getGyroAdc(&gyroX,&gyroY,&gyroZ);
    M5.IMU.getAccelAdc(&accX,&accY,&accZ);
    M5.IMU.getTempAdc(&temp);
    aRes = M5.IMU.aRes;
    gRes = M5.IMU.gRes;
  }
  else if (imuType == MPU6886){
    M5.MPU6886.getGyroAdc(&gyroX,&gyroY,&gyroZ);
    M5.MPU6886.getAccelAdc(&accX,&accY,&accZ);
    M5.MPU6886.getTempAdc(&temp);
    aRes = M5.MPU6886.aRes;
    gRes = M5.MPU6886.gRes;
  }
  
  vbat = M5.Axp.GetVbatData() * 1.1 / 1000;
  isCharging    = M5.Axp.GetIchargeData() / 2;
  isDischarging = M5.Axp.GetIdischargeData() / 2;
  analogSensorValue = analogRead(sensorIn);
  
  //Low Pass Filter
  fXg = accX * alpha + (fXg * (1.0 - alpha));
  fYg = accY * alpha + (fYg * (1.0 - alpha));
  fZg = accZ * alpha + (fZg * (1.0 - alpha));

  //Roll & Pitch Equations
  roll  = 11+ (atan2(-fYg, fZg)*180.0)/M_PI;
  pitch = (atan2(fXg, sqrt(fYg*fYg + fZg*fZg))*180.0)/M_PI - 8;
}


void HandleDisplay(){

  USE_SERIAL.print("Pitch: "); USE_SERIAL.print(pitch); USE_SERIAL.print(" Roll: "); USE_SERIAL.print(roll);
  
  M5.Lcd.setCursor(0, 0);
  if (connected){
    M5.Lcd.print("OSC Send: ");
  }
  else {
    M5.Lcd.print("WiFiError ");
  }
  M5.Lcd.setCursor(60, 0);
  M5.Lcd.print(udpAddress);
  
  M5.Lcd.setCursor(0, 10);
  M5.Lcd.println("  X       Y      Z");
  M5.Lcd.setCursor(114, 10);
  if (imuType == SH200Q){
    M5.Lcd.print(" SH200Q");
  }
  else if (imuType == MPU6886){
    M5.Lcd.print("MPU6886");
  }
  else {
    M5.Lcd.print("UNKNOWN");
  }
  
  M5.Lcd.setCursor(0, 20);
  M5.Lcd.printf("%.2f   %.2f   %.2f    ", ((float) gyroX) * gRes, ((float) gyroY) * gRes,((float) gyroZ) * gRes);
  M5.Lcd.setCursor(144, 20);
  M5.Lcd.print("mg");
  M5.Lcd.setCursor(0, 30);
  M5.Lcd.printf("%.2f   %.2f   %.2f     ",((float) accX) * aRes,((float) accY) * aRes, ((float) accZ) * aRes);
  M5.Lcd.setCursor(140, 30);
  M5.Lcd.print("*/s");
  M5.Lcd.setCursor(0, 40);
  M5.Lcd.print("Analog: ");
  M5.Lcd.print((int)analogSensorValue);
  M5.Lcd.print("   ");
  M5.Lcd.setCursor(75, 40);
  M5.Lcd.print(" / 4095");
  M5.Lcd.setCursor(0, 60);
  M5.Lcd.printf("vbat:%.3fV",vbat);
  if (isCharging){
    M5.Lcd.print(" Charging      ");
  }
  else if (isDischarging){
    M5.Lcd.print(" Discharging   ");
  }
  else if (!isDischarging && !isCharging){
    M5.Lcd.print(" External Power");
  }
  M5.Lcd.setCursor(0, 70);
  M5.Lcd.printf("Temperature : %.2f C",((float) temp) / 333.87 + 21.0);
}


void HandleNetwork(){
  // Only send data when connected
  if(connected){
    OSCMessage oscPitchMsg("/pitch"); // First argument is OSC address
    oscPitchMsg.add((float)pitch); // Then append the data
    udp.beginPacket(udpAddress,udpPort);
    oscPitchMsg.send(udp); // send the bytes to the SLIP stream
    udp.endPacket(); // mark the end of the OSC Packet
    oscPitchMsg.empty(); // free space occupied by message

    OSCMessage oscRollMsg("/roll"); // First argument is OSC address
    oscRollMsg.add((float)roll); // Then append the data
    udp.beginPacket(udpAddress,udpPort);
    oscRollMsg.send(udp); // send the bytes to the SLIP stream
    udp.endPacket(); // mark the end of the OSC Packet
    oscRollMsg.empty(); // free space occupied by message

    OSCMessage oscM5AnalogMsg("/m5Analog"); // First argument is OSC address
    oscM5AnalogMsg.add((int)analogSensorValue); // Then append the data
    udp.beginPacket(udpAddress,udpPort);
    oscM5AnalogMsg.send(udp); // send the bytes to the SLIP stream
    udp.endPacket(); // mark the end of the OSC Packet
    oscM5AnalogMsg.empty(); // free space occupied by message

    USE_SERIAL.print(" WiFi connected and OSC packets sent");
  }
  else {
    USE_SERIAL.print(" WiFi disconnected");
  }
  USE_SERIAL.println("");
}

/////// Main Loop //////////////////////////////////////////////////
void loop() {
  HandleButtons();
  HandleSensors();
  HandleDisplay();
  HandleNetwork();

  if (turboModeActive){
    // No delay
  }
  else{
    // Delay based on user preference
    delay(nonTurboRefreshDelay);
  }
}
