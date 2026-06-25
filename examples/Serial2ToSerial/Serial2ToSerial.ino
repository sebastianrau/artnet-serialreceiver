#include <ArtNetSerialReceiver.h>

constexpr uint32_t DEBUG_BAUD = 115200;
constexpr uint32_t ARTNET_SERIAL_BAUD = 921600;
constexpr uint16_t MAX_RX_BYTES = 128;

void onPacket(uint8_t* buffer, uint16_t size);
void onOverflow(uint16_t droppedBytes);

ArtNetSerialReceiver receiver(onPacket, onOverflow);

void setup() {
  Serial.begin(DEBUG_BAUD);
  Serial2.begin(ARTNET_SERIAL_BAUD);

  while (!Serial) {
    delay(10);
  }

  Serial.println("ArtNetSerialReceiver Serial2 example started");
}

void loop() {
  receiver.update(Serial2, MAX_RX_BYTES);
}

void onPacket(uint8_t* buffer, uint16_t size) {
  (void)buffer;

  Serial.print("Art-Net packet received, size: ");
  Serial.println(size);
}

void onOverflow(uint16_t droppedBytes) {
  Serial.print("RX overflow, dropped bytes: ");
  Serial.println(droppedBytes);
}
