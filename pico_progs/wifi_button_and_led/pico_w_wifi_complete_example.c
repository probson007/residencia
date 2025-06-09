/* Servidor Http para para tratar envio de comandos e leitura de estado
   Sobre o sistema
   
   O sistema permite o controle remoto de três LEDs (azul, vermelho e verde), 
   que podem ser ligados, desligados ou configurados para oscilar em intervalos 
   de tempo específicos (1 ou 5 segundos). Além disso, o projeto inclui um buzzer 
   que pode ser ativado com frequências e durações personalizadas, proporcionando 
   feedback sonoro para o usuário. Outra funcionalidade importante é o monitoramento 
   de dois botões físicos, cujo estado (pressionado ou solto) é exibido na interface web.

   ***** Necessário configurar antes de rodar o programa: *****
     Definir o nome da rede Wifi e a senha:

     #define WIFI_SSID "nome da rede wifi"  // Substitua pelo nome da sua rede Wi-Fi
     #define WIFI_PASS "senha" // Substitua pela senha da sua rede Wi-Fi
     
     Definindo o IP estático?:

     #define STATIC_IP "192.168.0.50"   // defina IP fixo
     #define NETMASK "255.255.255.0"    // defina Máscara de sub-rede
     #define GATEWAY "192.168.0.1"      // defina Gateway padrão

*/

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "lwip/tcp.h"
#include "lwip/ip_addr.h" // Para manipular endereços IP
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define LED_PIN 12         // Pino do LED azul
#define RED_LED_PIN 13     // Pino do LED vermelho
#define GREEN_LED_PIN 11   // Pino do LED verde
#define BUTTON1_PIN 5      // Pino do botão 1
#define BUTTON2_PIN 6      // Pino do botão 2
#define BUZZER_PIN 21      // Pino do Buzzer
#define WIFI_SSID "nome da rede wifi"  // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASS "senha da rede wifi" // Substitua pela senha da sua rede Wi-Fi

#define STATIC_IP "192.168.0.50"   // defina IP fixo
#define NETMASK "255.255.255.0"    // defina Máscara de sub-rede
#define GATEWAY "192.168.0.1"      // defina Gateway padrão

// Estado dos LEDs para oscilção
bool blue_led_oscillate = false;
bool red_led_oscillate = false;
bool green_led_oscillate = false;
int blue_led_interval = 0;
int red_led_interval = 0;
int green_led_interval = 0;

// Estado dos botões (inicialmente sem mensagens)
char button1_message[50] = "Nenhum evento no botão 1";
char button2_message[50] = "Nenhum evento no botão 2";

// Buffer para resposta HTTP
char http_response[4096];

// configura o pino do Buzzer com o PWM
void init_buzzer() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_enabled(slice_num, false);
}

// define a frequencia do Buzzer e habilita o Buzzer
void play_buzzer(uint32_t frequency, uint32_t duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    uint32_t clock = 125000000; // Frequência do clock do sistema
    uint32_t divider = clock / (frequency * 100);

    pwm_set_wrap(slice_num, 100 - 1);
    pwm_set_clkdiv(slice_num, divider);

    pwm_set_gpio_level(BUZZER_PIN, 50); // Duty de 50%
    pwm_set_enabled(slice_num, true);

    busy_wait_ms(duration_ms);
    pwm_set_enabled(slice_num, false);
}

// Função para criar a resposta HTTP ao ser aberto no Navegador
// apontando para o IP fixo definido no início do código
void create_http_response() {
     snprintf(http_response, sizeof(http_response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "  <meta http-equiv='refresh' content='1' charset=\"UTF-8\">" 
             "  <title>Controle do LED, Buzzer e Botões</title>"
             "  <style>"
             "    body { text-align: center; font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; }"
             "    h1 { color: #333; margin-top: 20px; }"
             "    button { padding: 10px 20px; margin: 10px; font-size: 16px; border: none; border-radius: 5px; cursor: pointer; }"
             "    .blue { background-color: #007BFF; color: white; }"
             "    .red { background-color: #DC3545; color: white; }"
             "    .green { background-color: #28A745; color: white; }"
             "    .buzzer { background-color: #FFC107; color: black; }"
             "    .oscillate-blue { background-color: #0056b3; color: white; }"
             "    .oscillate-red { background-color: #a71d2a; color: white; }"
             "    .oscillate-green { background-color: #19692c; color: white; }"
             "    .refresh { background-color: #17A2B8; color: white; }"
             "  </style>"
             "</head>"
             "<body>"
             "   <div class=\"header\">"
             //"    <img src=\"https://ifce.edu.br/noticias/ifce-integra-capacitacao-nacional-em-sistemas-embarcados/captura-de-tela-2024-07-08-141318-1.jpg/@@images/a46e3b52-3b0c-450e-afa2-353600ffb4c2.jpeg\" alt=\"Logo IFCE\">"
             "    <h1>Web Server Para Teste de Placa - Projeto Embarcatech</h1>"
             "  </div>"
             "  <div>"
             "  <h1>Controle do LED, Buzzer e Botões</h1>"
             "  <div>"
             "    <button class=\"blue\" onclick=\"location.href='/led/on'\">Ligar LED</button>"
             "    <button class=\"blue\" onclick=\"location.href='/led/off'\">Desligar LED</button>"
             "    <button class=\"oscillate-blue\" onclick=\"location.href='/led/oscillate?interval=1'\">Oscilar LED (1s)</button>"
             "    <button class=\"oscillate-blue\" onclick=\"location.href='/led/oscillate?interval=5'\">Oscilar LED (5s)</button>"
             "  </div>"
             "  <div>"
             "    <button class=\"red\" onclick=\"location.href='/red_led/on'\">Ligar LED</button>"
             "    <button class=\"red\" onclick=\"location.href='/red_led/off'\">Desligar LED</button>"
             "    <button class=\"oscillate-red\" onclick=\"location.href='/red_led/oscillate?interval=1'\">Oscilar LED (1s)</button>"
             "    <button class=\"oscillate-red\" onclick=\"location.href='/red_led/oscillate?interval=5'\">Oscilar LED (5s)</button>"
             "  </div>"
             "  <div>"
             "    <button class=\"green\" onclick=\"location.href='/green_led/on'\">Ligar LED</button>"
             "    <button class=\"green\" onclick=\"location.href='/green_led/off'\">Desligar LED</button>"
             "    <button class=\"oscillate-green\" onclick=\"location.href='/green_led/oscillate?interval=1'\">Oscilar LED (1s)</button>"
             "    <button class=\"oscillate-green\" onclick=\"location.href='/green_led/oscillate?interval=5'\">Oscilar LED (5s)</button>"
             "  </div>"
             "  <div>"
             "    <button class=\"buzzer\" onclick=\"location.href='/buzzer?frequency=1000&duration=1000'\">Ativar Buzzer (1kHz)</button>"
             "  </div>"
             "  <div>"
             "    <button class=\"refresh\" onclick=\"location.href='/update_buttons'\">Monitorar Estado dos Botões</button>"
             "  </div>"
             "  <h2>Estado dos Botões:</h2>"
             "  <p>Botão 1: %s</p>"
             "  <p>Botão 2: %s</p>"
             "</body>"
             "</html>\r\n",
             button1_message, button2_message);
}


// Função para monitorar o estado dos botões
void monitor_buttons() {
    static bool button1_last_state = false;
    static bool button2_last_state = false;

    bool button1_state = !gpio_get(BUTTON1_PIN); // Botão pressionado = LOW
    bool button2_state = !gpio_get(BUTTON2_PIN);

    if (button1_state != button1_last_state) {
        button1_last_state = button1_state;
        if (button1_state) {
           // snprintf(button1_message, sizeof(button1_message), "Botão 1 foi pressionado!");
            printf("Botão 1 pressionado\n");         
            strcpy(button1_message,"Botão 1 foi pressionado!"); 
            printf(button1_message);
            
        } else {
            //snprintf(button1_message, sizeof(button1_message), "Botão 1 foi solto!");
            printf("Botão 1 solto\n");
            strcpy(button1_message,"Botão 1 foi solto!");
            printf(button2_message);
        }
    }

    if (button2_state != button2_last_state) {
        button2_last_state = button2_state;
        if (button2_state) {
            snprintf(button2_message, sizeof(button2_message), "Botão 2 foi pressionado!");
            printf("Botão 2 pressionado\n");
            strcpy(button2_message,"Botão 2 foi pressionado!"); 
            printf(button2_message);
        } else {
            snprintf(button2_message, sizeof(button2_message), "Botão 2 foi solto!");           
            strcpy(button2_message,"Botão 2 foi solto!");
            printf(button2_message);
        }
    }
}


// Função para alternar LEDs em intervalos
void handle_led_oscillation() {
    static absolute_time_t last_blue_toggle;
    static absolute_time_t last_red_toggle;
    static absolute_time_t last_green_toggle;

    if (blue_led_oscillate && absolute_time_diff_us(last_blue_toggle, get_absolute_time()) >= blue_led_interval * 1000000) {
        gpio_put(LED_PIN, !gpio_get(LED_PIN));
        last_blue_toggle = get_absolute_time();
    }
    if (red_led_oscillate && absolute_time_diff_us(last_red_toggle, get_absolute_time()) >= red_led_interval * 1000000) {
        gpio_put(RED_LED_PIN, !gpio_get(RED_LED_PIN));
        last_red_toggle = get_absolute_time();
    }
    if (green_led_oscillate && absolute_time_diff_us(last_green_toggle, get_absolute_time()) >= green_led_interval * 1000000) {
        gpio_put(GREEN_LED_PIN, !gpio_get(GREEN_LED_PIN));
        last_green_toggle = get_absolute_time();
    }
}

// Função de callback para processar requisições HTTP
// processa as interações no navegador e conforme o botão pressionado,
// a resposta http devolvida ao Micro é processada abaixo e o comando executado na placa
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        // Cliente fechou a conexão
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Processa a requisição HTTP
    char *request = (char *)p->payload;

    if (strstr(request, "GET /led/on")) {
        gpio_put(LED_PIN, 1);
        blue_led_oscillate = false;
    } else if (strstr(request, "GET /led/off")) {
        gpio_put(LED_PIN, 0);
        blue_led_oscillate = false;
    } else if (strstr(request, "GET /red_led/on")) {
        gpio_put(RED_LED_PIN, 1);
        red_led_oscillate = false;
    } else if (strstr(request, "GET /red_led/off")) {
        gpio_put(RED_LED_PIN, 0);
        red_led_oscillate = false;
    } else if (strstr(request, "GET /green_led/on")) {
        gpio_put(GREEN_LED_PIN, 1);
        green_led_oscillate = false;
    } else if (strstr(request, "GET /green_led/off")) {
        gpio_put(GREEN_LED_PIN, 0);
        green_led_oscillate = false;
    } else if (strstr(request, "GET /led/oscillate?interval=1")) {
        blue_led_oscillate = true;
        blue_led_interval = 1;
    } else if (strstr(request, "GET /led/oscillate?interval=5")) {
        blue_led_oscillate = true;
        blue_led_interval = 5;
    } else if (strstr(request, "GET /red_led/oscillate?interval=1")) {
        red_led_oscillate = true;
        red_led_interval = 1;
    } else if (strstr(request, "GET /red_led/oscillate?interval=5")) {
        red_led_oscillate = true;
        red_led_interval = 5;
    } else if (strstr(request, "GET /green_led/oscillate?interval=1")) {
        green_led_oscillate = true;
        green_led_interval = 1;
    } else if (strstr(request, "GET /green_led/oscillate?interval=5")) {
        green_led_oscillate = true;
        green_led_interval = 5;
    } else if (strstr(request, "GET /update_buttons")) {
        monitor_buttons();
    } else if (strstr(request, "GET /buzzer")) {
        uint32_t frequency = 1000;
        uint32_t duration = 2000;      
        if (duration > 10000) duration = 10000;
        play_buzzer(frequency, duration);
    }


    // Atualiza o conteúdo da página
    create_http_response();

    // Envia a resposta HTTP
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);

    // Libera o buffer recebido
    pbuf_free(p);

    return ERR_OK;
}


// Callback de conexão: associa o http_callback à conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);  // Associa o callback HTTP
    return ERR_OK;
}

// Função de setup e inicia do servidor web
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }

    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }

    pcb = tcp_listen(pcb);  // Coloca o PCB em modo de escuta
    tcp_accept(pcb, connection_callback);  // Associa o callback de conexão

    printf("Servidor HTTP rodando no IP %s, porta 80...\n", STATIC_IP);
}

void configure_static_ip() {
    ip4_addr_t ipaddr, netmask, gw;

    // Converte os endereços IP para o formato binário
    ip4addr_aton(STATIC_IP, &ipaddr);
    ip4addr_aton(NETMASK, &netmask);
    ip4addr_aton(GATEWAY, &gw);

    // Configura o IP estático na interface do dispositivo
    netif_set_addr(cyw43_state.netif, &ipaddr, &netmask, &gw);
}

int main() {
    stdio_init_all();
    sleep_ms(10000);
    printf("Iniciando servidor HTTP\n");

    if (cyw43_arch_init()) { // Liga e checa a inicialização do Wi-fi
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode(); // busca redes WI-Fi 

    // faz a conexão com a rede Wi-fi definida no começo do programa
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    }
    // configura o IP estático
    configure_static_ip();

    printf("Wi-Fi conectado com IP %s\n", STATIC_IP);
    
    // configuram os pinos dos LEDs RGB como saída
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(RED_LED_PIN);
    gpio_set_dir(RED_LED_PIN, GPIO_OUT);

    gpio_init(GREEN_LED_PIN);
    gpio_set_dir(GREEN_LED_PIN, GPIO_OUT);

    gpio_init(BUTTON1_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_pull_up(BUTTON1_PIN);

    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_pull_up(BUTTON2_PIN);
     
    // configuração inicial do Buzzer
    init_buzzer();
    
    // inicia o servidor htto da aplicação Raspberry PI PICo W
    start_http_server();

    while (true) {
        cyw43_arch_poll(); // mantem o driver de rede em funcionmento 
        handle_led_oscillation(); // processa a oscilação dos LEDS RGB
        sleep_ms(100);
    }

    cyw43_arch_deinit();
    return 0;
}



