#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/i2c.h"

//Definição pinos dos LEDs
#define LED_PIN_RED 13
#define LED_PIN_BLUE 12
#define LED_PIN_GREEN 11

//Definição pinos do JoyStck e Botão
#define JOYSTICK_X 26
#define JOYSTICK_Y 27
#define BTN_JOYSTICK 22
#define BTN_A 5
 
//Definição pinos e outras configs do I2C- Display
#define I2C_PORT i2c1 //Indica a porta utilizada
#define I2C_SDA 14 // Pino SDA
#define I2C_SLC 15 // Pino SLC
#define WIDTH 128 //Largura da tela
#define HEIGHT 64 //Altura da tela
#define QUADRADO 8 //Tamanho do quadrado
#define endereco 0x3C //Endereço do display

// Variáveis globais
#define DEBOUNCE_TIME 200000 //Tempo de Debounce em microsegundos
bool estilo_borda = false;//Variável para alterar as bordas
ssd1306_t ssd;// Definição do display
bool leds_ativos = true; // Variável para armazenar o estado dos LEDs
bool led_verde_estado = false; // Variável para armazenar o estado do LED Verde
volatile uint32_t last_interrupt_time_joystick = 0; //Variável para debounce da interupção do botão do joystick
volatile uint32_t last_interrupt_time_A = 0; //Variável para debounce da interrupção do botão A
static volatile uint32_t last_time = 0;// Variável para armazenar tempo da ultima interrupção

// Calibração do joystick
int joystick_x_min = 4095;
int joystick_x_max = 0;
int joystick_y_min = 4095;
int joystick_y_max = 0;

#define DEAD_ZONE 100

// Protótipos das funções
void atualizar_borda_display(ssd1306_t *ssd, bool estilo);
int map_adc_to_screen(int value, int is_y_axis);
void setup_pwm(uint PWM_PIN);
uint16_t converter_adc_pwm(uint16_t value);
static void gpio_irq_handler(uint gpio, uint32_t events);

void atualizar_borda_display(ssd1306_t *ssd, bool estilo) {
    ssd1306_fill(ssd, false);

    if (estilo) {
        for (int i = 0; i < WIDTH; i += 4) {
            ssd1306_pixel(ssd, i, 0, true);
            ssd1306_pixel(ssd, i, HEIGHT - 1, true);
        }
        for (int i = 0; i < HEIGHT; i += 4) {
            ssd1306_pixel(ssd, 0, i, true);
            ssd1306_pixel(ssd, WIDTH - 1, i, true);
        }
    } else {
        for (int i = 0; i < WIDTH; i++) {
            ssd1306_pixel(ssd, i, 0, true);
            ssd1306_pixel(ssd, i, HEIGHT - 1, true);
        }
        for (int i = 0; i < HEIGHT; i++) {
            ssd1306_pixel(ssd, 0, i, true);
            ssd1306_pixel(ssd, WIDTH - 1, i, true);
        }
    }
    ssd1306_send_data(ssd);
}

int map_adc_to_screen(int value, int is_y_axis) {
    int mapped_value;
    int range;

    // Verifica se a calibração é válida
    if (joystick_x_max - joystick_x_min < 1000) {
        joystick_x_min = 500;
        joystick_x_max = 3500;
    }
    if (joystick_y_max - joystick_y_min < 1000) {
        joystick_y_min = 500;
        joystick_y_max = 3500;
    }

    if (is_y_axis) {
        if (abs(value - ((joystick_y_max + joystick_y_min) / 2)) < DEAD_ZONE) return -1;
        range = joystick_y_max - joystick_y_min;
        mapped_value = ((value - joystick_y_min) * (HEIGHT - QUADRADO)) / range;
    } else {
        if (abs(value - ((joystick_x_max + joystick_x_min) / 2)) < DEAD_ZONE) return -1;
        range = joystick_x_max - joystick_x_min;
        mapped_value = ((value - joystick_x_min) * (WIDTH - QUADRADO)) / range;
    }

    // Limita valores para não ultrapassar a tela
    if (mapped_value < 0) mapped_value = 0;
    if (is_y_axis && mapped_value > HEIGHT - QUADRADO) mapped_value = HEIGHT - QUADRADO;
    if (!is_y_axis && mapped_value > WIDTH - QUADRADO) mapped_value = WIDTH - QUADRADO;

    return mapped_value;
}

void setup_pwm(uint PWM_PIN) {
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PWM_PIN);
    pwm_set_wrap(slice, 255);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(PWM_PIN), 0);
    pwm_set_enabled(slice, true);
}

uint16_t converter_adc_pwm(uint16_t value) {
    return (value * 255) / 4095;
}

static void gpio_irq_handler(uint gpio, uint32_t events) {
        uint32_t current_time = to_us_since_boot(get_absolute_time());
    
        if (gpio == BTN_JOYSTICK) {
            if (current_time - last_interrupt_time_joystick < DEBOUNCE_TIME) return;
            last_interrupt_time_joystick = current_time;
    
            led_verde_estado = !led_verde_estado;
            gpio_put(LED_PIN_GREEN, led_verde_estado);
            estilo_borda = !estilo_borda;
            atualizar_borda_display(&ssd, estilo_borda);
        }
    
        if (gpio == BTN_A) {
            if (current_time - last_interrupt_time_A < DEBOUNCE_TIME) return;
            last_interrupt_time_A = current_time;
    
            leds_ativos = !leds_ativos;
        }
        
}


int main() {
    stdio_init_all();

    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);

    setup_pwm(LED_PIN_RED);
    setup_pwm(LED_PIN_BLUE);

    gpio_init(LED_PIN_GREEN);
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
    gpio_put(LED_PIN_GREEN, 0);

    gpio_init(BTN_JOYSTICK);
    gpio_set_dir(BTN_JOYSTICK, GPIO_IN);
    gpio_pull_up(BTN_JOYSTICK);

    gpio_init(BTN_A);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_A);

    gpio_set_irq_enabled_with_callback(BTN_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    irq_set_enabled(IO_IRQ_BANK0, true);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SLC, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SLC);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Calibração do joystick
    printf("Calibrando joystick. Mova o joystick para os extremos...\n");
    for (int i = 0; i < 200; i++) {
        adc_select_input(0);
        uint16_t valor_x = adc_read();
        adc_select_input(1);
        uint16_t valor_y = adc_read();

        if (valor_x < joystick_x_min) joystick_x_min = valor_x;
        if (valor_x > joystick_x_max) joystick_x_max = valor_x;
        if (valor_y < joystick_y_min) joystick_y_min = valor_y;
        if (valor_y > joystick_y_max) joystick_y_max = valor_y;

        sleep_ms(10);
    }
    printf("Calibracao finalizada.\n");
    printf("X Min: %d, X Max: %d, Y Min: %d, Y Max: %d\n", joystick_x_min, joystick_x_max, joystick_y_min, joystick_y_max);

    if (joystick_x_max - joystick_x_min < 1000) { // Corrige calibração incorreta
        joystick_x_min = 500;
        joystick_x_max = 3500;
    }
    if (joystick_y_max - joystick_y_min < 1000) {
        joystick_y_min = 500;
        joystick_y_max = 3500;
    }

    // Inicializa a posição do quadrado no centro
    int posicao_x = WIDTH / 2 - QUADRADO / 2;
    int posicao_y = HEIGHT / 2 - QUADRADO / 2;

    int last_pos_x = posicao_x;
    int last_pos_y = posicao_y;

    while (true) {
        adc_select_input(0);
        uint16_t valor_x = adc_read();
        uint16_t pwm_x = converter_adc_pwm(valor_x);

        adc_select_input(1);
        uint16_t valor_y = adc_read();
        uint16_t pwm_y = converter_adc_pwm(valor_y);

        if (leds_ativos) {
            pwm_set_gpio_level(LED_PIN_RED, pwm_x);
            pwm_set_gpio_level(LED_PIN_BLUE, pwm_y);
        } else {
            pwm_set_gpio_level(LED_PIN_RED, 0);
            pwm_set_gpio_level(LED_PIN_BLUE, 0);
        }

        int new_posicao_x = map_adc_to_screen(valor_x, 0);
        int new_posicao_y = map_adc_to_screen(valor_y, 1);

        // Atualiza posição apenas se não for -1
        if (new_posicao_x != -1) posicao_x = new_posicao_x;
        if (new_posicao_y != -1) posicao_y = new_posicao_y;

        // Apaga apenas o quadrado na posição antiga
        ssd1306_rect(&ssd, last_pos_x, last_pos_y, QUADRADO, QUADRADO, false, true);

        // Atualiza a posição do quadrado
        if (new_posicao_x != -1) posicao_x = new_posicao_x;
        if (new_posicao_y != -1) posicao_y = new_posicao_y;

        // Guarda a última posição para apagar corretamente no próximo loop
        last_pos_x = posicao_x;
        last_pos_y = posicao_y;

        // Atualiza apenas a borda se necessário
        atualizar_borda_display(&ssd, estilo_borda);

        // Desenha o quadrado na nova posição
        ssd1306_rect(&ssd, posicao_x, posicao_y, QUADRADO, QUADRADO, true, true);
        ssd1306_send_data(&ssd);

    }
}



