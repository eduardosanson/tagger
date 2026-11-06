// Substituto mínimo do FastLED para a barra de 4 LEDs WS2812.
//
// Por que existe: o FastLED 3.9 no núcleo ESP32 3.x (IDF 5) não consegue subir
// o canal RMT5 neste hardware — "ChannelManager: Timeout ... waiting for READY"
// em loop, mesmo com o IR desligado. Como a barra tem 4 LEDs e só precisa de
// cor sólida, um driver próprio no RMT5 resolve sem depender de terceiros.
//
// A API imita a do FastLED usada pelo projeto, então o resto do código não muda:
//   CRGB leds[N]; leds[i].setColorCode(0xFF0000); FastLED.show();

#ifndef _LEDS_SHIM_H_
#define _LEDS_SHIM_H_

#include <stdint.h>
#include <string.h>

#define NEOPIXEL 0
#define APA102   1
#define BGR      0
#define RGB      1

struct CRGB {
    uint8_t r = 0, g = 0, b = 0;

    void setRGB(uint8_t rr, uint8_t gg, uint8_t bb) { r = rr; g = gg; b = bb; }

    void setColorCode(uint32_t code) {
        r = (code >> 16) & 0xFF;
        g = (code >> 8) & 0xFF;
        b = code & 0xFF;
    }

    CRGB& operator=(uint32_t code) { setColorCode(code); return *this; }
};

class FastLEDShim {
  public:
    template <int CHIPSET, int PIN, int EXTRA = 0, int ORDER = 0>
    void addLeds(CRGB* data, int count) {
        _data = data;
        _count = count;
        _pin = PIN;
    }

    void setBrightness(uint8_t value) { _brightness = value; }

    // Definido em leds_shim.cpp (precisa do driver RMT)
    void show();

    void clear(bool write = false) {
        if (_data) memset((void*) _data, 0, sizeof(CRGB) * _count);
        if (write) show();
    }

  private:
    CRGB*   _data = nullptr;
    int     _count = 0;
    int     _pin = -1;
    uint8_t _brightness = 255;
};

extern FastLEDShim FastLED;

#endif // _LEDS_SHIM_H_
