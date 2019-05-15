#include <RTClib.h>
#include <SPI.h>
#include <Wire.h>
#include <Rotary.h>  /*by buxtronix you are my hero*/

/*
 * fix time index
 * 0 = none, 1 = Sec, 2 = Min, 3 = hour
 * 4 = day, 5 = month, 6 = year
 * same for which to blink.  don't do seconds
 */

/*
 * for my nixie clock display.  in the format yr/mo/dy  hr/min/sec
 * had a hell of a time getting spi to work so just used the shiftout 
 * using the max6922 io expander works pretty well.
 * using a rotary encoder/button combo to change time
 * probably a lot can be done to optimize the code but its functional
 * thanks to buxtronix for the rotary library
 * was all but given up on the rotary after I couldn't find a reliable way 
 * to do it.  their library does it almost flawlessly
 * sometimes misses detents but thats fine for this application
 */

RTC_DS3231 rtc;
const int LOAD = 6;  //load pin on the max6922s chained to all chips
const int CLOCK = 12; //also to all chips
const int DATA = 11;  //serial out to each chip.  each chip has a data out
//that goes to the next in the chain
const int RE_A = 10;  //rotary encoder A
const int RE_B = 9;  //rotary encoder B
const int RE_NO = 5;  //Rotary encoder NO switch
const uint32_t BLANK = 0xffffffff;  //for blanking and anding the display
char ZEROES[4] = {'0', '0', '0', '\0'};  //for lost time events
int lastSecond = 0;
int lastDay = 0;
DateTime now;  //from the rtclib library
//array for cycling through all digits in the display
char CHECK[][10] = {{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'},
                    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'},
                    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'}};
int CHECKindex = 0;
bool pressed = 0;
long lmPressed = millis();
bool lostpower = 0;
long lmLostPower = millis();
bool lostpowerblink = 0;
bool oneshot = 0;
Rotary rotary = Rotary(RE_A, RE_B);  //from the rotary library
bool CW = 0;
bool CCW = 0;



void setup() {
  delay(3000);
  pinMode(RE_A, INPUT_PULLUP);
  pinMode(RE_B, INPUT_PULLUP);
  pinMode(RE_NO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RE_A), rotate, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RE_B), rotate, CHANGE);
  pinMode(LOAD, OUTPUT);
  digitalWrite(LOAD, LOW);
  pinMode(CLOCK, OUTPUT);
  digitalWrite(CLOCK, LOW);
  pinMode(DATA, OUTPUT);
  SendOutStuff(BLANK, BLANK, BLANK, BLANK);  //clear the display
  if (! rtc.begin()){  //if no rtc do nothing
    while (1);
  }
  if (rtc.lostPower()) {  //if rtc lost power blink 0 like a vcr
    lostpower = 1;
    lmLostPower = millis();
    while(lostpower){
      if ((millis() - lmLostPower) > 250){
        lmLostPower = millis();
        if (lostpowerblink){
          lostpowerblink = 0;
          SendOutStuff(CD(ZEROES), CD(ZEROES), CD(ZEROES), CD(ZEROES));
        }
        else {
          lostpowerblink = 1;
          SendOutStuff(BLANK, BLANK, BLANK, BLANK);
        }
      }
      if (!digitalRead(RE_NO)){  //when the button is pressed go into change time mode
        pressed = 1;
        oneshot = 1;
        now = DateTime(2000, 00, 00, 00, 00, 00); //initialize date and time
        ChangeTime();
        lostpower = 0;
      }
    }
  }
}
 
void loop() {
  now = rtc.now();
  if (!digitalRead(RE_NO) && !pressed) {  //go into change time mode if button pressed
    pressed = 1;
    lmPressed = millis();
  }
  if (digitalRead(RE_NO)) {
    pressed = 0;
    oneshot = 0;
  }
  if (pressed && ((millis() - lmPressed) > 100) && !oneshot){
    oneshot = 1;
    ChangeTime();
  }
  if (lastDay != now.day()){   //cycle through digits at midnight
    lastDay = now.day();
    CHECKindex = 0;
  }
  if (lastSecond != now.second()){  //update display on the second tick
    lastSecond = now.second();
    if (CHECKindex >= 10) RefreshDisplay(now, 0);
    if (CHECKindex <= 9) CycleDisplay();
  }
}

void rotate() {  //IRQ for the encoder  Sets direction, updates handled elsewhere
  unsigned char result = rotary.process();
  if (result == DIR_CW) CW = 1;
  if (result == DIR_CCW) CCW = 1;
}

//routine for setting the time defaults seconds to 0
//could do it like watches and increment minutes when seconds tick up
//but why?
void ChangeTime(){  
  bool blinkornot = 0;
  int year = now.year();
  int month = now.month();
  int day = now.day();
  int hour = now.hour();
  int minute = now.minute();
  int second = 0;   
  int FixTimeIndex = 2;
  bool lockin = 1;
  long lmScreen = millis();
  long lmButton = millis();
  while (lockin){
    if (CW) {
      switch (FixTimeIndex){
        case 2:
        minute++;
        if (minute >= 60) minute = 0;
        break;
        case 3:
        hour++;
        if (hour >= 24) hour = 0;
        break;
        case 4:
        day++;
        if (day >= 32) day = 0;
        break;
        case 5:
        month++;
        if (month >= 13) month = 0;
        break;
        case 6:
        year++;
        if (year >= 100) year = 0;
        break;
        default:
        break;
        }
        CW = 0;
    }
    if (CCW){
      switch (FixTimeIndex){
        case 2:
        minute--;
        if (minute <= -1) minute = 59;
        break;
        case 3:
        hour--;
        if (hour <= -1) hour = 23;
        break;
        case 4:
        day--;
        if (day <= -1) day = 31;
        break;
        case 5:
        month--;
        if (month <= -1) month = 12;
        break;
        case 6:
        year--;
        if (year <= -1) year = 99;
        break;
        default:
        break;
        }
        CCW = 0;
    }   
    if (digitalRead(RE_NO)) {
      pressed = 0;
      oneshot = 0;
    }
    if (!digitalRead(RE_NO) && !pressed){
      pressed = 1;
      lmButton = millis();
    }
    if (pressed){
      if ((millis() - lmButton) > 3000){
        now = DateTime(year, month, day, hour, minute, second);
        rtc.adjust(now);
        oneshot = 1;
        lockin = 0;
      }
      if (((millis() - lmButton) <3000) && ((millis() - lmButton) > 100 ) && !oneshot){
        FixTimeIndex++;
        if (FixTimeIndex >= 7) FixTimeIndex = 2;
        lmButton = millis();
        oneshot = 1;
      }
    }  
    if ((millis() - lmScreen) > 250){
      lmScreen = millis();
      if (blinkornot) {
        blinkornot = 0;
        RefreshDisplay(DateTime(year, month, day, hour, minute, second), FixTimeIndex);
      }
      else {
        RefreshDisplay(DateTime(year, month, day, hour, minute, second), 0);
        blinkornot = 1;
      }
    }
  }
}
//for cycling display at midnight
void CycleDisplay(){
  char DATE012[4] = {CHECK[0][CHECKindex], CHECK[1][CHECKindex], CHECK[2][CHECKindex], '\0'};
  char DATE345[4] = {CHECK[0][CHECKindex], CHECK[1][CHECKindex], CHECK[2][CHECKindex], '\0'};
  char TIME012[4] = {CHECK[0][CHECKindex], CHECK[1][CHECKindex], CHECK[2][CHECKindex], '\0'};
  char TIME345[4] = {CHECK[0][CHECKindex], CHECK[1][CHECKindex], CHECK[2][CHECKindex], '\0'};
  SendOutStuff(CD(DATE012), CD(DATE345), CD(TIME012), CD(TIME345));
  CHECKindex++;
}
//pushes time out to display
//uses the blink for the change time thing
void RefreshDisplay(DateTime toupdate, int indextoblink){
    char Year[4];
    sprintf(Year, "%02u", (toupdate.year()-2000));
    char Month[4];
    sprintf(Month, "%02u", toupdate.month());
    char Day[4];
    sprintf(Day, "%02u", toupdate.day());
    char Hour[4];
    sprintf(Hour, "%02u", toupdate.hour());
    char Minute[4];
    sprintf(Minute, "%02u", toupdate.minute());
    char Second[4];
    sprintf(Second, "%02u", toupdate.second());
    char DATE012[4] = {Year[0], Year[1], Month[0], '\0'};
    char DATE345[4] = {Month[1], Day[0], Day[1], '\0'};
    char TIME012[4] = {Hour[0], Hour[1], Minute[0], '\0'};
    char TIME345[4] = {Minute[1], Second[0], Second[1], '\0'};
    switch (indextoblink){
      case 1:
      TIME345[1] = 'b';
      TIME345[2] = 'b';
      break;
      case 2:
      TIME012[2] = 'b';
      TIME345[0] = 'b';
      break;
      case 3:
      TIME012[0] = 'b';
      TIME012[1] = 'b';
      break;
      case 4:
      DATE345[1] = 'b';
      DATE345[2] = 'b';
      break;
      case 5:
      DATE012[2] = 'b';
      DATE345[0] = 'b';
      break;
      case 6:
      DATE012[0] = 'b';
      DATE012[1] = 'b';
      break;
      default:
      break;
    }
    SendOutStuff(CD(DATE012), CD(DATE345), CD(TIME012), CD(TIME345));
}
/*
 * the display uses the max6922 and a zener clamp.  a logic high turns the 
 * digit off while a logic low turns it on so this is to set the 
 * appropriate digit to the correct logic level and pack it into 32 bits
 * to go out.  32 bits per chip 4 chips
 * this does one 32 bit chunk
 */
uint32_t CD(char tobreak[4]){
  uint32_t broken;
  int val[3];
    for (int i = 0; i < 3; i++){
      switch (tobreak[i]){
        case '1':
        val[i] = 0b0100000000;
        break;
        case '2':
        val[i] = 0b0010000000;
        break;
        case '3':
        val[i] = 0b0001000000;
        break;
        case '4':
        val[i] = 0b0000100000;
        break;
        case '5':
        val[i] = 0b0000010000;
        break;
        case '6':
        val[i] = 0b0000001000;
        break;
        case '7':
        val[i] = 0b0000000100;
        break;
        case '8':
        val[i] = 0b0000000010;
        break;
        case '9':
        val[i] = 0b0000000001;
        break;
        case '0':
        val[i] = 0b1000000000;
        break;
        case 'b':
        val[i] = 0b0000000000;
        break;
        default:
        val[i] = 0b0000000000;
        break;
      }
    }
    broken = (val[0]<<22) + (val[1]<<12) + (val[2]<<2);
    return broken ^ BLANK;
}
/*
 * this sends out 4 32 bit chunks to the chips then cycles the load pin
 * to load in the sent data to the outputs
 */
void SendOutStuff(uint32_t D012, uint32_t D345, uint32_t T012, uint32_t T345){
  byte val[4];
  val[0] = T345 >> 24;
  val[1] = T345 >> 16;
  val[2] = T345 >> 8;
  val[3] = T345;
  for (int i = 0; i < 4; i++){
    shiftOut(DATA, CLOCK, MSBFIRST, val[i]);
  }
  val[0] = T012 >> 24;
  val[1] = T012 >> 16;
  val[2] = T012 >> 8;
  val[3] = T012;
  for (int i = 0; i < 4; i++){
    shiftOut(DATA, CLOCK, MSBFIRST, val[i]);
  }
  val[0] = D345 >> 24;
  val[1] = D345 >> 16;
  val[2] = D345 >> 8;
  val[3] = D345;
  for (int i = 0; i < 4; i++){
    shiftOut(DATA, CLOCK, MSBFIRST, val[i]);
  }
  val[0] = D012 >> 24;
  val[1] = D012 >> 16;
  val[2] = D012 >> 8;
  val[3] = D012;
  for (int i = 0; i < 4; i++){
    shiftOut(DATA, CLOCK, MSBFIRST, val[i]);
  }
  digitalWrite(LOAD, HIGH);
  digitalWrite(LOAD, LOW);
}
