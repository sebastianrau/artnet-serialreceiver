# ArtNetSerialReceiver

Arduino library for receiving Art-Net packets from a serial stream.

This library is designed for use with Ethernet-to-Serial modules such as the WCH CH9120. It reads bytes from an Arduino `Stream`, stores them in a ring buffer, detects Art-Net packets, and calls a user-defined callback when a valid packet is received.

## Features

- Receive Art-Net data from any Arduino `Stream`
- Ring buffer based parser
- Packet callback for received packets
- Optional overflow callback
- Byte input helper for interrupt-driven receivers with `pushFromISR()`
- Configurable maximum bytes per update call
- Lightweight and suitable for embedded systems

## Installation

Copy the library folder into your Arduino libraries directory:

```text
Documents/Arduino/libraries/ArtNetSerialReceiver/
```

## Basic Example

The Arduino IDE example is available at:

```text
File > Examples > ArtNetSerialReceiver > Serial2ToSerial
```

```cpp
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
```

## Ethernet-to-Serial Stream Example

```cpp
#include <ArtNetSerialReceiver.h>

constexpr uint16_t MAX_RX_BYTES = 128;
constexpr uint16_t ART_NET_BUFFER_SIZE = 530;

void newPacketReceived(uint8_t* buffer, uint16_t size);
void overflowError(uint16_t droppedBytes);

ArtNetSerialReceiver ArtnetRx(newPacketReceived, overflowError);

void loop() {
  ArtnetRx.update(Ethernet.GetStream(), MAX_RX_BYTES);
}

void newPacketReceived(uint8_t* buffer, uint16_t size) {
  if (size > 0 && size <= ART_NET_BUFFER_SIZE) {
    memcpy(artnet.getPacketBuffer(), buffer, size);
    artnet.read(size);
  }
}

void overflowError(uint16_t droppedBytes) {
  (void)droppedBytes;
  Serial.println("RX OVERFLOW ERROR");
}
```

The callback buffer is reused by the receiver, so copy or consume the packet before returning from `newPacketReceived()`.

## API

### Constructor

```cpp
ArtNetSerialReceiver receiver(packetCallback, overflowCallback);
```

Creates a receiver instance.

`packetCallback` is called when a valid Art-Net packet is received.

`overflowCallback` is optional and is called when bytes are dropped because the internal ring buffer is full.

## Callback Types

```cpp
using PacketCallback = void (*)(uint8_t* buffer, uint16_t size);
using OverflowCallback = void (*)(uint16_t droppedBytes);
```

## `update(Stream& serial, uint16_t maxBytes = 128)`

Reads up to `maxBytes` bytes from the given stream and stores them in the internal ring buffer.

```cpp
receiver.update(Serial1);
```

## `pushFromISR(uint8_t b)`

Pushes one byte into the ring buffer.

This helper can be used when bytes are received from an interrupt routine. It does not call `overflowCallback`, because callbacks often use APIs such as `Serial` that are unsafe from interrupt context.

```cpp
receiver.pushFromISR(byteValue);
```

Returns `true` if the byte was stored without overwriting older data. Returns `false` if the ring buffer was already full; in that case, the oldest byte is dropped and the new byte is still stored.

Call `parse()` from the main loop, not from inside the interrupt routine.

## `parse(uint8_t maxPackets)`

Parses up to `maxPackets` Art-Net packets from the internal buffer.

```cpp
receiver.parse(4);
```

## `reset()`

Clears the ring buffer and resets the parser state.

```cpp
receiver.reset();
```

## Constants

```cpp
static constexpr uint16_t BUFFER_SIZE = 600;
static constexpr uint8_t  ARTNET_HEADER_SIZE = 8;
static constexpr uint16_t ARTDMX_MIN_SIZE = 18;
static constexpr uint16_t ARTDMX_MAX_SIZE = 18 + 512;
```

## Notes

The receiver expects Art-Net packets beginning with the standard Art-Net ID:

```text
Art-Net\0
```

The maximum ArtDMX packet size is `530` bytes.

The default buffer size is `600` bytes, which is enough for one full ArtDMX packet plus additional incoming data.

For high baud rates such as `921600`, call `update()` and `parse()` frequently in your main loop.

ArtDMX and ArtNzs packets are accepted when their payload length is even and between `2` and `512` bytes.
