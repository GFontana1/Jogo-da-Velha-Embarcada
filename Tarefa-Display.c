#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "hardware/timer.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define WS2812_PIN 7
#define BUTTON_A 5
#define BUTTON_B 6
#define BLUE_LED 12
#define GREEN_LED 11
#define RED_LED 13

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

static volatile uint32_t LAST_TIME_A = 0;
static volatile uint32_t LAST_TIME_B = 0;
static volatile bool button_a_pressed = false;
static volatile bool button_b_pressed = false;

ssd1306_t ssd;
char board[3][3] = {{' ', ' ', ' '}, {' ', ' ', ' '}, {' ', ' ', ' '}};
int cursor_x = 0, cursor_y = 0;
char current_player = 'X';

void draw_board() {
    ssd1306_fill(&ssd, false);
    for (int i = 1; i < 3; i++) {
        ssd1306_line(&ssd, 0, i * 20, SCREEN_WIDTH, i * 20, true);
        ssd1306_line(&ssd, i * 40, 0, i * 40, SCREEN_HEIGHT, true);
    }
    
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            if (board[y][x] != ' ') {
                ssd1306_draw_char(&ssd, current_player, y * 45 + 10, x * 20 + 8);
            }
        }
    }
    
    ssd1306_rect(&ssd, cursor_x * 23, cursor_y * 50, 20, 10, true, false);
    ssd1306_send_data(&ssd);
}


void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if (gpio == BUTTON_A && current_time - LAST_TIME_A > 200000) {
        LAST_TIME_A = current_time;
        button_a_pressed = true;
    } else if (gpio == BUTTON_B && current_time - LAST_TIME_B > 200000) {
        LAST_TIME_B = current_time;
        button_b_pressed = true;
    }
}

void update_game() {
    if (button_a_pressed) {
        button_a_pressed = false;
        if (board[cursor_y][cursor_x] == ' ') {
            board[cursor_y][cursor_x] = current_player;
            current_player = (current_player == 'X') ? 'O' : 'X';
        }else{
            gpio_put(RED_LED, true);
            printf("Posição já em uso, selecione outra.\n");
            sleep_ms(700);
            gpio_put(RED_LED, false);
        }
    }
    if (button_b_pressed) {
        button_b_pressed = false;
        cursor_x = (cursor_x + 1) % 3;
        if (cursor_x == 0) cursor_y = (cursor_y + 1) % 3;
    }
    draw_board();
}

int main() {
    stdio_init_all();
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, SCREEN_WIDTH, SCREEN_HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    gpio_init(BLUE_LED);
    gpio_set_dir(BLUE_LED, GPIO_OUT);
    gpio_init(GREEN_LED);
    gpio_set_dir(GREEN_LED, GPIO_OUT);
    gpio_init(RED_LED);
    gpio_set_dir(RED_LED, GPIO_OUT);

    draw_board();
    while (true) {
        update_game();
        sleep_ms(200);
    }
    return 0;
}
