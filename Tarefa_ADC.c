// Inclusão das bibliotecas necessárias
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/i2c.h"

//Atribuição de pinos
#define LED_PIN_RED 13    
#define LED_PIN_BLUE 12        
#define LED_PIN_GREEN 11       
#define JOYSTICK_PIN_X 26      
#define JOYSTICK_PIN_Y 27      
#define JOYSTICK_PIN_BTN 22  
#define BUTTON_PIN_A 5          
#define TAMANHO_QUADRADO 8      
#define ENDERECO_DISPLAY 0x3C   
#define CENTRO_JOYSTICK 2048    // Valor central do joystick 
#define ZONA_MORTA 100          // Zona morta do joystick (evita movimento fantasma)

// Variáveis globais
static volatile uint32_t last_interrupt_time_joystick = 0;  
static volatile uint32_t last_interrupt_time_A = 0;  
bool estilo_borda = false;      
bool leds_ligados = true;       
bool led_verde_ligado = false;  
ssd1306_t display;             // Variável pra controlar o display

// Variáveis da posição do quadrado
static int posicao_x_atual;     
static int posicao_y_atual;     
static int posicao_x_alvo;      
static int posicao_y_alvo;      

// Função pra suavisar o movimento do quadradp
int movimento_suave(int posicao_atual, int posicao_alvo) {
    if (posicao_atual == posicao_alvo) {
        return posicao_atual;
    }
    return posicao_atual + ((posicao_alvo - posicao_atual) / 3); //Move o quadrado de 30 em 30% para a posição alvo
}

// Função pra desenhar borda
void desenhar_borda(bool estilo_pontilhado) {
    ssd1306_fill(&display, false); 
    
    // Desenha as linhas horizontais (em cima e embaixo)
    for (int i = 0; i < WIDTH; i += (estilo_pontilhado ? 4 : 1)) {
        ssd1306_pixel(&display, i, 0, true);          
        ssd1306_pixel(&display, i, HEIGHT - 1, true); 
    }
    
    // Desenha as linhas verticais (esquerda e direita)
    for (int i = 0; i < HEIGHT; i += (estilo_pontilhado ? 4 : 1)) {
        ssd1306_pixel(&display, 0, i, true);         
        ssd1306_pixel(&display, WIDTH - 1, i, true); 
    }
}

// Função que converte a posição do joystick para a posição na tela
int converter_posicao_display(int valor_joystick, int tamanho_tela) {
    //Inversão do eixo Y
    if (tamanho_tela == HEIGHT) {
        valor_joystick = 4095 - valor_joystick;
    }
    
    int posicao_centro = tamanho_tela / 2 - TAMANHO_QUADRADO / 2;
    int alcance = (tamanho_tela - TAMANHO_QUADRADO) / 2;
    
    int posicao = posicao_centro + ((long)(valor_joystick - CENTRO_JOYSTICK) * alcance) / CENTRO_JOYSTICK;
    
    // Garante que o quadrado só se mova na tela
    if (posicao < 0) 
        return 0;
    if (posicao > tamanho_tela - TAMANHO_QUADRADO) 
        return tamanho_tela - TAMANHO_QUADRADO;
    return posicao;
}

// Função LED(PWM) - JOYSTICK
uint16_t calcular_brilho_led(uint16_t valor_joystick) {
    // Apagar LED na zona morta
    if (abs(valor_joystick - CENTRO_JOYSTICK) < ZONA_MORTA) 
        return 0;
    
    // Calcula o brilho do LED
    uint32_t diff = valor_joystick > CENTRO_JOYSTICK ? valor_joystick - CENTRO_JOYSTICK : CENTRO_JOYSTICK - valor_joystick;
    uint32_t alcance = valor_joystick > CENTRO_JOYSTICK ? 4095 - CENTRO_JOYSTICK : CENTRO_JOYSTICK;
                      
    return (diff * 255) / alcance; //Garante que o LED aumente nas extremidades
}

// Função de interrupção dos botões
static void buttons_callback(uint gpio, uint32_t events) {

    uint32_t current_time = to_us_since_boot(get_absolute_time());
    
    // Callback do JOYSTICK
    if (gpio == JOYSTICK_PIN_BTN && current_time - last_interrupt_time_joystick > 200000) {
        last_interrupt_time_joystick = current_time;
        led_verde_ligado = !led_verde_ligado;
        gpio_put(LED_PIN_GREEN, led_verde_ligado);
        estilo_borda = !estilo_borda;
    }
    // Callback botão A
    else if (gpio == BUTTON_PIN_A && current_time - last_interrupt_time_A > 200000) {
        last_interrupt_time_A = current_time;
        leds_ligados = !leds_ligados;
    }
}

void setup() {
    stdio_init_all();
    
    // Inicializa I2C 
    i2c_init(i2c1, 400000);
    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);
    gpio_pull_up(14);
    gpio_pull_up(15);
    
    // Inicializa ADC 
    adc_init();
    adc_gpio_init(JOYSTICK_PIN_X);
    adc_gpio_init(JOYSTICK_PIN_Y);
    
    // Inicializa os LEDs
    gpio_init(LED_PIN_GREEN);
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
    gpio_put(LED_PIN_GREEN, 0);
    
    // Configura os LEDs PWM 
    gpio_set_function(LED_PIN_RED, GPIO_FUNC_PWM);
    gpio_set_function(LED_PIN_BLUE, GPIO_FUNC_PWM);
    
    uint slice_vermelho = pwm_gpio_to_slice_num(LED_PIN_RED);
    uint slice_azul = pwm_gpio_to_slice_num(LED_PIN_BLUE);
    
    pwm_set_wrap(slice_vermelho, 255);
    pwm_set_wrap(slice_azul, 255);
    
    pwm_set_enabled(slice_vermelho, true);
    pwm_set_enabled(slice_azul, true);
    
    // Inicializa os botões
    gpio_init(JOYSTICK_PIN_BTN);
    gpio_init(BUTTON_PIN_A);
    gpio_set_dir(JOYSTICK_PIN_BTN, GPIO_IN);
    gpio_set_dir(BUTTON_PIN_A, GPIO_IN);
    gpio_pull_up(JOYSTICK_PIN_BTN);
    gpio_pull_up(BUTTON_PIN_A);
    
    // Configura as interrupções 
    gpio_set_irq_enabled_with_callback(JOYSTICK_PIN_BTN, GPIO_IRQ_EDGE_FALL, true, buttons_callback);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN_A, GPIO_IRQ_EDGE_FALL, true, buttons_callback);
    
    // Inicializa o display
    ssd1306_init(&display, WIDTH, HEIGHT, false, ENDERECO_DISPLAY, i2c1);
    ssd1306_config(&display);
    
    // Define a posição inicial do quadrado no centro da tela
    posicao_x_atual = posicao_x_alvo = WIDTH / 2 - TAMANHO_QUADRADO / 2;
    posicao_y_atual = posicao_y_alvo = HEIGHT / 2 - TAMANHO_QUADRADO / 2;
}

int main() {
    setup();
    sleep_ms(100);  // Pequena pausa para estabilização
    
    // Loop principal
    while (true) {
        // Desenha a borda
        desenhar_borda(estilo_borda);
        
        // Lê os valores do joystick
        adc_select_input(1);
        uint16_t valor_x = adc_read();
        adc_select_input(0);
        uint16_t valor_y = adc_read();
        
        // Atualiza as posições alvo do quadrado
        if (abs(valor_x - CENTRO_JOYSTICK) < ZONA_MORTA) {
            posicao_x_alvo = WIDTH / 2 - TAMANHO_QUADRADO / 2;  // Centraliza se joystick parado
        } else {
            posicao_x_alvo = converter_posicao_display(valor_x, WIDTH);
        }

        if (abs(valor_y - CENTRO_JOYSTICK) < ZONA_MORTA) {
            posicao_y_alvo = HEIGHT / 2 - TAMANHO_QUADRADO / 2;  // Centraliza se joystick parado
        } else {
            posicao_y_alvo = converter_posicao_display(valor_y, HEIGHT);
        }
        
        // Move quadrado 
        posicao_x_atual = movimento_suave(posicao_x_atual, posicao_x_alvo);
        posicao_y_atual = movimento_suave(posicao_y_atual, posicao_y_alvo);
        
        // Desenha o quadrado 
        ssd1306_rect(&display, posicao_y_atual, posicao_x_atual, TAMANHO_QUADRADO, TAMANHO_QUADRADO, true, true);
        ssd1306_send_data(&display);
        
        // Atualiza os LEDs 
        if (leds_ligados) {
            pwm_set_gpio_level(LED_PIN_RED, calcular_brilho_led(valor_x));
            pwm_set_gpio_level(LED_PIN_BLUE, calcular_brilho_led(valor_y));
        } else {
            pwm_set_gpio_level(LED_PIN_RED, 0);
            pwm_set_gpio_level(LED_PIN_BLUE, 0);
        }
        
        sleep_ms(10);  
    }
}