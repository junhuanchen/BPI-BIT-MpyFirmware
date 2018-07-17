
#include "modesp.h"

#define LedPower 2
#define LedCtrl  4

#define FillArea(Led, Size, View, Color) for(uint32_t x = 0, t = View; x < Size; x++) memset(&Led[x * 3], 0, 3), Led[x * 3 + Color[x / 5]] = (t & 1) * 5, t >>= 1

#define FillNext(Next, View) View <<= 5, View |= Next

#define FillView(Led, Pos, View) memcpy(Led + Pos*5, View, 5)

const uint8_t ViewLib[][5] = {
    {0x00, 0x1F, 0x11, 0x1F, 0x00},
    {0x00, 0x12, 0x1F, 0x10, 0x00},
    {0x00, 0x1D, 0x15, 0x17, 0x00},
    {0x00, 0x15, 0x15, 0x1F, 0x00},
    {0x00, 0x07, 0x04, 0x1F, 0x00},
    {0x00, 0x17, 0x15, 0x1D, 0x00},
    {0x00, 0x1F, 0x15, 0x1D, 0x00},
    {0x00, 0x01, 0x1D, 0x03, 0x00},
    {0x00, 0x1F, 0x15, 0x1F, 0x00},
    {0x00, 0x17, 0x15, 0x1F, 0x00},
    {0x00, 0x1F, 0x05, 0x1F, 0x00},
    {0x00, 0x1F, 0x15, 0x0A, 0x00},
    {0x00, 0x0E, 0x11, 0x11, 0x00},
    {0x00, 0x1F, 0x11, 0x0E, 0x00},
    {0x00, 0x1F, 0x15, 0x15, 0x00},
    {0x00, 0x1F, 0x05, 0x05, 0x00},
};

inline void BitShowName(uint8_t buf[], uint8_t len)
{
    uint8_t buffer[25 * 3] = { };
    uint32_t View = 0;
    uint8_t Color[5] = {}, Temp = 0;

    for(int i = 0; i < len; i++)
    {
        FillArea(buffer, 25, View, Color);

        esp_neopixel_write(LedCtrl, buffer, sizeof(buffer), 1);

        FillNext(buf[i], View);

        if(0 == (i % 5)) Temp = (Temp + 1) % 3;

        for(int i = 4; i > 0; i--) Color[i] = Color[i - 1];

        Color[0] = Temp;
        
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

inline void BitLedOpen()
{
    gpio_set_direction(LedPower, GPIO_MODE_OUTPUT);
    gpio_set_level(LedPower, 1);
    gpio_set_direction(LedCtrl, GPIO_MODE_OUTPUT);
}

inline void BitLedExit()
{
    gpio_set_level(LedPower, 0);
    gpio_reset_pin(LedPower);
    gpio_reset_pin(LedCtrl);
}

inline void BitRunView(uint8_t data[])
{
    BitLedOpen();
    uint8_t result[5 * 5] = { 0 };
    for(int i = 0; i < 4; i++) FillView(result, i, ViewLib[data[i]]);
    BitShowName(result, sizeof(result));
    // BitLedExit();
}

