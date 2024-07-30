
#define USER_SETUP_ID 303

#define TFT_PARALLEL_8_BIT

#define ILI9486_DRIVER

// ESP32 S3 pins used for the parallel interface TFT
#define TFT_CS   9
#define TFT_DC  14  // Data Command control pin - must use a GPIO in the range 0-31
#define TFT_RST 21

#define TFT_WR  19  // Write strobe control pin - must use a GPIO in the range 0-31
#define TFT_RD  20

#define TFT_D0   1  // Must use GPIO in the range 0-31 for the data bus
#define TFT_D1   2  // so a single register write sets/clears all bits
#define TFT_D2   3
#define TFT_D3   4
#define TFT_D4   5
#define TFT_D5   6
#define TFT_D6   7
#define TFT_D7   8


#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT