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
#define NUM_RESULTS 3
#define NUM_PIXELS 25
#define WS2812_PIN 7
#define IS_RGBW 0
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
static volatile int button_a_press_count = 0; // Contador de pressões do botão A

ssd1306_t ssd;
char board[3][3] = {{' ', ' ', ' '}, {' ', ' ', ' '}, {' ', ' ', ' '}};
int cursor_x = 0, cursor_y = 0; // cursor_x controla a coluna, cursor_y controla a linha
char current_player = 'X';
bool game_over = false; // Variável para controlar se o jogo terminou

void draw_board() {
    ssd1306_fill(&ssd, false);

    // Desenha as linhas do tabuleiro
    for (int i = 1; i < 3; i++) {
        ssd1306_line(&ssd, 0, i * 20, SCREEN_WIDTH, i * 20, true);  // Linhas horizontais
        ssd1306_line(&ssd, i * 40, 0, i * 40, SCREEN_HEIGHT, true); // Linhas verticais
    }

    // Desenha os símbolos do jogo
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            if (board[x][y] != ' ') {
                ssd1306_draw_char(&ssd, board[x][y], x * 40 + 16, y * 20 + 6);
            }
        }
    }

    // Desenha o cursor
    ssd1306_rect(&ssd, cursor_x * 20 + 2, cursor_y * 40 + 2, 36, 16, true, false);

    ssd1306_send_data(&ssd);
}

// Função para os leds da matriz
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Função para os leds da matriz
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// Matriz de leds
const bool matriz_led[NUM_RESULTS][NUM_PIXELS] = {
    {1,0,0,0,1, 0,1,0,1,0, 0,0,1,0,0, 0,1,0,1,0, 1,0,0,0,1}, // X
    {0,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 0,1,1,1,0}, // O
    {0,0,1,0,0, 0,1,0,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1}, // V
};

// Função para desenhar um símbolo na matriz
void show_symbol_leds(int symbol, uint32_t color) {
    int i;

    for (i = 0; i < NUM_PIXELS; i++) {
        put_pixel(matriz_led[symbol][i] ? color : 0);
    }
    sleep_ms(10);
} 

char check_winner() {
    // Verifica linhas
    for (int y = 0; y < 3; y++) {
        if (board[y][0] != ' ' && board[y][0] == board[y][1] && board[y][1] == board[y][2]) {
            return board[y][0]; // Retorna o jogador que venceu
        }
    }

    // Verifica colunas
    for (int x = 0; x < 3; x++) {
        if (board[0][x] != ' ' && board[0][x] == board[1][x] && board[1][x] == board[2][x]) {
            return board[0][x]; // Retorna o jogador que venceu
        }
    }

    // Verifica diagonais
    if (board[0][0] != ' ' && board[0][0] == board[1][1] && board[1][1] == board[2][2]) {
        return board[0][0]; // Retorna o jogador que venceu
    }
    if (board[0][2] != ' ' && board[0][2] == board[1][1] && board[1][1] == board[2][0]) {
        return board[0][2]; // Retorna o jogador que venceu
    }

    return ' '; // Nenhum vencedor
}

// Função para verificar se o jogo empatou
bool check_draw() {
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            if (board[y][x] == ' ') {
                return false; // Ainda há espaços vazios, o jogo não empatou
            }
        }
    }
    return true; // Todos os espaços estão preenchidos, o jogo empatou
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

void reset_game() {
    // Limpa o tabuleiro
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            board[y][x] = ' ';
        }
    }

    // Reinicia as variáveis de estado
    cursor_x = 0;
    cursor_y = 0;
    current_player = 'X';
    game_over = false;

    // Desliga os LEDs
    gpio_put(BLUE_LED, false);
    gpio_put(GREEN_LED, false);
    gpio_put(RED_LED, false);

    // Limpa a matriz de LEDs
    show_symbol_leds(3, 0); // Usa a matriz vazia

    // Redesenha o tabuleiro
    draw_board();
}

void update_game() {
    if (game_over) {
        // Verifica se o botão A foi pressionado duas vezes para reiniciar
        if (button_a_pressed) {
            button_a_pressed = false;
            button_a_press_count++;
            if (button_a_press_count == 2) {
                button_a_press_count = 0; // Reinicia o contador
                reset_game(); // Reinicia o jogo
            }
        }
        return; // Se o jogo terminou, não faz nada além de verificar o reinício
    }

    if (button_a_pressed) {
        button_a_pressed = false;
        if (board[cursor_y][cursor_x] == ' ') {
            board[cursor_y][cursor_x] = current_player;

            // Desenha o tabuleiro antes de verificar o vencedor
            draw_board();

            // Verifica se há um vencedor
            char winner = check_winner();
            if (winner != ' ') {
                game_over = true;
                printf("Jogador %c venceu!\n", winner);
                printf("Pressione A 2 vezes para jogar novamente!\n");
                if (winner == 'X') {
                    gpio_put(BLUE_LED, true); // Acende o LED azul para indicar vitória do X
                    show_symbol_leds(0, urgb_u32(0, 0, 200)); // Exibe "X" na matriz de LEDs (azul)
                } else if (winner == 'O') {
                    gpio_put(GREEN_LED, true); // Acende o LED verde para indicar vitória do O
                    show_symbol_leds(1, urgb_u32(0, 200, 0)); // Exibe "O" na matriz de LEDs (verde)
                }
                return;
            }

            // Verifica se o jogo empatou
            if (check_draw()) {
                game_over = true;
                printf("Deu velha!\n");
                printf("Pressione A 2 vezes para jogar novamente!\n");
                gpio_put(RED_LED, true); // Acende o LED vermelho para indicar empate
                show_symbol_leds(2, urgb_u32(200, 0, 0)); // Exibe "V" na matriz de LEDs (vermelho)
                return;
            }

            // Alterna o jogador
            if (current_player == 'X') {
                current_player = 'O';
            } else {
                current_player = 'X';
            }
        } else {
            gpio_put(RED_LED, true);
            printf("Posição já em uso, selecione outra.\n");
            sleep_ms(700);
            gpio_put(RED_LED, false);
        }
    }
    if (button_b_pressed) {
        button_b_pressed = false;
        cursor_x = (cursor_x + 1) % 3; // Move horizontalmente
        if (cursor_x == 0) {
            cursor_y = (cursor_y + 1) % 3; // Move verticalmente apenas quando cursor_x volta a 0
        }
    }
    
    draw_board();
}

void init_leds() {
    gpio_init(BLUE_LED);
    gpio_set_dir(BLUE_LED, GPIO_OUT);
    gpio_init(GREEN_LED);
    gpio_set_dir(GREEN_LED, GPIO_OUT);
    gpio_init(RED_LED);
    gpio_set_dir(RED_LED, GPIO_OUT);
}

int main() {
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW); // Inicializa a máquina de estados
    stdio_init_all();
    init_leds();
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

    draw_board();
    while (true) {
        update_game();
        sleep_ms(200);
    }
    return 0;
}