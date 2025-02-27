// Bibliotecas
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "hardware/timer.h"

// Definições de constantes e pinos
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define NUM_PLAYERS 4
#define NUM_PIXELS 25
#define WS2812_PIN 7
#define IS_RGBW 0
#define BUTTON_A 5
#define BUTTON_B 6
#define BLUE_LED 12
#define GREEN_LED 11
#define RED_LED 13
#define BUZZER_PIN 21

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Variáveis globais para controle de estado
static volatile uint32_t LAST_TIME_A = 0;
static volatile uint32_t LAST_TIME_B = 0;
static volatile bool button_a_pressed = false;
static volatile bool button_b_pressed = false;
static volatile int button_a_press_count = 0; // Contador de pressões do botão A

ssd1306_t ssd; // Estrutura para o display OLED
char board[3][3] = {{' ', ' ', ' '}, {' ', ' ', ' '}, {' ', ' ', ' '}}; // Matriz do jogo
int cursor_x = 0, cursor_y = 0; // Posição do cursor no tabuleiro
char current_player = 'X'; // Jogador atual
bool game_over = false; // Indica se o jogo terminou

// Função para desenhar o tabuleiro no display
void draw_board() {
    ssd1306_fill(&ssd, false); // Limpa o display

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

    ssd1306_send_data(&ssd); // Envia os dados para o display
}

// Função para enviar um pixel para a matriz de LEDs
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Função para converter RGB em um valor de 32 bits
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// Matriz de LEDs para representar os símbolos do jogo
const bool matriz_led[NUM_PLAYERS][NUM_PIXELS] = {
    {1,0,0,0,1, 0,1,0,1,0, 0,0,1,0,0, 0,1,0,1,0, 1,0,0,0,1}, // X
    {0,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 0,1,1,1,0}, // O
    {0,0,1,0,0, 0,1,0,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1}, // V
    {0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0}  // Matriz vazia
};

// Função para exibir um símbolo na matriz de LEDs
void show_symbol_leds(int symbol, uint32_t color) {
    int i;

    for (i = 0; i < NUM_PIXELS; i++) {
        put_pixel(matriz_led[symbol][i] ? color : 0);
    }
    sleep_ms(10);
} 

// Função para tocar um som no buzzer
void play_sound(int frequency, int duration_ms) {
    int period_us = 1000000 / frequency; // Período em microssegundos
    int half_period_us = period_us / 2;

    for (int i = 0; i < (duration_ms * 1000) / period_us; i++) {
        gpio_put(BUZZER_PIN, 1); // Liga o buzzer
        sleep_us(half_period_us);
        gpio_put(BUZZER_PIN, 0); // Desliga o buzzer
        sleep_us(half_period_us);
    }
}

// Função para verificar se há um vencedor
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

// Função de interrupção para os botões
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

// Função para reiniciar o jogo
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

    // Informa que o jogo foi reiniciado
    printf("Jogo reiniciado! Bom jogo!\n");
}

// Função para atualizar o estado do jogo
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

    // Insere o símbolo do jogador atual na matriz ao pressionar o botão A
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
                printf("Pressione o botão A duas vezes para reiniciar o jogo.\n"); // Instrução para reiniciar
                if (winner == 'X') {
                    gpio_put(BLUE_LED, true); // Acende o LED azul para indicar vitória do X
                    show_symbol_leds(0, urgb_u32(0, 0, 200)); // Exibe "X" na matriz de LEDs (azul)
                    play_sound(1000, 600); // Toca o som da vitória do X
                } else if (winner == 'O') {
                    gpio_put(GREEN_LED, true); // Acende o LED verde para indicar vitória do O
                    show_symbol_leds(1, urgb_u32(0, 200, 0)); // Exibe "O" na matriz de LEDs (verde)
                    play_sound(700, 600); // Toca o som da vitória do O
                }
                return;
            }

            // Verifica se o jogo empatou
            if (check_draw()) {
                game_over = true;
                printf("Deu velha!\n");
                printf("Pressione o botão A duas vezes para reiniciar o jogo.\n"); // Instrução para reiniciar
                gpio_put(RED_LED, true); // Acende o LED vermelho para indicar empate
                show_symbol_leds(2, urgb_u32(200, 0, 0)); // Exibe "V" na matriz de LEDs (vermelho)
                play_sound(500, 600); // Toca o som do empate
                return;
            }

            // Alterna o jogador
            if (current_player == 'X') {
                current_player = 'O';
            } else {
                current_player = 'X';
            }
        
        // Caso o jogador tente usar um espaço já ocupado
        } else {
            gpio_put(RED_LED, true);
            printf("Posição já em uso, selecione outra.\n");
            play_sound(200, 300);
            sleep_ms(500);
            gpio_put(RED_LED, false);
        }
    }

    // Se locomove por entre as células da matriz ao pressionar o botão B
    if (button_b_pressed) {
        button_b_pressed = false;
        cursor_x = (cursor_x + 1) % 3; // Move horizontalmente
        if (cursor_x == 0) {
            cursor_y = (cursor_y + 1) % 3; // Move verticalmente apenas quando cursor_x volta a 0
        }
    }
    
    draw_board();
}

// Função para inicializar os LEDs
void init_leds() {
    gpio_init(BLUE_LED);
    gpio_set_dir(BLUE_LED, GPIO_OUT);
    gpio_init(GREEN_LED);
    gpio_set_dir(GREEN_LED, GPIO_OUT);
    gpio_init(RED_LED);
    gpio_set_dir(RED_LED, GPIO_OUT);
}

// Função para inicializar o buzzer
void init_buzzer() {
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
}

// Função principal
int main() {
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW); // Inicializa a máquina de estados
    stdio_init_all();
    init_leds();
    init_buzzer();
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, SCREEN_WIDTH, SCREEN_HEIGHT, false, endereco, I2C_PORT);  // Inicializa o display
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // Inicializa o botão A
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Inicializa o botão B
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Desenha o tabuleiro e atualiza a cada interação dos jogadores com a placa
    draw_board();
    while (true) {
        update_game();
        sleep_ms(200);
    }
    return 0;
}