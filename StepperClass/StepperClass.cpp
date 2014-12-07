#include "Arduino.h"
#include "StepperClass.h"

StepperClass::StepperClass(byte step, byte dir, byte enable, byte ms1, byte ms2, byte ms3, int step_rev, int type)
{
   fStepPin = step;
   fDirPin = dir;
   fEnablePin = enable;
   fEnabled = false;
   fMS1Pin = ms1; fMS2Pin = ms2; fMS3Pin = ms3;   
   fStepsPerRevolutionDefault = step_rev;
   fStepsPerRevolution = fStepsPerRevolutionDefault;

   fPosition = 0; fMaxPosition = 0; fMinPosition = 0;
   fTargetPosition = 0; fRelativePosition = 0;
   fSpeed = 0; fMaxSpeed = 0; fMinSpeed = 0;
   fMicroStep = 0;  

   fType = type;
   if (fType==DRV8825) {
     fMicroSteps[MICROSTEP1] = 32;
     fMicroSteps[MICROSTEP2] = 16;
     fMicroSteps[MICROSTEP4] = 8;
     fMicroSteps[MICROSTEP8] = 4;
     fMicroSteps[MICROSTEP16] = 2;
     fMicroSteps[MICROSTEP32] = 1;
   } else if (fType==A4988){
     fMicroSteps[MICROSTEP1] = 16;
     fMicroSteps[MICROSTEP2] = 8;
     fMicroSteps[MICROSTEP4] = 4;
     fMicroSteps[MICROSTEP8] = 2;
     fMicroSteps[MICROSTEP16] = 1;
   }
   fRangeCheck = false;
   fStepTime_microsec = 2;
   
   fLastStep = 0;
}

void StepperClass::Init()
{   
    pinMode(fDirPin, OUTPUT);
    pinMode(fStepPin, OUTPUT);
    pinMode(fMS1Pin, OUTPUT);
    pinMode(fMS2Pin, OUTPUT);  
    pinMode(fMS3Pin, OUTPUT);
    pinMode(fEnablePin, OUTPUT);

    fEnabled = false;
    digitalWrite(fEnablePin, HIGH);      
}

void StepperClass::EnablePower(boolean enable)
{
   if (fEnabled==enable)
      return;
   fEnabled=enable;   
   if (fEnabled){
     digitalWrite(fEnablePin, LOW);          
     delayMicroseconds(1);         
   }
    else 
     digitalWrite(fEnablePin, HIGH);
}

long StepperClass::Step(int dir)
{
    if (fRangeCheck){
      if ((dir==STEP_BACKWARD)&&(fPosition-fMicroSteps[fMicroStep]<fMinPosition))
          return 0;
      if ((dir==STEP_FORWARD)&&(fPosition+fMicroSteps[fMicroStep]>fMaxPosition)) 
          return 0;
    }

    EnablePower(true);
  
    if (dir==STEP_BACKWARD)    
      digitalWrite(fDirPin, HIGH);
    else if (dir==STEP_FORWARD)
      digitalWrite(fDirPin, LOW);     
    delayMicroseconds(1);  
   
    digitalWrite(fStepPin,HIGH);
    delayMicroseconds(fStepTime_microsec);
    digitalWrite(fStepPin,LOW);
    delayMicroseconds(fStepTime_microsec);

    fPosition+=dir*fMicroSteps[fMicroStep];
    
    return dir*fMicroSteps[fMicroStep];

}   

long StepperClass::Roll(int dir)
{
      long t = micros();
      long dt = t - fLastStep;    
      if (abs(dt)<fSpeed)
         return 0;     
      fLastStep = t;     
      return Step(dir);
}

void StepperClass::SetMicroStep(int new_micro_step)
{
    if (new_micro_step!=fMicroStep){
      switch (new_micro_step) {
        case MICROSTEP1:
         digitalWrite(fMS1Pin, LOW); 
         digitalWrite(fMS2Pin, LOW);
         digitalWrite(fMS3Pin, LOW);
         fMicroStep = new_micro_step;        
        break;
        case MICROSTEP2:
         digitalWrite(fMS1Pin, HIGH); 
         digitalWrite(fMS2Pin, LOW);
         digitalWrite(fMS3Pin, LOW);
         fMicroStep = new_micro_step;
        break;
        case MICROSTEP4:
         digitalWrite(fMS1Pin, LOW); 
         digitalWrite(fMS2Pin, HIGH);
         digitalWrite(fMS3Pin, LOW);
         fMicroStep = new_micro_step;
        break;
        case MICROSTEP8:
         digitalWrite(fMS1Pin, HIGH); 
         digitalWrite(fMS2Pin, HIGH);
         digitalWrite(fMS3Pin, LOW);
         fMicroStep = new_micro_step;
        break;
        case MICROSTEP16:
         if (fType==DRV8825){
           digitalWrite(fMS1Pin, LOW); 
           digitalWrite(fMS2Pin, LOW);
           digitalWrite(fMS3Pin, HIGH);
         } else if (fType==A4988){
           digitalWrite(fMS1Pin, HIGH); 
           digitalWrite(fMS2Pin, HIGH);
           digitalWrite(fMS3Pin, HIGH);
         }
         fMicroStep = new_micro_step;
        break;
        case MICROSTEP32:
         digitalWrite(fMS1Pin, HIGH); 
         digitalWrite(fMS2Pin, HIGH);
         digitalWrite(fMS3Pin, HIGH);
         fMicroStep = new_micro_step;
        break;
      }
      fStepsPerRevolution = fMicroSteps[fMicroStep]*fStepsPerRevolutionDefault;  // change this to fit the number of steps per revolution    
    }
}