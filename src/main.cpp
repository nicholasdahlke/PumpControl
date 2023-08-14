#include <Arduino.h>

#define DEVICE 0x1

const uint8_t enable_pin = 10;
const uint8_t step_pin = 9;
const uint8_t dir_pin = 8;
const long baud_rate = 115200;
const int pulses_per_rev = 400;
const float mm_per_rev = 0.7;

class Syringe
{
public:
  Syringe(){}
  float volume = 0; // Syringe volume in ml
  float length = 0; // Syringe length in mm

};

class Pump
{
public:
  Pump(){}
  Syringe syringe;
  float rate;
  enum State{RUNNING,STOPPED};
  State pump_state = STOPPED;
  unsigned long get_period();
  void set_state();
};

unsigned long Pump::get_period()
{
  return 1/(rate * (syringe.length / syringe.volume) * (1/mm_per_rev) * (400/60));
}

Pump * pump = new Pump();


enum ErrorType
{
  INVALID_NUM_OF_BYTES = 99,
  ERROR = 1,
  OK = 0
};

enum Direction
{
  FORWARD = LOW,
  REVERSE = HIGH
};

enum Commands
{
  SET_SYRINGE_LENGTH = 0x1,
  SET_SYRINGE_VOLUME = 0x2,
  SET_RATE = 0x3,
  SET_STATE = 0x4,
  MESSAGE_START = 0xc,
  MESSAGE_END = 0xf,
  RESPONSE_START = 0xd,
  RESPONSE_END = 0xf,
  STATUS = 0x6,
  DEVICE_NUMBER = 0x7,
  GET_DEVICE_NUMBER = 0x5

};


void send_error(ErrorType _error)
{
  uint8_t send[5] = {0};
  send[0] = RESPONSE_START;
  send[1] = 2;
  send[2] = STATUS;
  send[3] = _error;
  send[4] = RESPONSE_END;
  Serial.write(&send[0], 5);
}

void send_response(Commands _command, uint8_t * _data, int length)
{
  int data_length = 4+length;
  uint8_t send[data_length] = {0};
  send[0] = RESPONSE_START;
  send[1] = 1 + length;
  send[2] = _command;
  memcpy(&send[3], _data, length);
  send[data_length-1] = RESPONSE_END;
  Serial.write(&send[0], data_length);

}

void serial_handler(uint8_t * _byte_array, int _length)
{
  
  if (_byte_array[0] == MESSAGE_START && _byte_array[_length] == MESSAGE_END)
  {
    switch (_byte_array[2])
    {
    case SET_SYRINGE_LENGTH:
    {
      if (_byte_array[1] != 5) // Message length is 1 + Float byte length is 4 = 5
      {
        send_error(INVALID_NUM_OF_BYTES);
        return;
      }
      float length = 0.0;
      memcpy(&length, _byte_array + 3, 4);
      pump->syringe.length = length;
      send_error(OK);
      break;
    }
    case SET_SYRINGE_VOLUME:
    {
      if (_byte_array[1] != 5) // Message length is 1 + Float byte length is 4 = 5
      {
        send_error(INVALID_NUM_OF_BYTES);
        return;
      }
      float volume = 0.0;
      memcpy(&volume, _byte_array + 3, 4);
      pump->syringe.volume = volume;
      send_error(OK);
      break;
    }  
    case SET_RATE:
    {
      if (_byte_array[1] != 5) // Message length is 1 + Float byte length is 4 = 5
      {
        send_error(INVALID_NUM_OF_BYTES);
        return;
      }
      float rate = 0.0;
      memcpy(&rate, _byte_array + 3, 4);
      pump->rate = rate;
      send_error(OK);
      break;
    }
    case SET_STATE:
    {
      if (_byte_array[1] != 2) // Message length is 1 + int byte length is 1 = 2
      {
        send_error(INVALID_NUM_OF_BYTES);
        return;
      }
      if (_byte_array[3] == 0)
        pump->pump_state = pump->RUNNING;
      else
        pump->pump_state = pump->STOPPED;

      
      send_error(OK);
      break;
    }
    case GET_DEVICE_NUMBER:
    {
      if (_byte_array[1] != 1) // Message length is 1 + int byte length is 1 = 2
      {
        send_error(INVALID_NUM_OF_BYTES);
        return;
      }
      uint8_t device_number = DEVICE;
      send_response(DEVICE_NUMBER, &device_number, 1);
      break;
    }
    default:
      return;
    }
  }  
}


void setup() {
  pinMode(enable_pin, OUTPUT);
  pinMode(step_pin, OUTPUT);
  pinMode(dir_pin, OUTPUT);

  Serial.begin(baud_rate);
}

unsigned long t1 = micros();
unsigned long  period = 1000;
void loop() {
  
  if(Serial.available() > 0)
  {
    const int read_char = 10;
    uint8_t recv[read_char] = {0};
    int length = Serial.readBytesUntil(MESSAGE_END, &recv[0], read_char) + 1;
    recv[length] = MESSAGE_END; // Add terminator because it is not read from readBytesUntil
    serial_handler(&recv[0], length);
  }
  if(micros()-t1 >= period && pump->pump_state == pump->RUNNING)
  {
    digitalWrite(step_pin, HIGH);
    delayMicroseconds(8);
    digitalWrite(step_pin, LOW);
  }

}