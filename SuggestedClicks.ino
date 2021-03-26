
 /*
Complete Auto Sag Tool Demo Code
*/
 
 // Include the necessary libraries
 #include <Wire.h> 
 #include <AltSoftSerial.h>
 #include <SoftwareSerial.h>
 #include <math.h>
 #include <ServoTimer2.h>
 
 // Create a Servo object
 ServoTimer2 myservo;
 
 // Enter the gear radius (mm)
 float gearRadius = 3.1831;

 // Intialize variables needed for Blexar Terminal Input
 String str = "";
 char ser_byte;
 float dataNumber; 
 
 float totalStroke;
 float desiredSag;
 float totalLength;
 float sagTime;
 float sagTimeBias;
 float saddleUpTime = 5000;
 float totalClicks;
 float bleedTime;
 float bleedTimeLimit = 500;
 float noBleedTimeLimit = 8000;
 float bleedBias;
 float blexTime;
 float blexBias;
 float connectTime = 10000;
 bool bleedState; // 0 means no bleed, 1 means bleed
 
 // Rotary Encoder Inputs
 int encoderPin = A0;
 int encoderReading;
 int blexDelay = 25;
 int blexDelaySag = 50;
 int steps;
 int printStep = 2;
 float angleBias;
 float bits2angle = 0.35156;
 float encoderAngleRaw = 0;
 float encoderAngleUnwrapped = 0;
 float oldEncoderAngleRaw = 0;
 float dAngle = 0;
 float turns = 0;
 float counter;
 float counterOld;
 float deg2length = -(2*3.14*gearRadius)/360; // Calculate Step in Length for each step of encoder
 float servoSignal = 1800;
 float servoHome = 1800;
 float servoBleed = 1660;
 
 //Variables for calculating zeta
 int reboundRun;
 int reboundSetting[3];
 int totalRuns = 2;
 float dZeta_dClick;
 float zeta[2];
 float maxTime = 15;
 float runTime;
 float biasTime;
 float posSettle;
 float posStep;
 float posOvershoot;
 float maxOS;
 float desiredZeta[2];
 float correctClicksMax;
 float correctClicksMin;
 int predClicks;
 
 // Setup Bluetooth as HM-10 TX/RX pins
 AltSoftSerial Serial1(8, 9); // RX, TX*// 
 SoftwareSerial Serial2(8,9); // RX, TX
 //SoftwareSerial Serial1
 void setup() { 

  // Setup high and low desired damping ratios
  desiredZeta[0] = 0.3;
  desiredZeta[1] = 0.7;
   
  // Initial reading of encoder, subtracted to allow us to start from 0
  angleBias = (analogRead(encoderPin)*bits2angle);

  // Begin Bluetooth connection
  Serial1.begin(9600);
   
  // Attach servo on pin 10 to the servo object
  myservo.attach(10);

  // Move servo to home position
  myservo.write(servoHome);
      
 } 

 void loop() {

  // Give user time to connect Blexar and navigate to terminal
  delay(connectTime);

  // Prompt user for input
  Serial1.println("Input Shock/Fork");
  Serial1.println("Stroke (mm):");
  // Input total stroke of shock and fork
  while (totalStroke == 0) {
    if (Serial1.available()){
      ser_byte = Serial1.read(); // read BLE data
      if (ser_byte=='\n'){ // wait for newline char to print
        totalStroke = str.toFloat();
        str=""; // clear string
      } else {
        str+=ser_byte; // append to string
      }
    }
  }
  
  //while (!Serial1){}; // wait for BLE to settle
  delay(2000);

  // Prompt user for input
  Serial1.println("Input Desired");
  Serial1.println("Sag (%):");
  
  // Allow Blexar terminal to clear before next input
  Serial1.end();    // Ends the serial communication once all data is received
  Serial1.begin(9600);
  
  // Input desired sag
  while (desiredSag == 0) {
    if (Serial1.available()){
      ser_byte = Serial1.read(); // read BLE data
      if (ser_byte=='\n'){ // wait for newline char to print
        desiredSag = str.toFloat();
        str=""; // clear string
      } else {
        str+=ser_byte; // append to string
      }
    }
  }

  // Calculate total length needed to travel to reach desired sag
  totalLength =(desiredSag/100)*totalStroke;
  
  // User instruction
  Serial1.println("Navigate to plotting");    // new for this version
  Serial1.println("tab, pump when bleed");    // new for this version
  Serial1.println("pauses");
  delay(3000);
  
  sagTimeBias = millis();
  bleedBias = millis();
  blexBias = millis();
  
  // Allow Blexar terminal to clear before next input
  Serial1.end();    // Ends the serial communication once all data is received
  Serial1.begin(9600);

  // Let out air while the shock/fork hasnt travelled far enough to reach desired sag
  while ((abs(counter) < totalLength) || (bleedState == 0)) {

    sagTime = millis() - sagTimeBias;
    blexTime = millis() - blexBias;
    
    // Read encoder
    encoderReading = analogRead(encoderPin);

    // Subtract bias to normalize around zero
    encoderAngleRaw = ((encoderReading*bits2angle) - angleBias);

    // Calculate change in angle
    dAngle = encoderAngleRaw - oldEncoderAngleRaw;

    // Decision structure that if we jumped really far unwrap angle
    if (abs(dAngle) > 180) { 
        if (dAngle > 0) {
          turns = turns - 1;
        }
        if (dAngle < 0) {
          turns = turns + 1;
        }
    }

    encoderAngleUnwrapped = ((360*turns) + encoderAngleRaw);

    // Update old value
    oldEncoderAngleRaw = encoderAngleRaw;

    // Convert angle to distance
    counter = encoderAngleUnwrapped*deg2length;
    
    // Start the bleed if the rider has had time to saddle the bike. Needed to normalize at the beginning of loop (Start from zero sag)
    if (sagTime > saddleUpTime) {

      // Find how long we have been in the state we are in
      bleedTime = millis() - bleedBias;
      
      // If we ARE bleeding and the time has surpassed the time to bleed --> stop bleeding
      if ((bleedState == 1) && (bleedTime > bleedTimeLimit)) {
        
        // Move the servo to stop bleed
        servoSignal = servoHome;

        // Change state to not bleeding
        bleedState = 0;

        //Reset  bias for next step
        bleedBias = millis();
        
      }
      
      // If we ARE NOT bleeding and the time has surpassed the time to wait, --> start bleeding
      if ((bleedState == 0) && (bleedTime > noBleedTimeLimit)) {
        
        // Move the servo to push bleed button
        servoSignal = servoBleed;

        // Change state to bleed
        bleedState = 1;

        //Reset  bias for next step
        bleedBias = millis();
        
      }
      myservo.write(servoSignal);
    
    }
    //delay(blexDelay);

    // Only plot every 40 milliseconds to not overwhelm blexar app
    if (blexTime > blexDelaySag) {
      Serial1.println(((counter/totalStroke)*100),2);
      blexBias = millis();
    }
  }

  servoSignal = servoHome;
  // Move the servo to stop bleed
  myservo.write(servoSignal);

  // Allow user to navigate back to terminal console
  delay(5000);
  
  // Print the actual sag reading for the user
  Serial1.println("Final Sag is: ");    // new for this version
  delay(1000);
  Serial1.println(((counter/totalStroke)*100),2);     // new for this version
  delay(5000);

  // Begin Rebound Setup
  Serial1.println("Now begin Rebound");
  Serial1.println("Setup");

  delay(1000);
  // Prompt user for input
  Serial1.println("Total Rebound Clicks:");    // new for this version

  // Allow Blexar terminal to clear before next input
  Serial1.end();    // Ends the serial communication once all data is received
  Serial1.begin(9600);
  
  // User inputs total Clicks
  while (totalClicks == 0) {
    if (Serial1.available()){
      ser_byte = Serial1.read(); // read BLE data
      if (ser_byte=='\n'){ // wait for newline char to print
        totalClicks = str.toFloat();
        str=""; // clear string
      } else {
        str+=ser_byte; // append to string
      }
    }
  }

  // Allow Blexar terminal to clear before next input
  Serial1.end();    // Ends the serial communication once all data is received
  Serial1.begin(9600);
  //while (!Seria1){}; // wait for BLE to settle
  
  while(reboundRun < totalRuns) {
    
    posSettle = 0;
    posStep = 0;
    posOvershoot = 0;
    runTime = 0;
    delay(3000);

    // For the first run we are doing the minimal damping
    if (reboundRun == 0) {
      reboundSetting[reboundRun] = 0;
    }

    // For the second run we are doing the maximum damping
    if (reboundRun == 1) {
      reboundSetting[reboundRun] = totalClicks;
    }
    
    // For the first run we are doing the minimal damping
    if (reboundRun == 0) {
      Serial1.println("Set Rebound to Zero");
      Serial1.println("Clicks (Rabbit)");
    }

    // For the second run we are doing the maximum damping
    if (reboundRun == 1) {
      Serial1.println("Set Rebound to Max");
      Serial1.println("Clicks (Turtle)");
    }
    delay(10000);

    encoderAngleRaw = 0;
    encoderAngleUnwrapped = 0;
    oldEncoderAngleRaw = 0;
    dAngle = 0;
    turns = 0;
    counter=0;
    counterOld=0;
    blexBias = millis();
    blexTime = 0;
    angleBias = (analogRead(encoderPin)*bits2angle);
    
    Serial1.println("Navigate to plotting");
    Serial1.println("tab");

    // Allow Blexar terminal to clear before next input
    Serial1.end();    // Ends the serial communication once all data is received
    Serial2.begin(9600);
    
    delay(5000);

    biasTime = millis()/1000;
    // Resample angle bias and time bias for rebound step input
    
    while (runTime < maxTime) {

        runTime = (millis()/1000) - biasTime;
        encoderReading = analogRead(encoderPin);
        encoderAngleRaw = ((encoderReading*bits2angle) - angleBias);
        dAngle = encoderAngleRaw - oldEncoderAngleRaw;
        blexTime = millis() - blexBias;
        
        if (abs(dAngle) > 180) { 
            if (dAngle > 0) {
              turns = turns - 1;
            }
            if (dAngle < 0) { 
              turns = turns + 1;
            }
        }
      
        encoderAngleUnwrapped = ((360*turns) + encoderAngleRaw);
        oldEncoderAngleRaw = encoderAngleRaw;
    
        counter = (encoderAngleUnwrapped*deg2length);
        counterOld = counter;
        
        // Only plot every 40 milliseconds to not overwhelm blexar app
        if (blexTime > blexDelay) {
          Serial2.println(((counter/totalStroke)*100),2);
          blexBias = millis();
        }
        //Serial2.println((counter/totalStroke)*100);
        //delay(blexDelay);
        }

    // Allow Blexar terminal to clear before next input
    //Serial2.end();
    // Ends the serial communication once all data is received
    //Serial2.begin(9600);
    
    // Prompt user for starting position of step input
    Serial2.println("Input Max Compression:");

    // Allow Blexar terminal to clear before next input
    Serial2.end();
    // Ends the serial communication once all data is received
    Serial2.begin(9600);
  
    // Wait for step input data from user
    while (posStep == 0) {
      if (Serial2.available()){
        ser_byte = Serial2.read(); // read BLE data
        if (ser_byte=='\n'){ // wait for newline char to print
          posStep = str.toFloat();
          str=""; // clear string
        } else {
          str+=ser_byte; // append to string
        }
      }
    }

    // Prompt user for maximum position from step input
    Serial2.println("Input Rebound Overshoot Position:");

    // Allow Blexar terminal to clear before next input
    Serial2.end();    // Ends the serial communication once all data is received
    Serial2.begin(9600);
  
    // Wait for step input data from user
    while (posOvershoot == 0) {
      if (Serial2.available()){
        ser_byte = Serial2.read(); // read BLE data
        if (ser_byte=='\n'){ // wait for newline char to print
          posOvershoot = str.toFloat();
          str=""; // clear string
        } else {
          str+=ser_byte; // append to string
        }
      }
    }

        // Prompt user for starting position of step input
    Serial2.println("Input Settling Point:");

    // Allow Blexar terminal to clear before next input
    Serial2.end();
    // Ends the serial communication once all data is received
    Serial2.begin(9600);
  
    // Wait for step input data from user
    while (posSettle == 0) {
      if (Serial2.available()){
        ser_byte = Serial2.read(); // read BLE data
        if (ser_byte=='\n'){ // wait for newline char to print
          posSettle = str.toFloat();
          str=""; // clear string
        } else {
          str+=ser_byte; // append to string
        }
      }
    }
    
    delay(2000);
    
    maxOS = ((-posSettle - -posOvershoot)/(-posStep - (-posSettle))*100); // Calculate Percent Max overshooot
    zeta[reboundRun] = (-log(maxOS/100))/(pow(((pow(3.14, 2)) + (pow(log(maxOS/100), 2))), .5)); // Calculate Zeta Damping Ratio
    if (maxOS <= 0 && reboundRun == 1){
      totalClicks = totalClicks - 2;
      Serial2.println("Max Clicks is now: ");
      Serial2.println(totalClicks);
      continue;
      } 
    Serial2.println("Max Overshoot (%): ");
    Serial2.println(maxOS);
    Serial2.println("Zeta: ");
    Serial2.println(zeta[reboundRun]);
    delay(4000);
    
    // Iterate to next Rebound Run
    reboundRun = reboundRun + 1;
    
  }

  dZeta_dClick = (zeta[1]-zeta[0])/totalClicks;
  correctClicksMin = int((desiredZeta[0] - zeta[0])/dZeta_dClick);
  if (correctClicksMin < 0) {
    correctClicksMin = 0;
  }
  
  correctClicksMax = totalClicks + int((zeta[1] - desiredZeta[1])/dZeta_dClick);
    if (correctClicksMax > totalClicks) {
    correctClicksMax = totalClicks;
  }
  
  Serial2.println("Min Clicks: ");
  Serial2.println(correctClicksMin);
  if (correctClicksMin < 0) {
    Serial2.println("Service Suspension or consider running more sag");
  }
  Serial2.println("Max Clicks: ");
  Serial2.println(correctClicksMax);
    if (correctClicksMax == totalClicks) {
    Serial2.println("Service Suspension or consider running more sag");
  }
  delay(100000);
 }
