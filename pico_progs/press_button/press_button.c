#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/irq.h"
#include "hardware/timer.h"

typedef enum {
  Idle,
  ButtonPressed,
  ButtonReleased,
} TButtonState;
//-----------------------------------------------------------------------------
static TButtonState State;

// contador a ser decrementado pela interrupção
volatile uint32_t contador; 

// Intervalo de 1 ms (em microssegundos) para o Alarm 0
#define INTERVAL_US 1000u  


#define DEBOUNCE_MS 50   // período de debounce em milissegundos
#define PIN 5
#define PIN_LED 11

// Variáveis para comunicação ISR ↔ main
volatile bool button_event = false;
volatile uint32_t event_time = 0;


void __not_in_flash_func(timer0_isr)() {
    // 2.1 Limpa o flag de interrupção do Alarm 0 (bit 0 de timer_hw->intr)
    hw_clear_bits(&timer_hw->intr, 1u << 0);

    // 2.2 Agenda o próximo alarme somando INTERVAL_US ao contador de micros atual
    uint32_t agora = (uint32_t)timer_hw->timerawl;
    timer_hw->alarm[0] = agora + INTERVAL_US;

    // 2.3 Decrementa a variável global (se ainda não for zero)
    if (contador) {
        contador--;
    }
}


// ISR: só registra o instante e aciona a flag
void gpio_callback(uint gpio, uint32_t events) {
    event_time    = to_ms_since_boot(get_absolute_time());
    button_event  = true;
    printf("Interrompeu ...\n");
}

// Retorna true se já passou o intervalo de debounce
bool debounce_check(uint32_t event_time, uint32_t *last_time) {
    if (event_time - *last_time >= DEBOUNCE_MS) {
        *last_time = event_time;
        return true;
    }
    return false;
}

// Processa o evento “de verdade” (após debounce)
void process_button(uint pin, uint32_t event_time) {
    //static uint32_t last_time = 0;

   // // se ainda dentro do período morto, sai
    //if (!debounce_check(event_time, &last_time)) {
     //   return;
    //}

    // lê o nível atual e age conforme
    bool level = gpio_get(pin);
    if (!level) {
        printf("Botão pressionado\n");
        gpio_put(11, 1);
    } else {
        printf("Botão solto\n");
        gpio_put(11, 0);
    }
}

void ButtonPressRun()
{
    static uint32_t last_time = 0;
    bool level = gpio_get(PIN);
    switch (State)
    {
    case Idle:
        if (button_event & debounce_check(event_time, &last_time)) {
            if (level) {
                State = ButtonPressed;
                //button_event = false;
                gpio_put(PIN_LED, 1);
                printf("Idle: Pressed\n");
            } 
        } else button_event = false;
        break;

    case ButtonPressed:
        if (button_event) { //& debounce_check(event_time, &last_time)) {
            if (!level) {
                State = ButtonReleased;
                //button_event = false;
                gpio_put(PIN_LED, 0);
                printf("Pressed: Released\n");
            } 
        } else button_event = false;
        break;
    case ButtonReleased:
        State = Idle;
        button_event = false;
        event_time = 0;
        last_time = 0;
        gpio_put(PIN_LED, 0);
        printf("Released: Released\n");
        break;
    default:
        break;
    }
}

int main() {
    stdio_init_all();
//    const uint PIN = 5;

    // configuração do GPIO
    gpio_init(PIN);
    gpio_set_dir(PIN, GPIO_IN);
    gpio_pull_up(PIN);

    gpio_init(11);           // configura o pino
    gpio_set_dir(11, GPIO_OUT); // define como saída
    gpio_put(11, 0);

    // habilita interrupções em ambas as bordas
    gpio_set_irq_enabled_with_callback(
        PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true,
        &gpio_callback
    );

    while (1) {
        //printf("Espera botão\n");
        //if (button_event) {
        //    printf("Dentro do Botão\n");
        //    button_event = false;                // limpa a flag
            //process_button(PIN, event_time);     // trata com debounce
        //}
        //printf("Saiu Espera botão\n");
        ButtonPressRun();
      //  printf("... ");
        //tight_loop_contents();
    }
}
