#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/i2c.h"

// Definições de pinos permanecem as mesmas
#define LED_PIN_RED 13
#define LED_PIN_BLUE 12
#define LED_PIN_GREEN 11
#define JOYSTICK_X 26
#define JOYSTICK_Y 27
#define BTN_JOYSTICK 22
#define BTN_A 5
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define WIDTH 128
#define HEIGHT 64
#define QUADRADO 8
#define ENDERECO 0x3C

// Variáveis globais permanecem as mesmas
#define DEBOUNCE_TIME 200000
static volatile uint32_t last_interrupt_time_joystick = 0;
static volatile uint32_t last_interrupt_time_A = 0;
bool estilo_borda = false;
bool leds_ativos = true;
bool led_verde_estado = false;
ssd1306_t ssd;
int joystick_x_min = 500, joystick_x_max = 3500;
int joystick_y_min = 500, joystick_y_max = 3500;
#define DEAD_ZONE 50

// Protótipos de funções
void atualizar_borda_display(ssd1306_t *ssd, bool estilo);
int map_adc_to_screen(int value, int is_y_axis);
void setup_pwm(uint PWM_PIN);
uint16_t converter_adc_pwm(uint16_t value);
static void gpio_irq_handler(uint gpio, uint32_t events);

void atualizar_borda_display(ssd1306_t *ssd, bool estilo) {
    ssd1306_fill(ssd, false);
    // Desenha as bordas horizontais
    for (int i = 0; i < WIDTH; i += (estilo ? 4 : 1)) {
        ssd1306_pixel(ssd, i, 0, true);
        ssd1306_pixel(ssd, i, HEIGHT - 1, true);
    }
    // Desenha as bordas verticais
    for (int i = 0; i < HEIGHT; i += (estilo ? 4 : 1)) {
        ssd1306_pixel(ssd, 0, i, true);
        ssd1306_pixel(ssd, WIDTH - 1, i, true);
    }
}

int map_adc_to_screen(int value, int is_y_axis) {
    int range = (is_y_axis ? joystick_y_max - joystick_y_min : joystick_x_max - joystick_x_min);
    if (range == 0) return 0;
    int mapped_value = ((value - (is_y_axis ? joystick_y_min : joystick_x_min)) * 
                       (is_y_axis ? HEIGHT - QUADRADO : WIDTH - QUADRADO)) / range;
    return mapped_value < 0 ? 0 : 
           (mapped_value > (is_y_axis ? HEIGHT - QUADRADO : WIDTH - QUADRADO) ? 
           (is_y_axis ? HEIGHT - QUADRADO : WIDTH - QUADRADO) : mapped_value);
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
    if (gpio == BTN_JOYSTICK && current_time - last_interrupt_time_joystick > DEBOUNCE_TIME) {
        last_interrupt_time_joystick = current_time;
        led_verde_estado = !led_verde_estado;
        gpio_put(LED_PIN_GREEN, led_verde_estado);
    }
    if (gpio == BTN_A && current_time - last_interrupt_time_A > DEBOUNCE_TIME) {
        last_interrupt_time_A = current_time;
        leds_ativos = !leds_ativos;
    }
}

int main() {
    stdio_init_all();
    
    // Inicializa I2C
    i2c_init(I2C_PORT, 400000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Inicializa ADC
    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);
    
    // Inicializa PWM e GPIO
    setup_pwm(LED_PIN_RED);
    setup_pwm(LED_PIN_BLUE);
    gpio_init(LED_PIN_GREEN);
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
    gpio_put(LED_PIN_GREEN, 0);
    
    // Inicializa botões com interrupções
    gpio_init(BTN_JOYSTICK);
    gpio_init(BTN_A);
    gpio_set_dir(BTN_JOYSTICK, GPIO_IN);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_JOYSTICK);
    gpio_pull_up(BTN_A);
    gpio_set_irq_enabled_with_callback(BTN_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    
    // Inicializa display OLED
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    
    int posicao_x = WIDTH / 2 - QUADRADO / 2;
    int posicao_y = HEIGHT / 2 - QUADRADO / 2;
    
    while (true) {
        // Limpa o display e atualiza a borda
        atualizar_borda_display(&ssd, estilo_borda);
        
        // Lê valores do joystick
        adc_select_input(0);
        uint16_t valor_x = adc_read();
        adc_select_input(1);
        uint16_t valor_y = adc_read();
        
        // Atualiza posição
        int new_x = map_adc_to_screen(valor_x, 0);
        int new_y = map_adc_to_screen(valor_y, 1);
        if (new_x != -1) posicao_x = new_x;
        if (new_y != -1) posicao_y = new_y;
        
        // Desenha o quadrado usando ssd1306_rect
        ssd1306_rect(&ssd, posicao_y, posicao_x, QUADRADO, QUADRADO, true, true);
        
        // Envia os dados para o display
        ssd1306_send_data(&ssd);
        
        // Atualiza LEDs se ativos
        if (leds_ativos) {
            pwm_set_gpio_level(LED_PIN_RED, converter_adc_pwm(valor_x));
            pwm_set_gpio_level(LED_PIN_BLUE, converter_adc_pwm(valor_y));
        } else {
            pwm_set_gpio_level(LED_PIN_RED, 0);
            pwm_set_gpio_level(LED_PIN_BLUE, 0);
        }
        
        sleep_ms(10);
    }
}








/*Controlar a intensidade luminosa dos LEDs RGB, onde:
• O LED Azul terá seu brilho ajustado conforme o valor do eixo Y. Quando o joystick estiver solto 
(posição central - valor 2048), o LED permanecerá apagado. À medida que o joystick for movido para 
cima (valores menores) ou para baixo (valores maiores), o LED aumentará seu brilho gradualmente, 
atingindo a intensidade máxima nos extremos (0 e 4095).
• O LED Vermelho seguirá o mesmo princípio, mas de acordo com o eixo X. Quando o joystick estiver 
solto (posição central - valor 2048), o LED estará apagado. Movendo o joystick para a esquerda 
(valores menores) ou para a direita (valores maiores), o LED aumentará de brilho, sendo mais intenso 
nos extremos (0 e 4095).
• Os LEDs serão controlados via PWM para permitir variação suave da intensidade luminosa.
Exibir no display SSD1306 um quadrado de 8x8 pixels, inicialmente centralizado, que se moverá 
proporcionalmente aos valores capturados pelo joystick.
Adicionalmente, o botão do joystick terá as seguintes funcionalidades:
• Alternar o estado do LED Verde a cada acionamento.
• Modificar a borda do display para indicar quando foi pressionado, alternando entre diferentes estilos 
de borda a cada novo acionamento.
Finalmente, o botão A terá a seguinte funcionalidade:
• Ativar ou desativar os LED PWM a cada acionamento*/