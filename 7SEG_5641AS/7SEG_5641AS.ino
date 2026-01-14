#include <Arduino.h>

/* ========= DIGIT PINS (Common Cathode) ========= */
const int DIG1_PIN = 32;
const int DIG2_PIN = 23;
const int DIG3_PIN = 17;
const int DIG4_PIN = 16;

/* ========= 74HC595 CONTROL PINS ========= */
const int SR_SER_PIN   = 4;   // SER
const int SR_CLK_PIN   = 18;  // SRCLK
const int SR_LATCH_PIN = 19;  // RCLK

//SRCLK – Shift Register Clock
//RCLK – Latch Clock

const unsigned long DIGIT_TIME_US = 1500;

const unsigned long TINE_TO_CHANGE_NUM_ms = 1500;

bool ENABLE_7SEG_FLAG = 1;

uint8_t digitMask[10] = { 0b00111111 ,0b00000110, 0b01011011, 0b01001111, 0b01100110, 0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111 };

uint8_t seg7_buffer[4] = {1, 2, 3, 4};


/* ========= SETUP ========= */
void setup() {
  pinMode(DIG1_PIN, OUTPUT);
  pinMode(DIG2_PIN, OUTPUT);
  pinMode(DIG3_PIN, OUTPUT);
  pinMode(DIG4_PIN, OUTPUT);

  pinMode(SR_SER_PIN, OUTPUT);
  pinMode(SR_CLK_PIN, OUTPUT);
  pinMode(SR_LATCH_PIN, OUTPUT);
  

    // --- Safe initial states ---

  // Turn OFF all digits (CC -> HIGH = off)
  digitalWrite(DIG1_PIN, HIGH);
  digitalWrite(DIG2_PIN, HIGH);
  digitalWrite(DIG3_PIN, HIGH);
  digitalWrite(DIG4_PIN, HIGH);

  // Shift register lines idle LOW
  digitalWrite(SR_SER_PIN, LOW);
  digitalWrite(SR_CLK_PIN, LOW);
  digitalWrite(SR_LATCH_PIN, LOW);


}

//this function set rising edge on the pin that the function get
void sr_pulse(int pin) 
{
  digitalWrite(pin, LOW);
  delayMicroseconds(4);
  digitalWrite(pin, HIGH);
  delayMicroseconds(4);
  digitalWrite(pin, LOW);

}

//SER pin decided if the the output is low or high
//when SRCLK pin get rising edge the bit on SER push to the shift register
void sr_shiftBit(bool bitValue)
{
  if(bitValue)
  {
    digitalWrite(SR_SER_PIN, HIGH);
  }
  else
  {
    digitalWrite(SR_SER_PIN, LOW);
  }
  delayMicroseconds(4);
  sr_pulse(SR_CLK_PIN);
}

// in the shift register the first bit to insert to the shift register will go to Qh, the last to Qa
// this function get a vlaue of 8 bits and send bit by bit to "sr_shiftBit" function to insert them to the shift register
//the function send the MSB first
void sr_writeByte(uint8_t value)
{
  bool bit;
  for (int i = 7; i >= 0; i--)
    {
        bit = (value >> i) & 1;
        sr_shiftBit(bit);
    }
  sr_pulse(SR_LATCH_PIN);
}

// this function get digit and send the number to "sr_writeByte" function
void seg7_showDigit(uint8_t digit)
{
  sr_writeByte(digitMask[(digit)]);
}


void seg7_selectDigit(uint8_t digitIndex)
{

  switch (digitIndex) {
  case 0:
      digitalWrite(DIG1_PIN, LOW);
    break;
  case 1:
      digitalWrite(DIG2_PIN, LOW);
    break;
  case 2:
      digitalWrite(DIG3_PIN, LOW);
    break;
  case 3:
      digitalWrite(DIG4_PIN, LOW);
    break;
  default:
    break;
}
}



void seg7_task()
{
  static uint8_t currentDigit = 0;
  static unsigned long lastSwitchUs = 0;

  if (ENABLE_7SEG_FLAG == 0)
  {
    // Turn OFF all digits (CC -> HIGH = off)
    digitalWrite(DIG1_PIN, HIGH);
    digitalWrite(DIG2_PIN, HIGH);
    digitalWrite(DIG3_PIN, HIGH);
    digitalWrite(DIG4_PIN, HIGH);
    sr_writeByte(0x00);
    return;
  }

  if(micros() - lastSwitchUs > DIGIT_TIME_US)
  {
  // Turn OFF all digits (CC -> HIGH = off)
    digitalWrite(DIG1_PIN, HIGH);
    digitalWrite(DIG2_PIN, HIGH);
    digitalWrite(DIG3_PIN, HIGH);
    digitalWrite(DIG4_PIN, HIGH);
    seg7_showDigit(seg7_buffer[currentDigit]);
    seg7_selectDigit(currentDigit);
    currentDigit = (currentDigit + 1) % 4;
    lastSwitchUs = micros();
  }
  else
  {
    return;
  }

}

void seg7_setNumber(int value)
{
  if (value<0) value = 0;
  else if (value>9999) value = 9999;
  seg7_buffer[0] = value/1000;  //Thousands
  seg7_buffer[1] = (value/100)%10; //Hundreds
  seg7_buffer[2] = (value/10)%10;  //Tens
  seg7_buffer[3] = value%10;     //ones

}

void seg7_setEnabled(bool en)
{
  if (en == 0)
  {
    ENABLE_7SEG_FLAG = 0;
  }
  else
  {
    ENABLE_7SEG_FLAG = 1;
  }
}

void loop()
{

static unsigned long LastTimeNumChange = 0;
static int NUMBERS[4] = { 0 , 7, 1234, 9999};
static int i = 0;
if (millis()- LastTimeNumChange > TINE_TO_CHANGE_NUM_ms )
{
  seg7_setNumber(NUMBERS[i]);
  i = (i +1)%4;
  LastTimeNumChange = millis();
}
seg7_task();
 
}