/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2014 by Ray Wang (ray@opensprinkler.com)
 *
 * GPIO functions
 * Feb 2015 @ OpenSprinkler.com
 *
 * This file is part of the OpenSprinkler library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>. 
 */

#include "gpio.h"

#if defined(ARDUINO)

#elif defined(OSPI) || defined(OSBO)

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

byte _digitalRead(int pin);
void _pinMode(int pin, byte mode);
void _digitalWrite(int pin, byte value);

#ifdef DIRECT_IO

#include "defines.h"

#define NUM_DIRECT_IO 17
// logical to physical pin number mappings - same as WiringPi
byte pi_r1_direct_io[NUM_DIRECT_IO] = {
  17, 18, 21, 22, 23, 24, 25, 4, 0, 1, 8, 7, 10, 9, 11, 14, 15
};
byte pi_r2_direct_io[NUM_DIRECT_IO] = {
  17, 18, 27, 22, 23, 24, 25, 4, 2, 3, 8, 7, 10, 9, 11, 14, 15
};
byte* pin_direct_io = pi_r2_direct_io;
int initialized = 0;
int pin_sr_latch = -1;
int pin_sr_clock = -1;
int pin_sr_oe = -1;
int pin_sr_data = -1;
int station_num = MAX_NUM_STATIONS - 1;
byte station_state[MAX_NUM_STATIONS];

void pinMode(int pin, byte mode) {
  DEBUG_PRINTLN("Initializing in pin mode");
  if (!initialized) {
    for (int i = 0; i < NUM_DIRECT_IO; ++i) {
      _pinMode(pin_direct_io[i], OUTPUT);
    }
  }
  DEBUG_PRINTLN("Done initializing in pin mode");
  initialized = 1;
}

// PIN_SR_LATCH -> LOW
// For each pin:
//    PIN_SR_CLOCK -> LOW
//    PIN_SR_DATA -> 0/1
//    PIN_SR_CLOCK -> HIGH
// PIN_SR_LATCH -> HIGH

void digitalWrite(int pin, byte value) {

  switch (pin) {
    case PIN_SR_DATA:
    pin_sr_data = value;
    DEBUG_PRINT("PIN_SR_DATA: value = ");
    DEBUG_PRINTLN(value)
    break;
    case PIN_SR_DATA_ALT:
    pin_sr_data = value;
    DEBUG_PRINT("PIN_SR_DATA_ALT: value = ");
    DEBUG_PRINTLN(value)
    break;
    case PIN_SR_OE:
    pin_sr_oe = value;
    DEBUG_PRINT("PIN_SR_OE: value = ");
    DEBUG_PRINTLN(value)
    break;
    case PIN_SR_LATCH:
    DEBUG_PRINT("PIN_SR_LATCH: value = ");
    DEBUG_PRINTLN(value)
    pin_sr_latch = value;
    if (pin_sr_latch == 0) {
      DEBUG_PRINTLN("PIN_SR_LATCH set low, resetting station_num");
      station_num = MAX_NUM_STATIONS - 1;
    } else {
      DEBUG_PRINTLN("PIN_SR_LATCH set high, writing station states to GPIO");
      for (int i = 0; i < NUM_DIRECT_IO; ++i) {
        int state = 0;
        if (station_state[i] == 0) {
          state = 1;
        }
        _digitalWrite(pin_direct_io[i], state);
      }
    }
    break;
    case PIN_SR_CLOCK:
    DEBUG_PRINT("PIN_SR_CLOCK: value = ");
    DEBUG_PRINTLN(value)
    pin_sr_clock = value;
    if (pin_sr_clock > 0) {
      if (pin_sr_latch == 0 && pin_sr_oe == 0) {
        DEBUG_PRINT("PIN_SR_CLOCK set high, setting station ");
        DEBUG_PRINT(station_num);
        DEBUG_PRINT(" to state ");
        DEBUG_PRINTLN(pin_sr_data);
        station_state[station_num] = pin_sr_data;
        --station_num;
      }
    }
    break;
    default:
    DEBUG_PRINT("digitalWrite pin not recognized: pin = ");
    DEBUG_PRINT(pin);
    DEBUG_PRINT(", value = ");
    DEBUG_PRINTLN(value)
    break;
  }
}

byte digitalRead(int pin) {
  DEBUG_PRINT("digitalRead: pin = ");
  DEBUG_PRINTLN(pin);
  return 0;
}

#else

void pinMode(int pin, byte mode) {
  _pinMode(pin, mode);
}
void digitalWrite(int pin, byte value) {
  _digitalWrite(pin, value);
}
byte digitalRead(int pin) {
  return _digitalReal(pin);
}

#endif

static byte GPIOExport(int pin) {
  DEBUG_PRINT("GPIOExport, pin = ");
  DEBUG_PRINTLN(pin);
#define BUFFER_MAX 3
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/export", O_WRONLY);
  if (-1 == fd) {
    DEBUG_PRINTLN("failed to open export for writing");
    return 0;
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return 1;

}

void _pinMode(int pin, byte mode) {
  static const char s_directions_str[] = "in\0out";

#define DIRECTION_MAX 35
  char path[DIRECTION_MAX];
  int fd;

  snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);

  struct stat st;
  if(stat(path, &st)) {
    if (!GPIOExport(pin)) return;
  }

  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    DEBUG_PRINTLN("failed to open gpio direction for writing");
    return;
  }

  if (-1 == write(fd, &s_directions_str[INPUT == mode ? 0 : 3], INPUT == mode ? 2 : 3)) {
    DEBUG_PRINTLN("failed to set direction");
    return;
  }

  close(fd);
  return;
}

byte _digitalRead(int pin) {
#define VALUE_MAX 30
  char path[VALUE_MAX];
  char value_str[3];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_RDONLY);
  if (-1 == fd) {
    DEBUG_PRINTLN("failed to open gpio value for reading");
    return 0;
  }

  if (-1 == read(fd, value_str, 3)) {
    DEBUG_PRINTLN("failed to read value");
    return 0;
  }

  close(fd);
  return(atoi(value_str));
}

void _digitalWrite(int pin, byte value) {
  static const char s_values_str[] = "01";

  DEBUG_PRINT("_digitalWrite: pin = ");
  DEBUG_PRINT(pin);
  DEBUG_PRINT(", value = ");
  DEBUG_PRINTLN(value)

  char path[VALUE_MAX];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    DEBUG_PRINTLN("failed to open gpio value for writing");
    return;
  }

  if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
    DEBUG_PRINTLN("failed to write value");
    DEBUG_PRINTLN(pin);
    return;
  }

  close(fd);
}

#else

void pinMode(int pin, byte mode) {
}
void digitalWrite(int pin, byte value) {
}
byte digitalRead(int pin) {
  return 0;
}

#endif
