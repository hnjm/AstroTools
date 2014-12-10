
/* 
 Focuser - controlling of the telescope focuser with stepper motor. 
 
 The motor driver is attached to digital pins 8, 9, 10, 11, 12, 13 of the Arduino.
 The Left and Right buttons conected to pins 2 and 4. 
 The Release button - pin 7.
 A speed control (potentiometer) is connected to analog input 0.
  
 The motor will rotate in a clockwise or opposite direction depends on the button pressed. 
 The higher the potentiometer value, the faster the motor speed. 
 Because setSpeed() sets the delay between steps, 
 you may notice the motor is less responsive to changes in the sensor value at 
 low speeds.
 
 In case of zero speed (potentimetsr gives 0), it works in step-by-step mode. 
 This mean then with each button press only one step comes, holding button do nothing. Its important for final turning of focus 
 (disabled with micro-step driver).
 
 Created 20.04.2014
 by Alexey V. Popov
 St.Petersbrug 
 9141866@gmail.com
 
 */

//#include <Stepper.h>
#include <StepperClass.h>

#define DirectionPin 9                      //Direction Pin - Initial State is ZERO
#define StepPin 8                           //Step Pin - Pulse this to step the motor in the direction selected by the Direction Pin

#define MS1 12
#define MS2 11
#define MS3 10
#define A4988_ENABLES 3

const char SketchVersion[] = "1.1";

//  Communication protocol
// 168 - first byte, #13#10 - end of the command

const int FOCUSER_STOP = 210;
const int FOCUSER_STEP_RIGHT = 211;
const int FOCUSER_ROLL_RIGHT = 212;
const int FOCUSER_RANGE_CHECK = 215;
const int FOCUSER_STEP_LEFT = 209;
const int FOCUSER_ROLL_LEFT = 208;
const int FOCUSER_PING = 255;
const int FOCUSER_HANDSHAKE = 254;
const int FOCUSER_STEPPED = 253;
const int FOCUSER_ROLLING = 252;
const int FOCUSER_GET_SPEED = 240;  
const int FOCUSER_SET_SPEED = 241;  
const int FOCUSER_RELEASE = 220; 
const int FOCUSER_CMD_START = 168;
const int FOCUSER_GET_MAX_SPEED = 239;
const int FOCUSER_GET_MIN_SPEED = 238;
const int FOCUSER_SET_MICROSTEP = 231;
const int FOCUSER_GET_MICROSTEP = 230;
const int FOCUSER_GET_POSITION = 225;
const int FOCUSER_GO_TO_POSITION = 226;
const int FOCUSER_RESET_POSITION = 227;
const int FOCUSER_SET_MAX_POSITION = 228;
const int FOCUSER_SET_POSITION = 229;
const int FOCUSER_CMD_STOP_1 = 13;
const int FOCUSER_CMD_STOP_2 = 10;
const int FOCUSER_CMD_DEBUG = 251;
const int FOCUSER_DEBUG = 250;


//  Arduino 
const int buttonPinLeft = 2;     // 
const int buttonPinRight = 4;     // 
const int buttonPinRelease = 7;
const int speedled = 6;           // speed led
const int motorled = 5;           // motor led

int buttonStateLeft = 0;         // variable for reading the pushbutton status
int buttonOldStateLeft = LOW;
int buttonStateRight = 0;         // variable for reading the pushbutton status
int buttonOldStateRight = LOW;

// for your motor
long MAXSPEED = 100000;
long MINSPEED = 250;

unsigned long LastAction=0, LastPosCheck = 0, LastSpeedCheck = 0;

// initialize the stepper library on pins 8 through 11, SeeedStudio MotorShield:
//Stepper myStepper(stepsPerRevolution, 8,11,12,13);            

//initialize the stepper library AccelStepper on pins 8 (STEP) and 9 (DIR) , A4988:
//AccelStepper myStepper(AccelStepper::DRIVER, StepPin, DirectionPin); // Defaults to AccelStepper::FULL4WIRE (4 pins) on 2, 3, 4, 5

StepperClass FocuserStepper(StepPin, DirectionPin, A4988_ENABLES, MS1, MS2, MS3, 200, A4988);

long OldMotorSpeedDU = -100;
long old_position = 0;

int ReleaseTime = 10000;
int CheckSpeedTime = 100;

const int ROLLING_LEFT = -1;
const int ROLLING_RIGHT = 1;
const int HOLD = 0;
int IsRolling = HOLD;

int IsRollingToNewPos = 0;

int IsDebug = 0;

int IsRangeCheck = 0;
int SerialBuf[255];
int BufLength = 0;

// -------------------------------------


void setup() {

    FocuserStepper.Init();   
    FocuserStepper.SetMicroStep(MICROSTEP16);
    FocuserStepper.fSpeed = 500;
    FocuserStepper.fMinSpeed = MINSPEED;
    FocuserStepper.fMaxSpeed = MAXSPEED;
    
    pinMode(buttonPinLeft, INPUT);
    pinMode(buttonPinRight, INPUT);
    pinMode(buttonPinRelease, INPUT);
    pinMode(speedled, OUTPUT);
    pinMode(motorled, OUTPUT);

//    pinMode(DirectionPin, OUTPUT);
//    pinMode(StepPin, OUTPUT);
//  A4988 microstepping
//    pinMode(MS1, OUTPUT);
//    pinMode(MS2, OUTPUT);  
//   pinMode(MS3, OUTPUT);   
//  A4988 Enable outputs
//    pinMode(A4988_ENABLES, OUTPUT);
    
    Serial.begin(9600);
    Serial.println("Started!");
//    Serial.println(steptime_microsec);
    
    GetMotorSpeed();
    
    LastAction = millis();
}

void SendCmd(int cmd)
{
       Serial.write(FOCUSER_CMD_START);
       Serial.write(cmd);
       Serial.println("");
}

void SendCmd(int cmd, int par)
{
       Serial.write(FOCUSER_CMD_START);
       Serial.write(cmd);
       Serial.write(par);       
       Serial.println("");
}

void SendCmd(int cmd, String str)
{
       Serial.write(FOCUSER_CMD_START);
       Serial.write(cmd);
       Serial.println(str);
}

boolean UpdatePosition(){
  
   if (old_position != FocuserStepper.fPosition){
      old_position = FocuserStepper.fPosition;
      SendCmd(FOCUSER_GET_POSITION, String(FocuserStepper.fPosition));
      return true;
   } 
  
   return false; 

}

void GetMotorSpeed()
{
// read the sensor value:
  int sensorReading = analogRead(A0);
  int new_motorSpeed = sensorReading; //map(sensorReading, 0, 1023, 1, 200);
  int speed_diff = new_motorSpeed-OldMotorSpeedDU;
  if ( abs(speed_diff)>4){ 
      if (IsDebug)
        SendCmd(FOCUSER_CMD_DEBUG, "change motor speed: " + String(OldMotorSpeedDU) + " -> " + String(new_motorSpeed));
      OldMotorSpeedDU = new_motorSpeed;      
      SetSpeed(map(new_motorSpeed, 0, 1023, MINSPEED, MAXSPEED));
  }
}

void SetSpeed(long new_motorSpeed)
{ 
  if (FocuserStepper.fSpeed != new_motorSpeed){              
    FocuserStepper.fSpeed = new_motorSpeed;
    int led_brightness = 0;
    led_brightness = map(new_motorSpeed, MINSPEED, MAXSPEED, 5, 255);
    analogWrite(speedled, led_brightness);    
    SendCmd(FOCUSER_GET_SPEED, String(FocuserStepper.fSpeed));
   }  
}

void Roll(int dir){     
      analogWrite(motorled, HIGH);       
      FocuserStepper.Roll(dir);      
      analogWrite(motorled, LOW);        
}


int FindCmdStart(int *buff, int length)
{
    for (int i=0; i<length; i++){
       if (buff[i]==FOCUSER_CMD_START) 
           return i;       
    }
    return -1;
}

int FindCmdEnd(int *buff, int length)
{
    for (int i=0; i<length-1; i++){
       if ((buff[i]==13)&&(buff[i+1]==10))
           return i;       
    }
    return -1;
}

int ShiftBuffer(int *SerialBuffer, int length, int x)
{
    for (int i=x; i!=length; i++)
      SerialBuffer[i-x] = SerialBuffer[i];

    for (int i=length-x; i!=length; i++)
      SerialBuffer[i] = 0;
      
    return length-x;
}

void serialEvent(){
   if (Serial.available()>0){

    if (IsDebug)
      SendCmd(FOCUSER_CMD_DEBUG, "serial event, buff length: " + String(Serial.available()));
    int command = -1;
    int cmd_end = -1;
    
    do{ 
      command = Serial.read();
      SerialBuf[BufLength] = command;
      BufLength++;
    } while (Serial.available()>0);   
       
    if (IsDebug)
      SendCmd(FOCUSER_CMD_DEBUG, "data recieved: " + String(BufLength));
 
    while (BufLength>0){
    
        int x = FindCmdStart(SerialBuf, BufLength);   
                           
        if (x>0){
           if (IsDebug)           
             SendCmd(FOCUSER_CMD_DEBUG, "cmd start >0 - shift");
           BufLength = ShiftBuffer(SerialBuf, BufLength, x);
           continue;
        }
        
        if (x==-1){
           BufLength = 0;
           if (IsDebug)
             SendCmd(FOCUSER_CMD_DEBUG, "cmd not found");
           return;
        }       
        
        cmd_end = FindCmdEnd(SerialBuf, BufLength);          
        if (cmd_end ==-1){
           if (IsDebug)          
             SendCmd(FOCUSER_CMD_DEBUG, "cmd end not found");
           return;
        }
                             
        LastAction = millis();    
        String str = SketchVersion;
        command = SerialBuf[1];
        String param = "";
        for (int i=2; i<cmd_end; i++){
          param = param + char(SerialBuf[i]);
        }       
        if (IsDebug)       
          SendCmd(FOCUSER_CMD_DEBUG, "cmd found: " + String(command));
        
        switch (command){
          case FOCUSER_PING:   //  ping back;
               SendCmd(FOCUSER_PING);
             break;
             
          case FOCUSER_HANDSHAKE:  // handshake
              SendCmd(FOCUSER_HANDSHAKE, str);
            break; 
            
          case FOCUSER_STOP:  // stop
             SendCmd(FOCUSER_STOP);             
             IsRolling = false;    
             SendCmd(FOCUSER_GET_POSITION, String(FocuserStepper.fPosition));                          
            break;             
            
          case FOCUSER_STEP_RIGHT: // step right
             FocuserStepper.Step(STEP_BACKWARD);  
             SendCmd(FOCUSER_STEP_RIGHT);
            break;      
            
          case FOCUSER_STEP_LEFT: //  step left
             FocuserStepper.Step(STEP_FORWARD);
             SendCmd(FOCUSER_STEP_LEFT);
            break;

          case FOCUSER_ROLL_RIGHT: // rolling right
            IsRolling = ROLLING_RIGHT;
            SendCmd(FOCUSER_ROLL_RIGHT);            
            break;      
                       
          case FOCUSER_ROLL_LEFT: //  rolling left     
            IsRolling = ROLLING_LEFT;      
            SendCmd(FOCUSER_ROLL_LEFT);            
            break;      
            
          case FOCUSER_RELEASE:
              SendCmd(FOCUSER_RELEASE);
              if  (IsRollingToNewPos==HIGH){
                IsRollingToNewPos = 0;
                SendCmd(FOCUSER_GO_TO_POSITION, "0");
              }
              FocuserStepper.EnablePower(false);
              UpdatePosition();                                                  
            break;  
            
          case FOCUSER_GET_SPEED:
             SendCmd(FOCUSER_GET_SPEED, String(FocuserStepper.fSpeed));         
            break;
            
          case FOCUSER_GET_MAX_SPEED:
             SendCmd(FOCUSER_GET_MAX_SPEED, String(MAXSPEED));
            break; 
           
          case FOCUSER_GET_MIN_SPEED:
             SendCmd(FOCUSER_GET_MIN_SPEED, String(MINSPEED));
            break; 
           
          case FOCUSER_SET_MICROSTEP:
                FocuserStepper.SetMicroStep(param.toInt());                
                SendCmd(FOCUSER_GET_MICROSTEP, FocuserStepper.fMicroStep);                
              break;
              
          case FOCUSER_GET_MICROSTEP:  
               SendCmd(FOCUSER_GET_MICROSTEP, FocuserStepper.fMicroStep);
              break; 
              
        case FOCUSER_RESET_POSITION:
                SendCmd(FOCUSER_RESET_POSITION);
                FocuserStepper.fPosition = 0;
                SendCmd(FOCUSER_GET_POSITION, "0");
              break;    
              
        case FOCUSER_GET_POSITION:
              SendCmd(FOCUSER_GET_POSITION, String(FocuserStepper.fPosition));
              break;
              
        case FOCUSER_GO_TO_POSITION:{
               int new_position = param.toInt();
               SendCmd(FOCUSER_GO_TO_POSITION, String(new_position));  
               FocuserStepper.fRelativePosition = new_position-FocuserStepper.fPosition; 
               if(IsDebug)
                 SendCmd(FOCUSER_CMD_DEBUG, "Going to new relative position:" + String(FocuserStepper.fRelativePosition));          
               FocuserStepper.fTargetPosition = new_position;
               IsRollingToNewPos = HIGH;
              break;
        }
        case FOCUSER_SET_MAX_POSITION:
                FocuserStepper.fMaxPosition = param.toInt();               
                SendCmd(FOCUSER_SET_MAX_POSITION, param ); //String(max_position));        
              break;                      

        case FOCUSER_SET_POSITION:
              FocuserStepper.fPosition = param.toInt();
              SendCmd(FOCUSER_GET_POSITION, String(FocuserStepper.fPosition));
             break;        
              
        case FOCUSER_RANGE_CHECK:
               IsRangeCheck = SerialBuf[2];
               SendCmd(FOCUSER_RANGE_CHECK, SerialBuf[2]);
             break;
        case FOCUSER_SET_SPEED:{
               long new_speed = param.toInt();
               if (new_speed>MAXSPEED)
                  new_speed=MAXSPEED;
               SetSpeed(new_speed);
            break;              
        }   
         case FOCUSER_DEBUG:
             IsDebug = SerialBuf[2];
             SendCmd(FOCUSER_DEBUG, IsDebug);
            break;          
       }      
       
       BufLength = ShiftBuffer(SerialBuf, BufLength, cmd_end+2);
    }    
  } 
}


void loop() {     
  

  if (millis() - LastSpeedCheck > CheckSpeedTime){   
     GetMotorSpeed();
     LastSpeedCheck = millis();
  }
          
  if (IsRollingToNewPos==HIGH){
    
    FocuserStepper.fRelativePosition = FocuserStepper.fTargetPosition-FocuserStepper.fPosition;    
    if (abs(FocuserStepper.fRelativePosition)>=FocuserStepper.fMicroSteps[FocuserStepper.fMicroStep]){
//       FocuserStepper.fSpeed = 250;
       if (FocuserStepper.fRelativePosition>0)        
            Roll(STEP_FORWARD);
       else if (FocuserStepper.fRelativePosition<0)
            Roll(STEP_BACKWARD);
    }
     
    if (abs(FocuserStepper.fRelativePosition)<FocuserStepper.fMicroSteps[FocuserStepper.fMicroStep]){
        UpdatePosition();         
        IsRollingToNewPos=LOW;
        SendCmd(FOCUSER_GO_TO_POSITION, "0");
    }    
   LastAction = millis();              
           
    if (IsDebug)
       SendCmd(FOCUSER_CMD_DEBUG, "distance:" + String(FocuserStepper.fRelativePosition));                                  
  } else {

    buttonStateLeft = digitalRead(buttonPinLeft);  
    buttonStateRight = digitalRead(buttonPinRight);
    if ((buttonStateLeft==HIGH)||(IsRolling==ROLLING_LEFT)){
       Roll(STEP_FORWARD);
       LastAction = millis();              
    } else if ((buttonStateRight==HIGH)||(IsRolling==ROLLING_RIGHT)) {  
       Roll(STEP_BACKWARD);
       LastAction = millis();          
    }
    buttonOldStateRight = buttonStateRight; 
    buttonOldStateLeft = buttonStateLeft;
  }    
  
  if (FocuserStepper.fEnabled){
    int buttonPinState = digitalRead(buttonPinRelease);      
    int time_diff = millis() - LastAction;
    if ((time_diff>ReleaseTime)||(buttonPinState==HIGH)){    
       FocuserStepper.EnablePower(false);      
       SendCmd(FOCUSER_RELEASE);      
    }       
  }
 

  int time_diff = millis() - LastPosCheck;
  if (time_diff>200){
      UpdatePosition();
      LastPosCheck = millis();
  }
  
}





