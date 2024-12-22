/***************************************************************************

    main.c

    stub for unix main.

    Copyright (c) 2024-2024, lixiasong.

***************************************************************************/

#define _GNU_SOURCE
#include <stddef.h>
#include "options.h"
#include "clifront.h"
#include "render.h"
#include <stdint.h>

#include <stdint.h>
#include <stdio.h>
#include <sched.h>
#include <gpiod.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <string.h>

//============================================================
//  LCD
//============================================================
#define MAX_SPI_BUF 4096

#define INPUT_KEY_A 19
#define INPUT_KEY_B 26
#define INPUT_KEY_D 13

#define LCD_DC   5  /* BCM.5引脚用来作为数据和命令切换引脚 */
#define LCD_REST 23 /* BCM/23引脚用来作为LCD重置 */

#define LCD_SCREEN_WIDTH  480 // 640 //480
#define LCD_SCREEN_HEIGHT 480 // 480 //320

#define LCD_SPI_MAX_SPEED      50000000
#define LCD_SPI_MODE           SPI_MODE_0
#define LCD_SPI_BITS_PER_WORD  8

#define LCD_SPI_MAX_CMD        128
struct spi_data {
    uint8_t cmd;
    uint8_t data[LCD_SPI_MAX_CMD];
    size_t data_len;
};

struct rgb565 {
    uint16_t r:5;
    uint16_t g:6;
    uint16_t b:5;
};

#include <alsa/asoundlib.h>
#include <math.h>

#define goto_error_if_fail(p)                           \
  if (!(p)) {                                           \
    printf("%s:%d " #p "\n", __FUNCTION__, __LINE__);   \
    goto error;                                         \
  }

snd_pcm_uframes_t frame_num;
snd_pcm_t* g_pcm_handle = NULL;

snd_pcm_t* device_create(void)
{
  snd_pcm_t* handle;		    /* pcm句柄 */
  snd_pcm_hw_params_t* params;	/* pcm参数 */
  int rc = -1;

  /* 打开设备 */
  rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK,0);
  goto_error_if_fail(rc >= 0);

  /* 初始化pcm属性 */
  snd_pcm_hw_params_alloca(&params);
  //goto_error_if_fail(rc >= 0);
  snd_pcm_hw_params_any(handle, params);

   /* 设置为交错模式 */
  snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

  /* 设置为双声道 */
  snd_pcm_hw_params_set_channels(handle, params, 2);

  /* 设置数据格式: 有符号16位格式 */
  snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16);

  /* 设置采样率 */
  int val = 48000;
  snd_pcm_hw_params_set_rate_near(handle,params,&val,0);


/* 将参数写入设备 */
  rc = snd_pcm_hw_params(handle, params);
  goto_error_if_fail(rc >= 0)

  snd_pcm_hw_params_get_period_size(params, &frame_num, 0);
  printf("frame num: %d\n", frame_num);

  return handle;
error:
  snd_pcm_hw_params_free(params);
  snd_pcm_close(handle);
  return NULL;
}

int device_play(snd_pcm_t* pcm_handle, const void* data, int data_size)
{
    snd_pcm_uframes_t frame_size;	  /* 一帧的大小 */
    unsigned int channels = 2;	  /* 双声道 */
    unsigned int format_sbits = 16; /* 数据格式: 有符号16位格式 */
    /* 获取一个播放周期的帧数 */
    frame_size = channels * format_sbits / 8;

    snd_pcm_uframes_t i;
    for(i = 0; i < data_size;)
    {
        int rc = -1;
        /* 播放 */
        rc = snd_pcm_writei(pcm_handle, data + i, frame_num);
        goto_error_if_fail(rc >= 0 || rc == -EPIPE);
        if(rc == -EPIPE) {
            /* 让音频设备准备好接收pcm数据 */
            snd_pcm_prepare(pcm_handle);
        } else {
            i += frame_num * frame_size;
        }
    }
    return 0;
error:
    return -1;
}

void device_destroy(snd_pcm_t* pcm_handle)
{
  snd_pcm_drain(pcm_handle);   /* 等待数据全部播放完成 */
  snd_pcm_close(pcm_handle);
}


struct gpiod_line *g_lcd_dc;
struct gpiod_line *g_lcd_reset;
int g_spi_lcd_fd = -1;

int lcd_spi_write(int fd, uint8_t *wbuf, size_t size)
{
    int ret = write(fd, wbuf, size);
    if (ret != size) {
        printf("failed to send data.\n");
    }
    return ret;
}

int lcd_spi_send_cmd(int fd, struct spi_data *spi_data)
{
    gpiod_line_set_value(g_lcd_dc, 0);
    lcd_spi_write(fd, &spi_data->cmd, 1);
    if (spi_data->data_len == 0) {
        return 0;
    }
    gpiod_line_set_value(g_lcd_dc, 1);
    lcd_spi_write(fd, spi_data->data, spi_data->data_len);
    return 0;
}

void lcd_spi_send_data(int fd, uint8_t *data, size_t len)
{
    gpiod_line_set_value(g_lcd_dc, 1);
    size_t pos = 0;
    int left_len = len;
    while (left_len > 0) {
        size_t send = MAX_SPI_BUF;
        if (left_len < MAX_SPI_BUF) {
            send = left_len;
        }
        lcd_spi_write(fd, data + pos, send);
        pos += send;
        left_len -= send;
    }
}

void lcd_set_window(int fd, uint16_t xstart, uint16_t ystart,
    uint16_t xend, uint16_t yend)
{
    struct spi_data data = { 0 };
    data.cmd = 0x2A;
    data.data[0] = xstart >> 8;
    data.data[1] = xstart & 0xFF;
    data.data[2] = xend >> 8;
    data.data[3] = xend & 0xFF;
    data.data_len = 4;
    lcd_spi_send_cmd(fd, &data);

    data.cmd = 0x2B;
    data.data[0] = ystart >> 8;
    data.data[1] = ystart & 0xFF;
    data.data[2] = yend >> 8;
    data.data[3] = yend & 0xFF;
    data.data_len = 4;
    lcd_spi_send_cmd(fd, &data);

    data.cmd = 0x2C;
    data.data_len = 0;
    lcd_spi_send_cmd(fd, &data);
}

void test_lcd(int fd)
{
    size_t size = LCD_SCREEN_WIDTH * LCD_SCREEN_HEIGHT;
    struct rgb565 *data = malloc(size * sizeof(struct rgb565));
    if (data == NULL) {
        printf("malloc failed\n");
        return;
    }

	size_t i;
    for (i = 0; i < size; i++) {
        if (i / LCD_SCREEN_WIDTH < 64) {
            data[i].r = 0;
            data[i].g = 0xFF;
            data[i].b = 0;
        } else {
            data[i].r = 0xFF;
            data[i].g = 0;
            data[i].b = 0;
        }
        uint16_t temp = *(uint16_t *)&data[i];
        *(uint16_t *)&data[i] = (((temp >> 8) & 0xFF) + ((temp & 0xFF) << 8));
    }
    lcd_set_window(fd, 0, 0, LCD_SCREEN_WIDTH - 1, LCD_SCREEN_HEIGHT - 1);
    lcd_spi_send_data(fd, (uint8_t *)data, size * sizeof(struct rgb565));
	free(data);
}

struct spi_data g_spi_data[] = {
    {0x11, {}, 0},
    {0x36, {0x48}, 1},
    {0x3A, {0x55}, 1},
    {0xF0, {0xC3}, 1},
    {0xF0, {0x96}, 1},
    {0xB4, {0x01}, 1},
    {0xB7, {0xC6}, 1},
    {0xB9, {0x02, 0xE0}, 2},
    {0xC0, {0x80, 0x07}, 2},
    {0xC1, {0x15}, 1},
    {0xC2, {0xA7}, 1},
    {0xC5, {0x07}, 1},
    {0xE8, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 8},
    {0xE0, {0xF0, 0x04, 0x0E, 0x03, 0x02, 0x13, 0x34, 0x44, 0x4A, 0x3A, 0x15, 0x15, 0x2F, 0x34}, 14},
    {0xE1, {0xF0, 0x0F, 0x16, 0x0C, 0x09, 0x05, 0x34, 0x33, 0x4A, 0x35, 0x11, 0x11, 0x2C, 0x32}, 14},
    {0xF0, {0x3C}, 1},
    {0xF0, {0x69}, 1},
    {0x21, {}, 0},
    {0x29, {}, 0},
    {0x36, {0x28}, 1},

#if 0
    {0x11, {}, 0},
    {0x36, {0x48}, 1},
    {0x3A, {0x55}, 1},
    {0xF0, {0xC3}, 1},
    {0xF0, {0x96}, 1},
    {0xB4, {0x01}, 1},
    {0xB7, {0xC6}, 1},
    {0xC0, {0x80, 0x45}, 2},
    {0xC1, {0x13}, 1},
    {0xC2, {0xA7}, 1},
    {0xC5, {0x0A}, 1},
    {0xE8, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 8},
    {0xE0, {0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30, 0x33, 0x47, 0x17, 0x13, 0x13, 0x2B, 0x31}, 14},
    {0xE1, {0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F, 0x33, 0x47, 0x38, 0x15, 0x16, 0x2C, 0x32}, 14},
    {0xF0, {0x3C}, 1},
    {0xF0, {0x69}, 1},
    {0x21, {}, 0},
    {0x29, {}, 0},
    {0x36, {0x28}, 1},

#endif

#if 0
    {0x11, {}, 0},
    {0x26, {0x04}, 1},
    {0xB1, {0x0e, 0x10}, 2},
    {0xC0, {0x08, 0x00}, 2},
    {0xC1, {0x05}, 1},
    {0xC5, {0x38, 0x40}, 2},
    {0x3a, {0x05}, 1},
    {0x36, {0xA8}, 1},
    {0x2A, {0x00, 0x00, 0x00, 0x9F}, 4}, 
    {0x2B, {0x00, 0x00, 0x00, 0x7F}, 4},
    {0xB4, {0x00}, 1},
    {0xf2, {0x01}, 1},
    {0xE0, {0x3f, 0x22, 0x20, 0x30, 0x29, 0x0c, 0x4e, 0xb7, 0x3c, 0x19, 0x22, 0x1e, 0x02, 0x01, 0x00}, 15},
    {0xE1, {0x00, 0x1b, 0x1f, 0x0f, 0x16, 0x13, 0x31, 0x84, 0x43, 0x06, 0x1d, 0x21, 0x3d, 0x3e, 0x3f}, 15},
    {0x29, {}, 0},
    {0x2C, {}, 0},
#endif
};

int lcd_reset(int fd)
{
	size_t i;
    size_t count = sizeof(g_spi_data) / sizeof(g_spi_data[0]);
    for (i = 0; i < count; i++) {
        lcd_spi_send_cmd(fd, &g_spi_data[i]);
    }
    return 0;
}

void lcd_hard_reset(void)
{
    gpiod_line_set_value(g_lcd_reset, 0);
    usleep(200 * 1000);
    gpiod_line_set_value(g_lcd_reset, 1);
    usleep(500 * 1000);
}

int lcd_spi_init(void)
{
    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) {
        printf("failed to open spi device.\n");
	    return -1;
    }

    /* mode  */
    uint8_t mode = LCD_SPI_MODE;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) != 0) {
        printf("SPI_IOC_WR_MODE: %d\n", errno);
        return -1;
    }
    if (ioctl(fd, SPI_IOC_RD_MODE, &mode) != 0) {
        printf("SPI_IOC_RD_MODE: %d\n", errno);
        return -1;
    }

    /* speed */
    uint32_t speed = LCD_SPI_MAX_SPEED;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) != 0) {
        printf("SPI_IOC_WR_MAX_SPEED_HZ: %d\n", errno);
        return -1;
    }
    if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) != 0) {
        printf("SPI_IOC_RD_MAX_SPEED_HZ: %d\n", errno);
        return -1;
    }

    /* bits per word */
    uint8_t bits = LCD_SPI_BITS_PER_WORD;
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) != 0) {
        printf("SPI_IOC_WR_BITS_PER_WORD: %d\n", errno);
        return -1;
    }
    if (ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits) != 0) {
        printf("SPI_IOC_RD_BITS_PER_WORD: %d\n", errno);
        return -1;
    }
    return fd;
}

#define KEY_RIGHT 7
#define KEY_LEFT  6
#define KEY_DOWN  5
#define KEY_UP    4
#define KEY_START 3
#define KEY_SEL   2
#define KEY_A     1
#define KEY_B     0

/* 手柄输入定义 */
#define INPUT_KEY_CLOCK 20
#define INPUT_KEY_LATCH 26
#define INPUT_KEY_DATA  16

struct gpiod_line *g_key_clock;
struct gpiod_line *g_key_latch;
struct gpiod_line *g_key_data;

struct key_map {
    int key_val;
    int map_val;
    const char *name;
};

struct key_map g_key_map[] = {
    {ITEM_ID_UP,       KEY_UP,    "UP"},
    {ITEM_ID_DOWN,     KEY_DOWN,  "DOWN"},
    {ITEM_ID_LEFT,     KEY_LEFT,  "LEFT"},
    {ITEM_ID_RIGHT,    KEY_RIGHT, "RIGHT"},
    {ITEM_ID_O,        KEY_A,     "O"},
    {ITEM_ID_K,        KEY_B,     "K"},
    {ITEM_ID_5,        KEY_A,     "COIN"},
    {ITEM_ID_SELECT,   KEY_SEL,   "SELECT"},
    {ITEM_ID_1,        KEY_START, "P1 START"},
    {ITEM_ID_START,    KEY_START, "START"},
    {ITEM_ID_ENTER,    KEY_UP,    "UP"},
    {ITEM_ID_LCONTROL, KEY_A,     "LCONTROL"},
    {ITEM_ID_LALT,     KEY_B,     "LALT"}
};

struct gpiod_line *g_sound_pin;
void my_sound_init(struct gpiod_chip *chip)
{
   g_sound_pin = gpiod_chip_get_line(chip, 26);
   gpiod_line_request_output(g_sound_pin, "gpio_sound", 0);
}

void init_key(struct gpiod_chip *chip)
{
    g_key_clock = gpiod_chip_get_line(chip, INPUT_KEY_CLOCK);
    if (!g_key_clock) {
        fprintf(stderr, "Failed to get GPIO line\n");
        return;
    }

    g_key_latch = gpiod_chip_get_line(chip, INPUT_KEY_LATCH);
    if (!g_key_latch) {
        fprintf(stderr, "Failed to get GPIO line\n");
        return;
    }

    g_key_data = gpiod_chip_get_line(chip, INPUT_KEY_DATA);
    if (!g_key_data) {
        fprintf(stderr, "Failed to get GPIO line\n");
        return;
    }

    gpiod_line_request_output(g_key_clock, "input", 0);
    gpiod_line_request_output(g_key_latch, "input", 0);
    gpiod_line_request_input(g_key_data, "input");
}

int lcd_init() {
    struct gpiod_chip *chip;

    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {
        fprintf(stderr, "Failed to open GPIO chip\n");
        return 1;
    }

    g_lcd_dc = gpiod_chip_get_line(chip, LCD_DC);
    if (!g_lcd_dc) {
        fprintf(stderr, "Failed to get GPIO line\n");
        gpiod_chip_close(chip);
        return 1;
    }

    g_lcd_reset = gpiod_chip_get_line(chip, LCD_REST);
    if (!g_lcd_reset) {
        fprintf(stderr, "Failed to get GPIO line\n");
        gpiod_chip_close(chip);
        return 1;
    }
    gpiod_line_request_output(g_lcd_dc, "gpio-example", 0);
    gpiod_line_request_output(g_lcd_reset, "gpio-example", 0);

    init_key(chip);
    my_sound_init(chip);

    int spi_fd = lcd_spi_init();
	g_spi_lcd_fd = spi_fd;
    //lcd_hard_reset();
    //lcd_reset(spi_fd);
    //test_lcd(spi_fd);
    return 0;
}


//============================================================
//  OPTIONS
//============================================================

// struct definitions
const options_entry mame_unix_options[] =
{
	// debugging options
	{ NULL,                       NULL,       OPTION_HEADER,     "WINDOWS DEBUGGING OPTIONS" },
	{ "oslog",                    "0",        OPTION_BOOLEAN,    "output error.log data to the system debugger" },

	// performance options
	{ NULL,                       NULL,       OPTION_HEADER,     "WINDOWS PERFORMANCE OPTIONS" },
	{ "priority(-15-1)",          "0",        0,                 "thread priority for the main game thread; range from -15 to 1" },
	{ "multithreading;mt",        "0",        OPTION_BOOLEAN,    "enable multithreading; this enables rendering and blitting on a separate thread" },

	// video options
	{ NULL,                       NULL,       OPTION_HEADER,     "WINDOWS VIDEO OPTIONS" },
	{ "video",                    "d3d",      0,                 "video output method: none, gdi, ddraw, or d3d" },
	{ "numscreens(1-4)",          "1",        0,                 "number of screens to create; usually, you want just one" },
	{ "window;w",                 "0",        OPTION_BOOLEAN,    "enable window mode; otherwise, full screen mode is assumed" },
	{ "maximize;max",             "1",        OPTION_BOOLEAN,    "default to maximized windows; otherwise, windows will be minimized" },
	{ "keepaspect;ka",            "1",        OPTION_BOOLEAN,    "constrain to the proper aspect ratio" },
	{ "prescale",                 "1",        0,                 "scale screen rendering by this amount in software" },
	{ "effect",                   "none",     0,                 "name of a PNG file to use for visual effects, or 'none'" },
	{ "waitvsync",                "0",        OPTION_BOOLEAN,    "enable waiting for the start of VBLANK before flipping screens; reduces tearing effects" },
	{ "syncrefresh",              "0",        OPTION_BOOLEAN,    "enable using the start of VBLANK for throttling instead of the game time" },

	// DirectDraw-specific options
	{ NULL,                       NULL,       OPTION_HEADER,     "DIRECTDRAW-SPECIFIC OPTIONS" },
	{ "hwstretch;hws",            "1",        OPTION_BOOLEAN,    "enable hardware stretching" },

	// Direct3D-specific options
	{ NULL,                       NULL,       OPTION_HEADER,     "DIRECT3D-SPECIFIC OPTIONS" },
	{ "d3dversion(8-9)",          "9",        0,                 "specify the preferred Direct3D version (8 or 9)" },
	{ "filter;d3dfilter;flt",     "1",        OPTION_BOOLEAN,    "enable bilinear filtering on screen output" },

	// per-window options
	{ NULL,                       NULL,       OPTION_HEADER,     "PER-WINDOW VIDEO OPTIONS" },
	{ "screen",                   "auto",     0,                 "explicit name of all screens; 'auto' here will try to make a best guess" },
	{ "aspect;screen_aspect",     "auto",     0,                 "aspect ratio for all screens; 'auto' here will try to make a best guess" },
	{ "resolution;r",             "auto",     0,                 "preferred resolution for all screens; format is <width>x<height>[@<refreshrate>] or 'auto'" },
	{ "view",                     "auto",     0,                 "preferred view for all screens" },

	{ "screen0",                  "auto",     0,                 "explicit name of the first screen; 'auto' here will try to make a best guess" },
	{ "aspect0",                  "auto",     0,                 "aspect ratio of the first screen; 'auto' here will try to make a best guess" },
	{ "resolution0;r0",           "auto",     0,                 "preferred resolution of the first screen; format is <width>x<height>[@<refreshrate>] or 'auto'" },
	{ "view0",                    "auto",     0,                 "preferred view for the first screen" },

	{ "screen1",                  "auto",     0,                 "explicit name of the second screen; 'auto' here will try to make a best guess" },
	{ "aspect1",                  "auto",     0,                 "aspect ratio of the second screen; 'auto' here will try to make a best guess" },
	{ "resolution1;r1",           "auto",     0,                 "preferred resolution of the second screen; format is <width>x<height>[@<refreshrate>] or 'auto'" },
	{ "view1",                    "auto",     0,                 "preferred view for the second screen" },

	{ "screen2",                  "auto",     0,                 "explicit name of the third screen; 'auto' here will try to make a best guess" },
	{ "aspect2",                  "auto",     0,                 "aspect ratio of the third screen; 'auto' here will try to make a best guess" },
	{ "resolution2;r2",           "auto",     0,                 "preferred resolution of the third screen; format is <width>x<height>[@<refreshrate>] or 'auto'" },
	{ "view2",                    "auto",     0,                 "preferred view for the third screen" },

	{ "screen3",                  "auto",     0,                 "explicit name of the fourth screen; 'auto' here will try to make a best guess" },
	{ "aspect3",                  "auto",     0,                 "aspect ratio of the fourth screen; 'auto' here will try to make a best guess" },
	{ "resolution3;r3",           "auto",     0,                 "preferred resolution of the fourth screen; format is <width>x<height>[@<refreshrate>] or 'auto'" },
	{ "view3",                    "auto",     0,                 "preferred view for the fourth screen" },

	// full screen options
	{ NULL,                       NULL,       OPTION_HEADER,     "FULL SCREEN OPTIONS" },
	{ "triplebuffer;tb",          "0",        OPTION_BOOLEAN,    "enable triple buffering" },
	{ "switchres",                "0",        OPTION_BOOLEAN,    "enable resolution switching" },
	{ "full_screen_brightness;fsb(0.1-2.0)","1.0",     0,        "brightness value in full screen mode" },
	{ "full_screen_contrast;fsc(0.1-2.0)", "1.0",      0,        "contrast value in full screen mode" },
	{ "full_screen_gamma;fsg(0.1-3.0)",    "1.0",      0,        "gamma value in full screen mode" },

	// sound options
	{ NULL,                       NULL,       OPTION_HEADER,     "WINDOWS SOUND OPTIONS" },
	{ "audio_latency(1-5)",       "2",        0,                 "set audio latency (increase to reduce glitches)" },

	// input options
	{ NULL,                       NULL,       OPTION_HEADER,     "INPUT DEVICE OPTIONS" },
	{ "dual_lightgun;dual",       "0",        OPTION_BOOLEAN,    "enable dual lightgun input" },

	{ NULL }
};

typedef struct {
#if 0
	uint8_t a;
    uint8_t r;
    uint8_t g;
    uint8_t b;
#else
    uint16_t r:5;
    uint16_t g:6;
    uint16_t b:5;
#endif
} Pixel;

#define DRAW_WIDTH_MAX  LCD_SCREEN_WIDTH
#define DRAW_HEIGHT_MAX LCD_SCREEN_HEIGHT

render_target *g_render_target = NULL;
uint8_t read_key_state(void)
{
#if 1
    gpiod_line_set_value(g_key_latch, 0);
    usleep(1);
    gpiod_line_set_value(g_key_latch, 1);

    uint8_t i;
    uint8_t temp = 0;
    for (i = 0; i < 8; i++) {
        temp >>= 1;
        int b = gpiod_line_get_value(g_key_data);
        if (b == 0) {
            temp |= 0x80;
        }
        gpiod_line_set_value(g_key_clock, 1);
        usleep(1);
        gpiod_line_set_value(g_key_clock, 0);
        usleep(1);
    }
    gpiod_line_set_value(g_key_latch, 1);
#else

    gpiod_line_set_value(g_key_latch, 1);
    usleep(1);
    gpiod_line_set_value(g_key_latch, 0);

    uint8_t i;
    uint8_t temp = 0;
    for (i = 0; i < 8; i++) {
        temp >>= 1;
        int b = gpiod_line_get_value(g_key_data);
        if (b == 0) {
            temp |= 0x80;
        }
        gpiod_line_set_value(g_key_clock, 1);
        usleep(1);
        gpiod_line_set_value(g_key_clock, 0);
        usleep(1);
    }
#endif

    if (temp != 0) {
        int i;
        for (i = 0; i < 8; i++) {
            if ((temp >> i) & 0x01) {
                printf("1 ");
            } else {
                printf("0 ");
            }
        }
        printf("\n");
    }
    return temp;
}

struct key_val_map {
    uint8_t r:1;
    uint8_t l:1;
    uint8_t d:1;
    uint8_t u:1;

    uint8_t start:1;
    uint8_t select:1;
    uint8_t a:1;
    uint8_t b:1;
};

uint8_t g_mame_key_state = 0;

int get_key_state(void *device_internal, void *item_internal)
{
    struct key_map *key_map = (struct key_map *)item_internal;
    if (key_map == NULL) {
        return 0;
    }

    int key_offset = key_map->map_val;
    uint8_t temp = g_mame_key_state;
    if ((temp >> key_offset) & 0x01) {
        return 1;
    } else {
        return 0;
    }
    return 0;
}

int g_osd_inited = 0;
void osd_init(running_machine *machine)
{
    g_render_target = render_target_alloc(NULL, 0);
	input_device *input_device = input_device_add(DEVICE_CLASS_KEYBOARD, "my keyboard", NULL);
	if (input_device == NULL) {
		printf("failed to add input device.\n");
		return;
	}

    size_t item_num = sizeof(g_key_map) / sizeof(g_key_map[0]);
    size_t i = 0;
    for (i = 0; i < item_num; i++) {
    	input_device_item_add(input_device, "test", &g_key_map[i], g_key_map[i].key_val, get_key_state);
    }

    if (g_osd_inited != 0) {
        printf("already inited.\n");
        return;
    }
    g_osd_inited = 1;
}

int g_test_clock_index = 0;
clock_t g_test_clocks[100];

#include <pthread.h>

void writeImage(const char *filename, Pixel *pixels,
    int start_x, int start_y, int width, int height)
{
#if 1
	typedef struct {
        uint16_t r:5;
        uint16_t g:6;
        uint16_t b:5;
	} MyPixel;

	size_t count = width * height;
	MyPixel *mp = malloc(sizeof(MyPixel) * count);
	int i;
	for (i = 0; i < count; i++) {
		mp[i].r = pixels[i].r;
		mp[i].g = pixels[i].g;
		mp[i].b = pixels[i].b;

        uint16_t temp = *(uint16_t *)&mp[i];
        *(uint16_t *)&mp[i] = (((temp >> 8) & 0xFF) + ((temp & 0xFF) << 8));
	}

    lcd_set_window(g_spi_lcd_fd, start_x, start_y, width - 1, height - 1);
	lcd_spi_send_data(g_spi_lcd_fd, (uint8_t *)mp, width * height * sizeof(MyPixel));
    free(mp);
#endif

#if 0
	typedef struct {
    	uint8_t r;
    	uint8_t g;
    	uint8_t b;
	} PPMPixel;

	count = DRAW_WIDTH_MAX * DRAW_HEIGHT_MAX;
	mp = malloc(sizeof(PPMPixel) * count);
	for (i = 0; i < count; i++) {
		mp[i].r = pixels[i].r;
		mp[i].g = pixels[i].g;
		mp[i].b = pixels[i].b;
	}

    FILE* fp = fopen(filename, "wb");
    fprintf(fp, "P6\n%d %d\n255\n", width, height);
    fwrite(mp, sizeof(PPMPixel), width * height, fp);
    fclose(fp);
	free(mp);
#endif
}

#define MAX_LCD_BUFF_ID 2

Pixel *g_draw_buff = NULL;

pthread_mutex_t g_draw_mutex = PTHREAD_MUTEX_INITIALIZER;

void set_thread_affinity(uint32_t cpu)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(mask), &mask);
}

size_t g_audio_max_size = 0;
uint8_t *g_test_video_buff = NULL;
uint8_t *g_test_play_buff = NULL;

Pixel *g_test_hdmi_fb_bp;
struct fb_var_screeninfo g_test_hdmi_vinfo;

struct hdmi_rgb {
	uint8_t a;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

void output_hdmi_fb(Pixel *pixels, int width, int height)
{

    
    int x, y;
    for (x = 0; x < 480; x++) {
        for (y = 0; y < 480; y++) {
            Pixel *tmp = pixels + x + 480 * y;
            int location = x + y * g_test_hdmi_vinfo.xres_virtual;
            *(g_test_hdmi_fb_bp + location) = *tmp;        // Red
        }
    }
}

void *update_lcd(void *arg)
{
    set_thread_affinity(2);
    return NULL;
}

void *check_key(void *arg)
{
    set_thread_affinity(3);
    return NULL;
    while (true) {
        g_mame_key_state = read_key_state();
        usleep(100);
    }
    return NULL;
}

int g_test_video_same = 0;
void *play_sound(void *arg)
{
    int count = 0;
    set_thread_affinity(3);
    while (true) {
        sleep(1);
        //printf("%04d video same count: %05d --> %d\n", count++, g_test_video_same, g_test_video_same / DRAW_HEIGHT_MAX);
        g_test_video_same = 0;
    }
    return NULL;
}

int g_test_audio_update_count = 0;
int g_test_video_update_count = 0;

void osd_update_audio_stream(INT16 *buffer, int samples_this_frame)
{
    device_play(g_pcm_handle, buffer, 4 * samples_this_frame);
}

void osd_update(int skip_redraw)
{
	if ((g_render_target == NULL) || (g_draw_buff == NULL)) {
		return;
	}

	render_target_set_bounds(g_render_target, DRAW_WIDTH_MAX, DRAW_HEIGHT_MAX, 0);
	render_primitive_list *head = render_target_get_primitives(g_render_target);
	if (head == NULL) {
		printf("get primitives failed.\n");
		return;
	}

	drawdd_rgb565_draw_primitives(head, g_draw_buff, DRAW_WIDTH_MAX, DRAW_HEIGHT_MAX, DRAW_WIDTH_MAX);
    output_hdmi_fb(g_draw_buff, DRAW_HEIGHT_MAX, DRAW_WIDTH_MAX);
}

void hdmi_fb_init(void)
{
    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        perror("Error opening framebuffer device");
        exit(1);
    }

    struct fb_var_screeninfo vinfo;
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Error reading variable information");
        exit(1);
    }

    int screensize = vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8;
    char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((int)fbp == -1) {
        perror("Error mapping framebuffer to memory");
        exit(1);
    }
    memset(fbp, 0, screensize); // Clear framebuffer

    g_test_hdmi_fb_bp = (Pixel *)fbp;
    g_test_hdmi_vinfo = vinfo;
}


int main(int argc, const char **argv)
{
    lcd_init();
    hdmi_fb_init();
    g_pcm_handle = device_create();
	g_draw_buff = malloc(sizeof(Pixel) * DRAW_WIDTH_MAX * DRAW_HEIGHT_MAX);

    g_audio_max_size = sizeof(UINT16) * 4096 * 30 * 2;
    g_test_video_buff = malloc(g_audio_max_size);
    g_test_play_buff = malloc(g_audio_max_size);
    (void)memset(g_test_video_buff, 0, g_audio_max_size);
    (void)memset(g_test_play_buff, 0, g_audio_max_size);

    pthread_t tid;
    pthread_create(&tid, NULL, update_lcd, NULL);
    pthread_create(&tid, NULL, check_key, NULL);
    pthread_create(&tid, NULL, play_sound, NULL);

    set_thread_affinity(1);
    return cli_execute(argc, argv, mame_unix_options);
}

//============================================================
//  SOFTWARE RENDERING
//============================================================

#define FUNC_PREFIX(x)		drawdd_rgb565_##x
#define PIXEL_TYPE			UINT16
#define SRCSHIFT_R			3
#define SRCSHIFT_G			2
#define SRCSHIFT_B			3
#define DSTSHIFT_R			11
#define DSTSHIFT_G			5
#define DSTSHIFT_B			0

#include "rendersw.c"
