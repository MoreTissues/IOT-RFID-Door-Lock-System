#include <EEPROM.h>     // We are going to read and write Tag's UIDs from/to EEPROM
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <SPI.h>
#include <Wire.h>
#include <SoftwareSerial.h>
SoftwareSerial Bluetooth(10, 11); // RX, TX
uint8_t Data; // the data received


// Create instances
MFRC522 mfrc522(53, 5); // MFRC522 mfrc522(SS_PIN, RST_PIN)
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;  // create servo object to control a servo

// Set Pins for led's, servo, buzzer and wipe button
constexpr uint8_t greenLed = 8;
constexpr uint8_t blueLed = 7;
constexpr uint8_t redLed = 6;
constexpr uint8_t ServoPin = 9;
constexpr uint8_t BuzzerPin = 4;
constexpr uint8_t wipeB = 3; 

constexpr uint8_t trigPin = 3;
constexpr uint8_t echoPin = 2;
constexpr uint8_t led = 13;


boolean match = false;          // initialize card match to false
boolean programMode = false;  // initialize programming mode to false
boolean replaceMaster = false;

uint8_t successRead;    // Variable integer to keep if we have Successful Read from Reader

byte storedCard[4];   // Stores an ID read from EEPROM
byte readCard[4];   // Stores scanned ID read from RFID Module
byte masterCard[4];   // Stores master card's ID read from EEPROM

char storedPass[4]; // Variable to get password from EEPROM
char password[4];   // Variable to store users password
char masterPass[4]; // Variable to store master password
boolean RFIDMode = true; // boolean to change modes
boolean NormalMode = true; // boolean to change modes
char key_pressed = 0; // Variable to store incoming keys
uint8_t i = 0;  // Variable used for counter

// defining how many rows and columns our keypad have
const byte rows = 4;
const byte columns = 4;

// Keypad pin map
char hexaKeys[rows][columns] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Initializing pins for keypad
byte row_pins[rows] = {43, 41, 39, 37};
byte column_pins[columns] = {35, 33 , 31};

// Create instance for keypad
Keypad newKey = Keypad( makeKeymap(hexaKeys), row_pins, column_pins, rows, columns);


///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
  Bluetooth.begin(9600);
  Serial.begin(9600);
  Serial.println("Waiting for command...");
  Bluetooth.println("Group Fred Smart Door Lock Test");

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(led, OUTPUT);
  
  //Arduino Pin Configuration
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(BuzzerPin, OUTPUT);
  pinMode(wipeB, INPUT_PULLUP);   // Enable pin's pull up resistor

  // Make sure leds are off
  digitalWrite(redLed, LOW);
  digitalWrite(greenLed, LOW);
  digitalWrite(blueLed, LOW);

  //Protocol Configuration
  lcd.begin();  // initialize the LCD
  lcd.backlight();
  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();    // Initialize MFRC522 Hardware
  myServo.attach(ServoPin);   // attaches the servo on pin 8 to the servo object
  myServo.write(10);   // Initial Position

  //If you set Antenna Gain to Max it will increase reading distance
  //mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  ShowReaderDetails();  // Show details of PCD - MFRC522 Card Reader details

    digitalWrite(redLed, HIGH); // Red Led stays on to inform user we are going to wipe
    lcd.setCursor(0, 0);
    lcd.print("Wipe EEPROM");
    digitalWrite(BuzzerPin, HIGH);
    delay(1000);
    digitalWrite(BuzzerPin, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("This will remove");
    lcd.setCursor(0, 1);
    lcd.print("all records");
    delay(2000);
      lcd.clear();
      lcd.print("Wiping EEPROM...");
      for (uint16_t x = 0; x < EEPROM.length(); x = x + 1) {    //Loop end of EEPROM address
        if (EEPROM.read(x) == 0) {              //If EEPROM address 0
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        }
        else {
          EEPROM.write(x, 0);       // if not write 0 to clear, it takes 3.3mS
        }
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wiping Done");
      // visualize a successful wipe
      cycleLeds();
  
  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine the Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(1) != 143) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No Master Card ");
    Bluetooth.println("No Master Card");
    lcd.setCursor(0, 1);
    lcd.print("Defined");
    Bluetooth.println("Defined");
    delay(2000);

    lcd.setCursor(0, 0);
    lcd.print("Scan A Tag to ");
    Bluetooth.println("Scan A Tag to ");
    lcd.setCursor(0, 1);
    lcd.print("Define as Master");
    Bluetooth.println("Define as Master");

    do {
      successRead = getID();            // sets successRead to 1 when we get read from reader otherwise 0
      // Visualize Master Card need to be defined
      digitalWrite(blueLed, HIGH);
      digitalWrite(BuzzerPin, HIGH);
      delay(200);
      digitalWrite(BuzzerPin, LOW);
      digitalWrite(blueLed, LOW);
      delay(200);
    }
    while (!successRead);                  // Program will not go further while you not get a successful read
    for ( uint8_t j = 0; j < 4; j++ ) {        // Loop 4 times
      EEPROM.write( 2 + j, readCard[j] );  // Write scanned Tag's UID to EEPROM, start from address 3
    }
    EEPROM.write(1, 143);                  // Write to EEPROM we defined Master Card.
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Master Defined");
    Bluetooth.println("Master Defined");
    delay(2000);
    storePassword(6); // Store password for master tag. 6 is the position in the EEPROM
  }
  
  // Read Master Card's UID and master password from EEPROM
  for ( uint8_t i = 0; i < 4; i++ ) {
    masterCard[i] = EEPROM.read(2 + i);    // Write it to masterCard
    masterPass[i] = EEPROM.read(6 + i);    // Write it to masterpass
  }

  ShowOnLCD();    // Print on the LCD
  cycleLeds();    // Everything ready lets give user some feedback by cycling leds
}


///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop () {
  
  // System will first look for mode. if RFID mode is true then it will get the tags otherwise it will get keys
  if (RFIDMode == true) {
    do {
      successRead = getID();  // sets successRead to 1 when we get read from reader otherwise 0

      if (programMode) {
        cycleLeds();              // Program Mode cycles through Red Green Blue waiting to read a new card
      }
      else {
        normalModeOn();     // Normal mode, blue Power LED is on, all others are off
      }
    }
    while (!successRead);   //the program will not go further while you are not getting a successful read
    if (programMode) {
      if ( isMaster(readCard) ) { //When in program mode check First If master card scanned again to exit program mode
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Exiting Program Mode");
        Bluetooth.println("Exiting Program Mode");
        digitalWrite(BuzzerPin, HIGH);
        delay(1000);
        digitalWrite(BuzzerPin, LOW);
        ShowOnLCD();
        programMode = false;
        return;
      }

      else {
        if ( findID(readCard) ) { // If scanned card is known delete it
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Already there");
          Bluetooth.println("Already There");
          deleteID(readCard);

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Tag to ADD/REM");
          Bluetooth.println("Tag to ADD/REM");
          lcd.setCursor(0, 1);
          lcd.print("Master to Exit");
          Bluetooth.println("Master to Exit");
        }

        else {                    // If scanned card is not known add it
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("New Tag,adding...");
          Bluetooth.println("New Tag,adding...");
          writeID(readCard);

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Scan to ADD/REM");
          Bluetooth.println("Scan to ADD/REM");
          lcd.setCursor(0, 1);
          lcd.print("Master to Exit");
          Bluetooth.println("Master to Exit");
        }
      }
    }

    else {
      if ( isMaster(readCard)) {    // If scanned card's ID matches Master Card's ID - enter program mode
        programMode = true;
        matchpass();
        if (programMode == true) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Program Mode");
          Bluetooth.println("Program Mode");

          uint8_t count = EEPROM.read(0);   // Read the first Byte of EEPROM that stores the number of ID's in EEPROM
          lcd.setCursor(0, 1);
          lcd.print("I have ");
          Bluetooth.println("I have ");
          lcd.print(count);
          Bluetooth.println(count);
          lcd.print(" records");
          Bluetooth.println(" records");

          digitalWrite(BuzzerPin, HIGH);
          delay(2000);
          digitalWrite(BuzzerPin, LOW);

          Bluetooth.println("Scan a Tag to ADD/REMOVE");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Scan a Tag to ");
          lcd.setCursor(0, 1);
          lcd.print("ADD/REMOVE");
        }
      }

      else {
        if ( findID(readCard) ) { // If not, see if the card is in the EEPROM
          granted();
          RFIDMode = false;  // Make RFID mode false
          ShowOnLCD();
        }

        else {      // If not, show that the Access is denied
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Access Denied");
          Bluetooth.println("Access Denied");
          denied();
          ShowOnLCD();
        }
      }
    }
  }
  // If RFID mode is false, get keys
  else if (RFIDMode == false) {
    key_pressed = newKey.getKey();  //Store new key

    if (key_pressed)
    {
      password[i++] = key_pressed;  // Storing in password variable
      lcd.print("*");
    }
    if (i == 4) // If 4 keys are completed
    {
      delay(200);

      if (!(strncmp(password, storedPass, 4)))  // If password is matched
      {
        Bluetooth.println("Password Accepted");
        lcd.clear();
        lcd.print("Pass Accepted");
        Bluetooth.println("Door Opened");
        lcd.setCursor(0, 1);
        lcd.print("Door Opened");
        granted();
        RFIDMode = true;   // Make RFID mode true
        ShowOnLCD();
        i = 0;
      }
      else   // If password is not matched
      {
        lcd.clear();
        lcd.print("Wrong Password");
        Bluetooth.println("Wrong Password");
        denied();
        RFIDMode = true;   // Make RFID mode true
        ShowOnLCD();
        i = 0;
      }
    }
  }
}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted () {
  digitalWrite(blueLed, LOW);   // Turn off blue LED
  digitalWrite(redLed, LOW);  // Turn off red LED
  digitalWrite(greenLed, HIGH);   // Turn on green LED
  if (RFIDMode == false) {
    myServo.write(90);
    delay(3000);
    myServo.write(10);
  }
  delay(1000);
  digitalWrite(blueLed, HIGH);
  digitalWrite(greenLed, LOW);
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  digitalWrite(greenLed, LOW);  // Make sure green LED is off
  digitalWrite(blueLed, LOW);   // Make sure blue LED is off
  digitalWrite(redLed, HIGH);   // Turn on red LED
  digitalWrite(BuzzerPin, HIGH);
  delay(1000);
  digitalWrite(BuzzerPin, LOW);
  digitalWrite(blueLed, HIGH);
  digitalWrite(redLed, LOW);
}


///////////////////////////////////////// Get Tag's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading Tags
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new Tag placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a Tag placed get Serial and continue
    return 0;
  }
  // There are Mifare Tags which have 4 byte or 7 byte UID care if you use 7 byte Tag
  // I think we should assume every Tag as they have 4 byte UID
  // Until we support 7 byte Tags
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
  }
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

/////////////////////// Check if RFID Reader is correctly initialized or not /////////////////////
void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);

  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    lcd.setCursor(0, 0);
    lcd.print("Communication Failure");
    Bluetooth.println("Communication Failure");
    lcd.setCursor(0, 1);
    lcd.print("Check Connections");
    Bluetooth.println("Check Connections");
    digitalWrite(BuzzerPin, HIGH);
    delay(2000);
    // Visualize system is halted
    digitalWrite(greenLed, LOW);  // Make sure green LED is off
    digitalWrite(blueLed, LOW);   // Make sure blue LED is off
    digitalWrite(redLed, HIGH);   // Turn on red LED
    digitalWrite(BuzzerPin, LOW);
    while (true); // do not go further
  }
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
  digitalWrite(redLed, LOW);  // Make sure red LED is off
  digitalWrite(greenLed, HIGH);   // Make sure green LED is on
  digitalWrite(blueLed, LOW);   // Make sure blue LED is off
  delay(200);
  digitalWrite(redLed, LOW);  // Make sure red LED is off
  digitalWrite(greenLed, LOW);  // Make sure green LED is off
  digitalWrite(blueLed, HIGH);  // Make sure blue LED is on
  delay(200);
  digitalWrite(redLed, HIGH);   // Make sure red LED is on
  digitalWrite(greenLed, LOW);  // Make sure green LED is off
  digitalWrite(blueLed, LOW);   // Make sure blue LED is off
  delay(200);
  digitalWrite(redLed, LOW);   // Make sure red LED is on
  digitalWrite(greenLed, LOW);  // Make sure green LED is off
  digitalWrite(blueLed, HIGH);   // Make sure blue LED is off
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {
  digitalWrite(blueLed, HIGH);  // Blue LED ON and ready to read card
  digitalWrite(redLed, LOW);  // Make sure Red LED is off
  digitalWrite(greenLed, LOW);  // Make sure Green LED is off
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( uint8_t number ) {
  uint8_t start = (number * 8) + 2; // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
    storedPass[i] = EEPROM.read(start + i + 4);
  }
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 8 ) + 10; // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    storePassword(start + 4);
    BlinkLEDS(greenLed);
    lcd.setCursor(0, 1);
    lcd.print("Added");
    Bluetooth.println("Added");
    delay(1000);
  }
  else {
    BlinkLEDS(redLed);
    lcd.setCursor(0, 0);
    lcd.print("Failed!");
    Bluetooth.println("Failed!");
    lcd.setCursor(0, 1);
    lcd.print("wrong ID or bad EEPROM");
    Bluetooth.println("wrong ID or bad EEPROM");
    delay(2000);
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    BlinkLEDS(redLed);      // If not
    lcd.setCursor(0, 0);
    lcd.print("Failed!");
    Bluetooth.println("Failed!");
    lcd.setCursor(0, 1);
    lcd.print("wrong ID or bad EEPROM");
    Bluetooth.println("wrong ID or bad EEPROM");
    delay(2000);
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * 8) + 10;
    looping = ((num - slot)) * 4;
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    BlinkLEDS(blueLed);
    lcd.setCursor(0, 1);
    lcd.print("Removed");
    Bluetooth.println("Removed");
    delay(1000);
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != 0 )      // Make sure there is something in the array first
    match = true;       // Assume they match at first
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] )     // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if ( match ) {      // Check to see if if match is still true
    return true;      // Return true
  }
  else  {
    return false;       // Return false
  }
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
      break;          // Stop looking we found it
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  uint8_t count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      return true;
      break;  // Stop looking we found it
    }
    else {    // If not, return false
    }
  }
  return false;
}


///////////////////////////////////////// Blink LED's For Indication   ///////////////////////////////////
void BlinkLEDS(int led) {
  digitalWrite(blueLed, LOW);   // Make sure blue LED is off
  digitalWrite(redLed, LOW);  // Make sure red LED is off
  digitalWrite(greenLed, LOW);  // Make sure green LED is off
  digitalWrite(BuzzerPin, HIGH);
  delay(200);
  digitalWrite(led, HIGH);  // Make sure blue LED is on
  digitalWrite(BuzzerPin, LOW);
  delay(200);
  digitalWrite(led, LOW);   // Make sure blue LED is off
  digitalWrite(BuzzerPin, HIGH);
  delay(200);
  digitalWrite(led, HIGH);  // Make sure blue LED is on
  digitalWrite(BuzzerPin, LOW);
  delay(200);
  digitalWrite(led, LOW);   // Make sure blue LED is off
  digitalWrite(BuzzerPin, HIGH);
  delay(200);
  digitalWrite(led, HIGH);  // Make sure blue LED is on
  digitalWrite(BuzzerPin, LOW);
  delay(200);
}

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) ) {
    return true;
  }
  else
    return false;
}

/////////////////// Counter to check in reset/wipe button is pressed or not   /////////////////////
bool monitorWipeButton(uint32_t interval) {
  unsigned long currentMillis = millis(); // grab current time

  while (millis() - currentMillis < interval)  {
    int timeSpent = (millis() - currentMillis) / 1000;
    Serial.println(timeSpent);
    lcd.setCursor(10, 1);
    lcd.print(timeSpent);

    // check on every half a second
    if (((uint32_t)millis() % 10) == 0) {
      if (digitalRead(wipeB) != LOW) {
        return false;
      }
    }
  }
  return true;
}

////////////////////// Print Info on LCD   ///////////////////////////////////
void ShowOnLCD() {
  if (RFIDMode == false) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Password");
    Bluetooth.println("Enter Password");
    lcd.setCursor(0, 1);
  }

  else if (RFIDMode == true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Access Control");
    Bluetooth.println("Access Control");
    lcd.setCursor(0, 1);
    lcd.print("   Scan a Tag");
    Bluetooth.println("Scan a Tag");
  }
}

////////////////////// Store Passwords in EEPROOM   ///////////////////////////////////
void storePassword(int j) {
  int k = j + 4;
  BlinkLEDS(blueLed);
  lcd.clear();
  lcd.print("New Password:");
  Bluetooth.println("New Password:");
  lcd.setCursor(0, 1);
  while (j < k)
  {
    char key = newKey.getKey();
    if (key)
    {
      lcd.print("*");
      EEPROM.write(j, key);
      j++;
    }
  }
}

////////////////////// Match Passwords  ///////////////////////////////////
void matchpass() {
  RFIDMode = false;
  ShowOnLCD();
  int n = 0;
  while (n < 1) { // Wait until we get 4 keys
    key_pressed = newKey.getKey();  //Store new key
    if (key_pressed)
    {
      password[i++] = key_pressed;  // Storing in password variable
      lcd.print("*");
    }
    if (i == 4) // If 4 keys are completed
    {
      delay(200);

      if (!(strncmp(password, masterPass, 4)))  // If password is matched
      {
        RFIDMode = true;
        programMode = true;
        i = 0;
      }
      else   // If password is not matched
      {
        lcd.clear();
        lcd.print("Wrong Password");
        Bluetooth.println("Wrong Password");
        digitalWrite(BuzzerPin, HIGH);
        delay(1000);
        digitalWrite(BuzzerPin, LOW);
        programMode = false;
        RFIDMode = true;
        ShowOnLCD();
        i = 0;
      }
      n = 4;
    }
  }
}
