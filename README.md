# BitDogLab - Controle de LEDs RGB e Display com Joystick

Este projeto foi desenvolvido para a Unidade 4 do Curso EmbarcaTech explorando o uso de conversores analógico-digitais (ADC) no RP2040 e as funcionalidades da placa de desenvolvimento BitDogLab. O objetivo principal é demonstrar o controle de LEDs RGB e de um quadrado no display SSD1306 utilizando um joystick, juntamente com a funcionalidade dos botões A e do joystick.

## Vídeo de Demonstração

Você pode assistir à demonstração do projeto no seguinte link:

[Link do Vídeo de Demonstração](https://drive.google.com/file/d/1tWw8ersNzSI5uDFYuPic-j0S8sQ2Fisn/view?usp=drive_link)

## Descrição do Projeto

O joystick fornece valores analógicos correspondentes aos eixos X e Y, que são utilizados para:

*   **Controlar a intensidade luminosa dos LEDs RGB:**
    *   O LED Azul tem seu brilho ajustado conforme o valor do eixo Y.
    *   O LED Vermelho tem seu brilho ajustado conforme o valor do eixo X.
    *   O controle dos LEDs é feito via PWM para permitir variação suave da intensidade luminosa.
*   **Exibir no display SSD1306 um quadrado de 8x8 pixels** que se move proporcionalmente aos valores capturados pelo joystick.
*   O botão do joystick tem as seguintes funcionalidades:
    *   Alternar o estado do LED Verde a cada acionamento.
    *   Modificar a borda do display para indicar quando foi pressionado, alternando entre diferentes estilos de borda a cada novo acionamento.
*   O botão A tem a seguinte funcionalidade:
    *   Ativar ou desativar os LEDs PWM a cada acionamento.

## Componentes Utilizados

*   Placa BitDogLab
*   LED RGB (GPIOs 11, 12 e 13)
*   Botão do Joystick (GPIO 22)
*   Joystick (GPIOs 26 e 27)
*   Botão A (GPIO 5)
*   Display SSD1306 via I2C (GPIO 14 e GPIO15)

## Instruções de Utilização

Para compilar e executar este projeto, siga as seguintes instruções:

1.  **Instale o SDK do Raspberry Pi Pico:**
    *   Certifique-se de ter o SDK do Raspberry Pi Pico instalado no seu ambiente de desenvolvimento. 

2.  **Clone o repositório:**
    *   Clone este repositório para o seu computador:
      
3.  **Configure o ambiente de build:**
    *   Configure as variáveis de ambiente para o SDK do Pico.

4.  **Compile o projeto:**
    *   Crie um diretório de build e execute o CMake para gerar os arquivos de build:

5.  **Conecte a placa BitDogLab:**
    *   Conecte a placa BitDogLab ao seu computador via USB no modo bootloader (segurando o botão BOOTSEL ao conectar).

6.  **Flash o programa:**
    *   Copie o arquivo `*.uf2` gerado no diretório `build` para a placa BitDogLab, que aparecerá como um disco removível.

7.  **Execute o projeto:**
    *   Após a cópia, a placa irá reiniciar e o programa será executado automaticamente.
