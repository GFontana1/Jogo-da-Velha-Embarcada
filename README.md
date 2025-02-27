# Jogo-da-Velha-Embarcada
Projeto final do Embarcatech.

## Descrição do projeto

O programa permite ao usuário jogar um jogo da velha em um display SSD1306 conectado a uma placa Raspberry Pi Pico W ao utilizar:

| Componente | Pino no Pico 
| -----------|-------------
| Botão A | GPIO5
| Botão B | GPIO6
| Buzzer | GPIO21
| LED Azul | GPIO12
| LED Verde | GPIO11
| LED Vermelho | GPIO13
| Matriz WS2812 | GPIO7
| Display SSD1306 SDA | GPIO14
| Display SSD1306 SCL | GPIO15


## Funcionalidades

- Exibição do jogo feita a partir do display

- Os jogadores podem se mover pelas células do jogo da velha ao pressionar o Botão B

- Ao pressionar o Botão A, o 'X' ou 'O' é inserido na célula desejada

- Se um jogador tentar reutilizar a mesma célula, é aceso o LED Vermelho e o buzzer toca um som para avisar que a célula já está sendo utilizada

- Vence o jogador que completar uma linha (seja ela vertical, horizontal ou diagonal) com o símbolo de sua escolha (X ou O)

- Caso o jogador X vença, a matriz de LEDs mostra um X utilizando LEDs azuis, o LED central da placa acende na cor azul e o buzzer é ativado

- Caso o jogador O vença, a matriz de LEDs mostra um O utilizando LEDs verdes, o LED central da placa acende na cor verde e o buzzer é ativado

- Caso o jogo empate, a matriz de LEDs mostra um V utilizando LEDs vermelhos, o LED central da placa acende na cor vermelha e o buzzer é ativado

- Após o jogo ser encerrado, é possível jogar novamente ao pressionar o Botão A 2 vezes.


## Como rodar o código

1. *Necessário para compilar o código:*
    - Ter uma placa Raspberry Pi Pico W que contenha os componentes descritos na *Descrição do projeto*.
    - Ter o SDK do Raspberry Pi Pico W configurado.
    - Compilar o código utilizando CMake e GCC ARM.
    - Caso queira rodar no simulador do Wokwi, é necessário uma licença do mesmo. Tendo a licença, basta abrir o arquivo `diagram.json` pelo VSCode e clicar no botão verde que fica no canto superior esquerdo da interface.

2. *Transferir para o Raspberry Pi Pico W*:
   - Conecte sua placa ao computador com ela no modo **Bootsel**.
   - Monte a unidade **RPI-RP2** no computador.
   - Copie o arquivo compilado `.uf2` para a unidade montada ou aperte em Run na interface do VSCode caso tenha configurado a placa com o Zadig.
  
