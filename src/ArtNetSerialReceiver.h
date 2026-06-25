#pragma once

#include <Arduino.h>

class ArtNetSerialReceiver {
public:
    using PacketCallback = void (*)(uint8_t* buffer, uint16_t size);
    using OverflowCallback = void (*)(uint16_t droppedBytes);


    static constexpr uint16_t BUFFER_SIZE = 600;
    static constexpr uint8_t  ARTNET_HEADER_SIZE = 8;
    static constexpr uint16_t ARTDMX_MIN_SIZE = 18;
    static constexpr uint16_t ARTDMX_MAX_SIZE = 18 + 512;

    explicit ArtNetSerialReceiver(PacketCallback callback, OverflowCallback ofCb = nullptr);

    void update(Stream& serial, uint16_t maxBytes = 128);
    bool pushFromISR(uint8_t b);
    void parse(uint8_t maxPackets);

    void reset();

private:
    uint8_t ring[BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    

    PacketCallback packetCallback;
    OverflowCallback overflowCallback;

    uint16_t dropped = 0;
    static const uint8_t ARTNET_ID[ARTNET_HEADER_SIZE];

    bool push(uint8_t byte, bool notifyOverflow = true);
    uint8_t peek(uint16_t index) const;
    void copy(uint8_t* dst, uint16_t len) const;
    void drop(uint16_t bytes);

    bool headerMatches() const;
};
