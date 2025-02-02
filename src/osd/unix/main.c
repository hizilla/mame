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
#include <linux/i2c-dev.h>
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
#define LCD_SCREEN_WIDTH  480 // 640 //480
#define LCD_SCREEN_HEIGHT 480 // 480 //320

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
  snd_pcm_hw_params_set_rate_resample(handle, params, 0);

  unsigned int buffer_size = 8192; // 默认131072
  snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);
  snd_pcm_hw_params_set_period_size(handle, params, 128, 0);
  printf("Modify buffer size: %u frames\n", buffer_size);

  /* 设置采样率 */
  int val = 48000;
  snd_pcm_hw_params_set_rate_near(handle, params, &val, 0);

/* 将参数写入设备 */
  rc = snd_pcm_hw_params(handle, params);
  goto_error_if_fail(rc >= 0)

  snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
  printf("Get buffer size: %u frames\n", buffer_size);

  snd_pcm_hw_params_get_period_size(params, &frame_num, 0);
  printf("frame num: %d\n", frame_num);

  return handle;
error:
  snd_pcm_hw_params_free(params);
  snd_pcm_close(handle);
  return NULL;
}

int device_play(snd_pcm_t *pcm_handle, const void *data, int data_size)
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

#define KEY_RIGHT 0
#define KEY_LEFT  1
#define KEY_DOWN  2
#define KEY_UP    3
#define KEY_A     4
#define KEY_B     5
#define KEY_4     6
#define KEY_5     7


#define KEY_START  8
#define KEY_SELECT 9

/* 手柄输入定义 */
#define INPUT_KEY_CLOCK 5
#define INPUT_KEY_LATCH 0
#define INPUT_KEY_DATA  6

#define INPUT_KEY_SELECT 13
#define INPUT_KEY_START  26

struct gpiod_line *g_key_clock;
struct gpiod_line *g_key_latch;
struct gpiod_line *g_key_data;

struct gpiod_line *g_key_select;
struct gpiod_line *g_key_start;

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
    {ITEM_ID_LCONTROL, KEY_A,     "LCONTROL"},
    {ITEM_ID_LALT,     KEY_B,     "LALT"},

    {ITEM_ID_ENTER,    KEY_START,  "ENTER"},
    {ITEM_ID_1,        KEY_4,      "P1 START"},
    {ITEM_ID_5,        KEY_5,      "COIN"},
    {ITEM_ID_ESC,      KEY_SELECT, "ESCAPE"},
    {ITEM_ID_START,    KEY_START,  "START"},
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

    g_key_select = gpiod_chip_get_line(chip, INPUT_KEY_SELECT);
    if (!g_key_select) {
        fprintf(stderr, "Failed to get GPIO line\n");
        return;
    }

    g_key_start = gpiod_chip_get_line(chip, INPUT_KEY_START);
    if (!g_key_start) {
        fprintf(stderr, "Failed to get GPIO line\n");
        return;
    }

    gpiod_line_request_output(g_key_clock, "input", 0);
    gpiod_line_request_output(g_key_latch, "input", 0);
    gpiod_line_request_input(g_key_data, "input");
    gpiod_line_request_input(g_key_select, "input");
    gpiod_line_request_input(g_key_start, "input");
}

int gpio_init() {
    struct gpiod_chip *chip;
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {
        fprintf(stderr, "Failed to open GPIO chip\n");
        return 1;
    }
    init_key(chip);
    my_sound_init(chip);
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

struct rgb888 {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
};

#define DRAW_WIDTH_MAX  LCD_SCREEN_WIDTH
#define DRAW_HEIGHT_MAX LCD_SCREEN_HEIGHT

render_target *g_render_target = NULL;
uint16_t read_key_state(void)
{
#if 1
    gpiod_line_set_value(g_key_latch, 0);
    usleep(1);
    gpiod_line_set_value(g_key_latch, 1);

    uint16_t i;
    uint16_t temp = 0;
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

    int start = gpiod_line_get_value(g_key_start);
    if (start == 0) {
        temp |= 1 << KEY_START;
    }

    int select = gpiod_line_get_value(g_key_select);
    if (select == 0) {
        temp |= 1 << KEY_SELECT;
    }

#if 0
    if (temp != 0) {
        int i;
        for (i = 0; i < 16; i++) {
            if ((temp >> i) & 0x01) {
                printf("1 ");
            } else {
                printf("0 ");
            }
        }
        printf("\n");
    }
#endif
    return temp;
}

uint16_t g_mame_key_state = 0;
int get_key_state(void *device_internal, void *item_internal)
{
    struct key_map *key_map = (struct key_map *)item_internal;
    if (key_map == NULL) {
        return 0;
    }

    int key_offset = key_map->map_val;
    uint16_t temp = g_mame_key_state;
    if ((temp >> key_offset) & 0x01) {
        return 1;
    } else {
        return 0;
    }
    return 0;
}

#define JOYSTICK_COUNT 4
struct my_joystick {
    int id;
    const char *name;
    int key;
    int min;
    int max;
    int val;
} g_joysticks[JOYSTICK_COUNT] = {
    { 0, "left x", ITEM_ID_XAXIS,   1994, 32767 },
    { 1, "left y", ITEM_ID_YAXIS,   225,  32767 },
    { 2, "right y", ITEM_ID_RYAXIS, 125,  32767},
    { 3, "right x", ITEM_ID_RXAXIS, 1630, 32767 },
};

int g_joystick_fd = -1;
int joystick_axis_get_state(void *device_internal, void *item_internal)
{
    struct my_joystick *stick = (struct my_joystick *)item_internal;
    if (stick == NULL) {
        return 0;
    }

    int result;
    int center = (stick->min + stick->max) / 2;
    if (stick->val > center) {
        result = (int64_t)(stick->val - center) * (int64_t)INPUT_ABSOLUTE_MAX / (stick->max - center);
        return -MIN(result, INPUT_ABSOLUTE_MAX);
    } else {
        result = -((int64_t)(center - stick->val) * (int64_t)-INPUT_ABSOLUTE_MIN / (center - stick->min));
        return -MAX(result, INPUT_ABSOLUTE_MIN);
    }
}

void joystick_init(void)
{
    int fd = -1;
    if ((fd = open("/dev/i2c-1", O_RDWR)) < 0) {
        printf("Error: Couldn't open device! %d\n", fd);
        exit(1);
    }

    int asd_address = 0x48;
    if (ioctl(fd, I2C_SLAVE, asd_address) < 0) {
        printf("Error: Couldn't find device on address!\n");
        exit(1);
    }

    input_device *input_device = input_device_add(DEVICE_CLASS_JOYSTICK, "my joystick", NULL);
    if (input_device == NULL) {
        printf("failed to add joystick");
    }

    int i = 0;
    for (i = 0; i < JOYSTICK_COUNT; i++) {
        input_device_item_add(input_device, "", &g_joysticks[i], g_joysticks[i].key,
            joystick_axis_get_state);
    }
    g_joystick_fd = fd;
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
    joystick_init();
    if (g_osd_inited != 0) {
        printf("already inited.\n");
        return;
    }
    g_osd_inited = 1;
}

void set_thread_affinity(uint32_t cpu)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(mask), &mask);
}

#include <pthread.h>
struct rgb888 *g_draw_buff = NULL;
struct hdmi_rgba *g_test_hdmi_fb_bp;

struct hdmi_rgba {
	uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
};

void lcd_test(int width, int height)
{
    int x, y;
    for (y = 0; y < height; y++) {
        struct hdmi_rgba rgba = { 0 };
        if (y < 160) {
            rgba.r = 0xFF;
        } else if (160 <= y && y < 320) {
            rgba.g = 0xFF;
        } else {
            rgba.b = 0xFF;
        }
        for (x = 0; x < width; x++) {
            size_t loc = (x + 480 * y);
            *(g_test_hdmi_fb_bp + loc) = rgba;
        }
    }
    sleep(1);
}

void output_hdmi_fb(struct rgb888 *pixels, int width, int height)
{
    memcpy(g_test_hdmi_fb_bp, pixels, sizeof(struct rgb888) * width * height);
}

void *check_key(void *arg)
{
    set_thread_affinity(3);
    while (true) {
        g_mame_key_state = read_key_state();
        usleep(100);
    }
    return NULL;
}

void *poll_joystick(void *arg)
{
    uint8_t readBuf[2];
    uint8_t writeBuf[3];
    set_thread_affinity(2);
    while (true) {
        if (g_joystick_fd == -1) {
            usleep(1000);
            continue;
        }

        int i = 0;
        for (i = 0; i < JOYSTICK_COUNT; i++) {
            writeBuf[0] = 1; // config register is 1
            writeBuf[1] = 0b11000010 | i << 4; // 0xC2 single shot off
            writeBuf[2] = 0b11101001; // bits 7-0  0x85
            if (write(g_joystick_fd, writeBuf, 3) != 3) {
                perror("Write to register 1");
                exit(1);
            }

            usleep(10 * 1000);
            readBuf[0] = 0;
            if (write(g_joystick_fd, readBuf, 1) != 1) {
                perror("Write register select");
                exit(-1);
            }

            if (read(g_joystick_fd, readBuf, 2) != 2) {
                perror("Read conversion");
                exit(-1);
            }

            int val = readBuf[0] << 8 | readBuf[1];
            if (val < 0) {
                val = 0;
            }
            g_joysticks[i].val = val;
        }
    }
    return NULL;
}

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

	drawdd_rgb888_draw_primitives(head, g_draw_buff, DRAW_WIDTH_MAX, DRAW_HEIGHT_MAX, DRAW_WIDTH_MAX);
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
    memset(fbp, 0, screensize);
    g_test_hdmi_fb_bp = (struct hdmi_rgba *)fbp;
}


int main(int argc, const char **argv)
{
    gpio_init();
    hdmi_fb_init();
    g_pcm_handle = device_create();
	g_draw_buff = malloc(sizeof(struct rgb888) * DRAW_WIDTH_MAX * DRAW_HEIGHT_MAX);

    pthread_t tid;
    pthread_create(&tid, NULL, check_key, NULL);
    pthread_create(&tid, NULL, poll_joystick, NULL);
    lcd_test(DRAW_WIDTH_MAX, DRAW_HEIGHT_MAX);

    set_thread_affinity(1);
    return cli_execute(argc, argv, mame_unix_options);
}

//============================================================
//  SOFTWARE RENDERING
//============================================================

#define FUNC_PREFIX(x)		drawdd_rgb888_##x
#define PIXEL_TYPE			UINT32
#define SRCSHIFT_R			0
#define SRCSHIFT_G			0
#define SRCSHIFT_B			0
#define DSTSHIFT_R			16
#define DSTSHIFT_G			8
#define DSTSHIFT_B			0

#include "rendersw.c"
