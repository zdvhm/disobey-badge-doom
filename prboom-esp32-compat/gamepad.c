// Disobey Badge 2025/2026 - GPIO Button Input for DOOM
// Adapted from ESP32-DOOM gamepad.c for badge hardware

#include <stdlib.h>

#include "doomdef.h"
#include "doomtype.h"
#include "m_argv.h"
#include "d_event.h"
#include "g_game.h"
#include "d_main.h"
#include "gamepad.h"
#include "lprintf.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

// Joystick variables (unused but needed for compilation)
int usejoystick=0;
int joyleft, joyright, joyup, joydown;
volatile int joyVal=0;

// Disobey Badge 2025/2026 GPIO pins
#define BTN_UP      11
#define BTN_DOWN    1
#define BTN_LEFT    21
#define BTN_RIGHT   2
#define BTN_A       13
#define BTN_B       38
#define BTN_START   12
#define BTN_SELECT  45
#define BTN_STICK   14

typedef struct {
	int gpio;
	int *key;
	int active_low; // 1 = active LOW (pull-up), 0 = active HIGH (pull-down)
} GPIOKeyMap;

// Mapping badge buttons to DOOM keys
static const GPIOKeyMap keymap[]={
	{BTN_UP,     &key_up,           1},     // D-pad Up = Move forward
	{BTN_DOWN,   &key_down,         1},     // D-pad Down = Move backward
	{BTN_LEFT,   &key_left,         1},     // D-pad Left = Turn left
	{BTN_RIGHT,  &key_right,        1},     // D-pad Right = Turn right
	{BTN_A,      &key_fire,         1},     // A = Fire weapon
	{BTN_A,      &key_menu_enter,   1},     // A = Menu confirm
	{BTN_B,      &key_use,          1},     // B = Use/Open doors
	{BTN_START,  &key_escape,       1},     // Start = Menu/Escape
	{BTN_SELECT, &key_map,          0},     // Select = Automap (active HIGH)
	{BTN_STICK,  &key_speed,        1},     // Stick = Run modifier
	{0, NULL, 0},
};

void gamepadPoll(void)
{
}

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void gpioTask(void *arg) {
    uint32_t io_num;
	int level;
	event_t ev;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
			for (int i=0; keymap[i].key!=NULL; i++) {
				if(keymap[i].gpio == io_num)
				{
					level = gpio_get_level(io_num);
					// Active-low (pull-up): level 0 = pressed = keydown
					// Active-high (pull-down): level 1 = pressed = keydown
					if (keymap[i].active_low) {
						ev.type = level ? ev_keyup : ev_keydown;
					} else {
						ev.type = level ? ev_keydown : ev_keyup;
					}
					ev.data1=*keymap[i].key;
					D_PostEvent(&ev);
				}
			}
        }
    }
}

void gamepadInit(void)
{
	lprintf(LO_INFO, "gamepadInit: Disobey Badge 2025/2026 buttons.\n");
}

void jsInit()
{
	// Configure pull-up buttons (active LOW): all except Select
	gpio_config_t io_conf_pullup = {
		.pin_bit_mask = (1ULL<<BTN_UP) | (1ULL<<BTN_DOWN) | (1ULL<<BTN_LEFT) |
		                (1ULL<<BTN_RIGHT) | (1ULL<<BTN_A) | (1ULL<<BTN_B) |
		                (1ULL<<BTN_START) | (1ULL<<BTN_STICK),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_ANYEDGE,
	};
	gpio_config(&io_conf_pullup);

	// Configure Select button separately (active HIGH, pull-down)
	gpio_config_t io_conf_pulldown = {
		.pin_bit_mask = (1ULL<<BTN_SELECT),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.intr_type = GPIO_INTR_ANYEDGE,
	};
	gpio_config(&io_conf_pulldown);

	// Create queue for GPIO events from ISR
	gpio_evt_queue = xQueueCreate(16, sizeof(uint32_t));

	// Start GPIO processing task
	xTaskCreatePinnedToCore(&gpioTask, "GPIO", 2048, NULL, 7, NULL, 0);

	// Install GPIO ISR service
	gpio_install_isr_service(ESP_INTR_FLAG_SHARED);

	// Register ISR handlers - avoid duplicate registration for same GPIO
	int registered[50] = {0};
	for (int i=0; keymap[i].key!=NULL; i++) {
		if (keymap[i].gpio < 50 && !registered[keymap[i].gpio]) {
			gpio_isr_handler_add(keymap[i].gpio, gpio_isr_handler, (void*)(intptr_t)keymap[i].gpio);
			registered[keymap[i].gpio] = 1;
		}
	}

	lprintf(LO_INFO, "jsInit: Disobey Badge GPIO buttons initialized.\n");
}
