#include <Wire.h>
#include <SD.h>

// this class is a circular floating point queue of 32 floats
// it's used to cache the sensor data from the LPS22HB so it
// can be synced up with faster LSM9DS1 sensor.
class cfque
{
public:
  bool empty() { return (!full && (head == tail)); }
  void push(float f)
  {
    buffer[head] = f;
    if (full)
    {
      tail = (tail + 1) % 32;
    }
    head = (head + 1) % 32;
    full = (head == tail);
  }
  float pop()
  {
    if (empty())
    {
      return NAN;
    }
    float val = buffer[tail];
    full = false;
    tail = (tail + 1) % 32;
    return val;
  }
  float front()
  {
    if (empty())
    {
      return NAN;
    }
    return buffer[tail];
  }

private:
  float buffer[32];
  int head = 0;
  int tail = 0;
  bool full = false;
} temperature, pressure, sample_time;

// I2C addresses of our sensors
const uint8_t LSM9DS1_ADDRESS = 0x6b;
const uint8_t LPS22HB_ADDRESS = 0x5c;

// Create the struct used to hold sensor samples and its array
// large enough to hold all samaples from a full FIFO buffer read.
struct record
{
  float ms;
  float xl[3];
  float g[3];
  float p;
  float t;
};
const uint16_t RECORD_SIZE = sizeof(record);
record records[32];

uint32_t read_time[2]{0, 0};      // the start time of reading a FIFO buffer
uint32_t last_read_time[2]{0, 0}; // the last time we started reading a FIFO buffer
uint32_t next_read_check = 0;     // the next time the FIFO statuses should be checked for stored samples
float sample_frequency = 0;       // the calculated time between each sample in the FIFO buffer

uint8_t fifo_src; // holds the FIFO status
uint8_t fss;      // holds the number of samples in the FIFO buffer

File datafile;

void setup()
{

  // Wait for an SD card to be inserted.
  // An orange light will blink every 2
  // seconds until an SD card is detected.
  while (!SD.begin(10))
  {
    delay(2000);
  };
  SD.remove("datalog.dat");
  datafile = SD.open("datalog.dat", FILE_WRITE);

  // Start Wire1 at 10Mhz
  // The datasheet I2C table states support for 100kHz (standard mode)
  // and 400kHz (fast mode) but it looks like 10MHz (fast mode plus)
  // and 34Mhz (high speed mode) also work.
  Wire1.begin();
  Wire1.setClock(34000000);

  // Initialize the red and green LEDs.
  pinMode(LEDR, OUTPUT);
  digitalWrite(LEDR, HIGH);
  pinMode(LEDG, OUTPUT);
  digitalWrite(LEDG, HIGH);
  pinMode(LEDB, OUTPUT);
  digitalWrite(LEDB, HIGH);

  writeRegister(LPS22HB_ADDRESS, 0x10, 0b01010000); // set ODR to 75 Hz
  writeRegister(LPS22HB_ADDRESS, 0x11, 0b01010000); // enable FIFO memory
  writeRegister(LPS22HB_ADDRESS, 0x14, 0b11000000); // enable dynamic-stream mode

  writeRegister(LSM9DS1_ADDRESS, 0x2e, 0b11000000); // enable continuous FIFO mode
  writeRegister(LSM9DS1_ADDRESS, 0x23, 0b00000010); // enable FIFO memory
  writeRegister(LSM9DS1_ADDRESS, 0x20, 0b10001000); // set XL to 16 g @ 238 Hz
  writeRegister(LSM9DS1_ADDRESS, 0x10, 0b10011000); // set G to 2000 dps @ 238 Hz
}

void loop()
{

  if (millis() > next_read_check)
  {
    // We only want to check how many stored samples
    // are in the FIFO buffer after we allow it to gather
    // some samples. The loop is much faster than the sample
    // rate of 238 so we give it some time to fill the buffer.
    // 134 was calculated as the time it takes to fill the
    // LSM9DS1 FIFO buffer using the ODR of 238, which is the
    // faster of the two sensors we are using:
    // [ floor((1000 / 238) * 32) = 134 ]
    next_read_check += 134;

    // handle the LPS22HB sensor
    fifo_src = readRegister(LPS22HB_ADDRESS, 0x26); // read the FIFO status byte
    fss = fifo_src & 0b00111111;                    // fss is the number of samples available in the FIFO buffer
    if (fss > 0)
    {
      // check the overrun bit... if it's overrun, blink red, otherwise blink blue.
      // this should never blink red...
      if (fifo_src & 0b01000000)
      {
        fss = 32;
        digitalWrite(LEDR, LOW);
      }
      else
      {
        digitalWrite(LEDB, LOW);
      }
      last_read_time[0] = read_time[0]; // save the last read time
      read_time[0] = millis();          // update the current read time to now
      // Estimate the average time it took to store each sample in the FIFO buffer.
      sample_frequency = (read_time[0] - last_read_time[0]) / (float)fss;
      // we can read all the data using burst mode because
      // the total FIFO buffer is less that 256 bytes.
      uint8_t data[5 * 32];
      if (readRegisters(LPS22HB_ADDRESS, 0x28, (uint8_t *)data, fss * 5))
      {
        for (int i = 0; i < fss; i++)
        {
          // add the data to our queue for use later
          // the conversions come directly from the datasheet
          pressure.push((data[0] | (data[1] << 8) | (data[2] << 16)) / 4096.0); // three bytes... really? why?
          temperature.push((data[3] | (data[4] << 8)) / 100.0);
          sample_time.push(round(last_read_time[0] + sample_frequency * i));
        }
      }
      digitalWrite(LEDR, HIGH);
      digitalWrite(LEDB, HIGH);
    }

    // handle the LSM9DS1 sensor
    fifo_src = readRegister(LSM9DS1_ADDRESS, 0x2f); // read the FIFO status byte
    fss = fifo_src & 0b00111111;                    // fss is the number of samples available in the FIFO buffer
    if (fss > 0)
    {
      // check the overrun bit... if it's overrun, blink red, otherwise blink green.
      if (fifo_src & 0b01000000)
      {
        fss = 32;
        digitalWrite(LEDR, LOW);
      }
      else
      {
        digitalWrite(LEDG, LOW);
      }
      last_read_time[1] = read_time[1]; // save the last read time
      read_time[1] = millis();          // update the current read time to now
      // Estimate the average time it took to store each sample in the FIFO buffer.
      // Note that this is not accurate if there was an overrun, so it's
      // important that we don't see a red light!
      sample_frequency = (read_time[1] - last_read_time[1]) / (float)fss;
      for (int i = 0; i < fss; i++)
      {
        int16_t data[12];
        // Use burst to grab the data from XL and G registers at once.
        // I tried grabbing all the samples at once, but it seems there is
        // a limit of 265 bytes and we would need 384 (6 int16_t * 32).
        if (readRegisters(LSM9DS1_ADDRESS, 0x18, (uint8_t *)data, sizeof(data)))
        {
          // These converstions are from the datasheet, but the math doesn't
          // add up for XL, it has a max of 24, but should only be 16...
          records[i].ms = round(last_read_time[1] + sample_frequency * i);
          records[i].g[0] = data[0] * 70.0 / 1000.0;
          records[i].g[1] = data[1] * 70.0 / 1000.0;
          records[i].g[2] = data[2] * 70.0 / 1000.0;
          records[i].xl[0] = data[3] * 0.732 / 1000.0;
          records[i].xl[1] = data[4] * 0.732 / 1000.0;
          records[i].xl[2] = data[5] * 0.732 / 1000.0;
          records[i].p = NAN;
          records[i].t = NAN;
          // add the sensor data from the LPS22HB to the current record if
          // the LSM9DS1 sensor data is greater than or equal to the sample
          // time to try to align them.
          if (!sample_time.empty())
          {
            if (sample_time.front() <= records[i].ms)
            {
              // move this sample set from the queue to the record
              sample_time.pop();
              records[i].p = pressure.pop();
              records[i].t = temperature.pop();
            }
          }
        }
      }
      // We need to drop the first set of samples as suggested when using FIFO.
      // The last time will be zero only for the first loop.
      digitalWrite(LEDR, HIGH);
      digitalWrite(LEDG, HIGH);
      if ((last_read_time[0] + last_read_time[1]) > 0)
      {
        datafile.write((byte *)records, RECORD_SIZE * fss); // write our stuct array to the file
        datafile.flush();                                   // commit the changes (there might be a more correct term, but I'm an Oracle database guy)
      }
    }
  }
}

// This function writes a byte to a register of an I2C devices.
// This is based on the Arduino libs for the LSM9DS1.
uint8_t writeRegister(uint8_t slaveAddress, uint8_t regAddress, uint8_t regValue)
{
  Wire1.beginTransmission(slaveAddress);
  Wire1.write(regAddress);
  Wire1.write(regValue);
  if (Wire1.endTransmission() != 0)
  {
    return 0;
  }
  return 1;
}

// This function reads a byte from a register of an I2C devices.
// This is based on the Arduino libs for the LSM9DS1.
uint8_t readRegister(uint8_t slaveAddress, uint8_t regAddress)
{
  Wire1.beginTransmission(slaveAddress);
  Wire1.write(regAddress);
  if (Wire1.endTransmission() != 0)
  {
    return 0;
  }
  if (Wire1.requestFrom(slaveAddress, 1) != 1)
  {
    return 0;
  }
  return Wire1.read();
}

// This function reads multiple bytes starting at a given register of an I2C devices.
// This is based on the Arduino libs for the LSM9DS1.
// The device must support automatic incrementing for this to work.
uint8_t readRegisters(uint8_t slaveAddress, uint8_t regAddress, uint8_t *data, size_t length)
{
  Wire1.beginTransmission(slaveAddress);
  Wire1.write(regAddress);
  if (Wire1.endTransmission() != 0)
  {
    return -1;
  }
  if (Wire1.requestFrom(slaveAddress, length) != length)
  {
    return 0;
  }
  for (size_t i = 0; i < length; i++)
  {
    *data++ = Wire1.read();
  }
  return 1;
}
