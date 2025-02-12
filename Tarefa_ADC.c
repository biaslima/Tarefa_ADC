#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/i2c.h"

// Definições de pinos 
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

// Configurações de tempo e debounce
#define DEBOUNCE_TIME 200000
#define CALIBRATION_SAMPLES 100
#define DEAD_ZONE_PERCENT 5

// Variáveis globais
static volatile uint32_t last_interrupt_time_joystick = 0;
static volatile uint32_t last_interrupt_time_A = 0;
bool estilo_borda = false;
bool leds_ativos = true;
bool led_verde_estado = false;
ssd1306_t ssd;

// Variáveis para controle do quadrado
static int posicao_atual_x;
static int posicao_atual_y;
static int posicao_alvo_x;
static int posicao_alvo_y;

// Estrutura para calibração do joystick
typedef struct {
    int x_min, x_max, x_center;
    int y_min, y_max, y_center;
    int dead_zone_x, dead_zone_y;
} joystick_calibration_t;

static joystick_calibration_t joystick_cal;

// Função para movimento suave
int mover_suave(int atual, int alvo) {
    if (atual == alvo) return atual;
    
    // Move 30% da distância
    int diff = alvo - atual;
    return atual + (diff / 3);
}

// Função de calibração do joystick
void calibrar_joystick(void) {
    // Para o Wokwi, usamos valores fixos de calibração
    joystick_cal.x_center = 2048;  // Centro do eixo X
    joystick_cal.y_center = 2048;  // Centro do eixo Y
    joystick_cal.dead_zone_x = 100;  // Zona morta reduzida
    joystick_cal.dead_zone_y = 100;  // Zona morta reduzida
    
    printf("Calibração para Wokwi:\nX: center=%d, dead_zone=%d\nY: center=%d, dead_zone=%d\n",
           joystick_cal.x_center, joystick_cal.dead_zone_x,
           joystick_cal.y_center, joystick_cal.dead_zone_y);
}

bool is_in_dead_zone(int value, int center, int dead_zone) {
    return abs(value - center) < dead_zone;
}

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

int map_adc_to_screen(int value, int min_val, int max_val, int screen_max) {
    // Inverte o mapeamento para corrigir a direção
    value = max_val - value;
    
    // Define o centro e a faixa de movimento
    int center_screen = screen_max / 2 - QUADRADO / 2;
    int half_range = (screen_max - QUADRADO) / 2;
    
    // Calcula o deslocamento em relação ao centro do ADC
    int offset = ((long)(value - 2048) * half_range) / 2048;
    
    // Aplica o deslocamento ao centro da tela
    int resultado = center_screen + offset;
    
    // Garante limites
    if (resultado < 0) resultado = 0;
    if (resultado > screen_max - QUADRADO) resultado = screen_max - QUADRADO;
    
    return resultado;
}

void setup_pwm(uint PWM_PIN) {
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PWM_PIN);
    pwm_set_wrap(slice, 255);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(PWM_PIN), 0);
    pwm_set_enabled(slice, true);
}

uint16_t converter_adc_pwm(uint16_t value, uint16_t center, uint16_t dead_zone) {
    // Se estiver na zona morta, LED apagado
    if (is_in_dead_zone(value, center, dead_zone)) {
        return 0;
    }
    
    // Para simulação no Wokwi, invertemos a lógica e ajustamos os valores
    if (value > center) {
        // Movimento para direita/baixo (valores maiores)
        uint32_t diff = value - center;
        uint32_t range = 4095 - center;
        return (diff * 255) / range;
    } else {
        // Movimento para esquerda/cima (valores menores)
        uint32_t diff = center - value;
        uint32_t range = center;
        return (diff * 255) / range;
    }
}

static void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    
    if (gpio == BTN_JOYSTICK && current_time - last_interrupt_time_joystick > DEBOUNCE_TIME) {
        last_interrupt_time_joystick = current_time;
        led_verde_estado = !led_verde_estado;
        gpio_put(LED_PIN_GREEN, led_verde_estado);
        estilo_borda = !estilo_borda;
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
    
    // Calibra o joystick
    printf("Iniciando calibração do joystick...\n");
    calibrar_joystick();
    
    // Inicializa PWM e GPIO
    setup_pwm(LED_PIN_RED);
    setup_pwm(LED_PIN_BLUE);
    gpio_init(LED_PIN_GREEN);
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
    gpio_put(LED_PIN_GREEN, 0);

    pwm_set_gpio_level(LED_PIN_RED, 0);
    pwm_set_gpio_level(LED_PIN_BLUE, 0);
    
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

    // Garante que os LEDs começam apagados
    pwm_set_gpio_level(LED_PIN_RED, 0);
    pwm_set_gpio_level(LED_PIN_BLUE, 0);
    gpio_put(LED_PIN_GREEN, 0);
    
    // Define posição inicial no centro
    posicao_atual_x = WIDTH / 2 - QUADRADO / 2;
    posicao_atual_y = HEIGHT / 2 - QUADRADO / 2;
    posicao_alvo_x = posicao_atual_x;
    posicao_alvo_y = posicao_atual_y;
    
    // Aguarda um momento para estabilizar
    sleep_ms(100);
    
    while (true) {
        // Limpa o display e atualiza a borda
        atualizar_borda_display(&ssd, estilo_borda);
        
        // Lê valores do joystick
        adc_select_input(0);
        uint16_t valor_x = adc_read();
        adc_select_input(1);
        uint16_t valor_y = adc_read();
        
        // Atualiza posições alvo
        if (is_in_dead_zone(valor_x, joystick_cal.x_center, joystick_cal.dead_zone_x)) {
            posicao_alvo_x = WIDTH / 2 - QUADRADO / 2;
        } else {
            posicao_alvo_x = map_adc_to_screen(valor_x, 0, 4095, WIDTH);
        }
        
        if (is_in_dead_zone(valor_y, joystick_cal.y_center, joystick_cal.dead_zone_y)) {
            posicao_alvo_y = HEIGHT / 2 - QUADRADO / 2;
        } else {
            posicao_alvo_y = map_adc_to_screen(valor_y, 0, 4095, HEIGHT);
        }
        
        // Atualiza posições atuais
        posicao_atual_x = mover_suave(posicao_atual_x, posicao_alvo_x);
        posicao_atual_y = mover_suave(posicao_atual_y, posicao_alvo_y);
        
        // Desenha o quadrado
        ssd1306_rect(&ssd, posicao_atual_y, posicao_atual_x, QUADRADO, QUADRADO, true, true);
        ssd1306_send_data(&ssd);
        
        // Atualiza LEDs se ativos
        if (leds_ativos) {
            uint16_t pwm_x = converter_adc_pwm(valor_x, joystick_cal.x_center, joystick_cal.dead_zone_x);
            uint16_t pwm_y = converter_adc_pwm(valor_y, joystick_cal.y_center, joystick_cal.dead_zone_y);
            pwm_set_gpio_level(LED_PIN_RED, pwm_x);
            pwm_set_gpio_level(LED_PIN_BLUE, pwm_y);
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