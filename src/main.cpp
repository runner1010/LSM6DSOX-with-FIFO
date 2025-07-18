#include <Arduino.h>
#include <Wire.h>

#define LSM6DSOX_ADDR 0x6A // Default I2C address

// Register addresses
#define CTRL3_C             0x12
#define CTRL1_XL            0x10
#define CTRL2_G             0x11
#define CTRL10_C            0x19
#define FUNC_CFG_ACCESS     0x01
#define FIFO_CTRL1          0x07
#define FIFO_CTRL2          0x08
#define FIFO_CTRL3          0x09
#define FIFO_CTRL4          0x0A
#define FIFO_STATUS1        0x3A
#define FIFO_STATUS2        0x3B
#define FIFO_DATA_OUT_TAG   0x78

// Embedded function registers
#define EMB_FUNC_EN_A       0x04
#define EMB_FUNC_EN_B       0x05
#define SENSOR_HUB_ENABLE   0x14

void writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(LSM6DSOX_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg) {
    Wire.beginTransmission(LSM6DSOX_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(LSM6DSOX_ADDR, 1);
    return Wire.read();
}

void setupFIFO() {
    // Software reset
    writeRegister(CTRL3_C, 0x01);
    delay(100);

    // Set accelerometer ODR = 6667 Hz, FS = ±4g (FS_XL = 0b10 → 0xBC)
    writeRegister(CTRL1_XL, 0xBC);

    // Disable gyro
    writeRegister(CTRL2_G, 0x00);

    // Disable timestamp
    uint8_t ctrl10 = readRegister(CTRL10_C);
    ctrl10 &= ~(1 << 5); // TIMESTAMP_EN = 0
    writeRegister(CTRL10_C, ctrl10);

    // Disable embedded functions and sensor hub
    writeRegister(FUNC_CFG_ACCESS, 0x80); // Enable embedded reg access
    writeRegister(EMB_FUNC_EN_A, 0x00);   // Disable step counter, tilt, etc.
    writeRegister(EMB_FUNC_EN_B, 0x00);
    writeRegister(SENSOR_HUB_ENABLE, 0x00); // Disable sensor hub
    writeRegister(FUNC_CFG_ACCESS, 0x00);   // Exit embedded access

    // FIFO configuration
    writeRegister(FIFO_CTRL1, 0x20); // FIFO threshold = 32 (LSB)
    writeRegister(FIFO_CTRL2, 0x00); // No MSB, no decimation
    writeRegister(FIFO_CTRL3, 0x01); // FIFO XL only
    writeRegister(FIFO_CTRL4, 0x01); // Continuous mode
}

void setup() {
    Wire.begin(8, 9); // SDA = GPIO8, SCL = GPIO9
    Serial.begin(115200);
    delay(100);

    Serial.println("Initializing LSM6DSOX FIFO...");
    setupFIFO();
    Serial.println("Setup complete.");
}

void loop() {
    uint16_t fifo_level = readRegister(FIFO_STATUS1) | ((readRegister(FIFO_STATUS2) & 0x03) << 8);

    if (fifo_level > 0) {
        Serial.print("FIFO Samples: ");
        Serial.println(fifo_level);

        for (uint16_t i = 0; i < fifo_level; i++) {
            Wire.beginTransmission(LSM6DSOX_ADDR);
            Wire.write(FIFO_DATA_OUT_TAG);
            Wire.endTransmission(false);
            Wire.requestFrom(LSM6DSOX_ADDR, 7);

            if (Wire.available() < 7) {
                Serial.println("ERROR: Not enough bytes read from FIFO!");
                break;
            }

            uint8_t tag = Wire.read();
            int16_t x = Wire.read() | (Wire.read() << 8);
            int16_t y = Wire.read() | (Wire.read() << 8);
            int16_t z = Wire.read() | (Wire.read() << 8);

            Serial.print("TAG: 0x");
            Serial.print(tag, HEX);
            Serial.print(" ");

            // Decode tag
            switch (tag & 0x0F) {
                case 0x01: Serial.print("ACCEL: "); break;
                case 0x02: Serial.print("GYRO: "); break;
                case 0x03: Serial.print("TEMPERATURE: "); break;
                case 0x04: Serial.print("TIMESTAMP: "); break;
                case 0x05: Serial.print("STEP_COUNTER: "); break;
                case 0x06: Serial.print("SENSOR_HUB: "); break;
                case 0x07: Serial.print("EXT_SENSOR_0: "); break;
                case 0x08: Serial.print("EXT_SENSOR_1: "); break;
                case 0x09: Serial.print("EXT_SENSOR_2: "); break;
                case 0x0A: Serial.print("EXT_SENSOR_3: "); break;
                case 0x0F: Serial.print("ERROR: "); break;
                default:
                    Serial.print("UNKNOWN TAG: 0x");
                    Serial.print(tag & 0x0F, HEX);
                    Serial.print(" ");
                    break;
            }

            // Convert raw to g (±4g FS → 4.0 / 32768.0)
            float scale = 4.0 / 32768.0;
            float xf = x * scale;
            float yf = y * scale;
            float zf = z * scale;

            Serial.print("X: "); Serial.print(xf, 5); Serial.print(" g");
            Serial.print(" Y: "); Serial.print(yf, 5); Serial.print(" g");
            Serial.print(" Z: "); Serial.print(zf, 5); Serial.println(" g");
        }
    }

    delay(10);
}
