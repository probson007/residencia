#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

#define WIFI_SSID "IFCE Alunos"
#define WIFI_PASSWORD ""

#define LED_PIN CYW43_WL_GPIO_LED_PIN
#define LED_BLUE_PIN  12
#define LED_GREEN_PIN  11
#define LED_RED_PIN  13

// Função d callback para processar requisições HTTP recebidas
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p){ // verrifica se o pacote recebido é nulo (conexão recebida)
        tcp_close(tpcb); // Fecha a conexão TCP
        tcp_recv(tpcb, NULL); // Desabilita o recebimento de dados
        return ERR_OK;
    }
    
    // Copia a requisição http para uma string alocada dinamicamente 
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';
    printf("Request: %s\n", request); // exibe a requisiçao recebida
    
    // Controle dos leds com base na URL recebida
    if (strstr(request, "GET /blue_on") != NULL) { // liga o LED Azul
        gpio_put(LED_BLUE_PIN, 1);        
    } else if (strstr(request, "GET /blue_off") != NULL) { // desliga o LED Azul
        gpio_put(LED_BLUE_PIN, 0);  
    } else if (strstr(request, "GET /green_on") != NULL) { // liga o LED Vrrrde
        gpio_put(LED_GREEN_PIN, 1);        
    } else if (strstr(request, "GET /green_off") != NULL) { // desliga o LED verde
        gpio_put(LED_GREEN_PIN, 0);  
    } else if (strstr(request, "GET /red_on") != NULL) { // liga o LED Vermelho
        gpio_put(LED_RED_PIN, 1);        
    } else if (strstr(request, "GET /red_off") != NULL) { // desliga o LED Vermelho
        gpio_put(LED_RED_PIN, 0);  
    } else if (strstr(request, "GET /on") != NULL) { // liga o LED EMBUTIDO
        gpio_put(LED_PIN, 1);        
    } else if (strstr(request, "GET /off") != NULL) { // desliga o LED EMBUTIDO
        gpio_put(LED_PIN, 0);  
    }
    
    // Leitura de temperatura interna usando ADC
    adc_select_input(4); // seleciona o canal 4 do ADC para o sensor interno
    uint16_t raw_value = adc_read(); // lê o valor do ADC
    const float conversion_factor = 3.3f / (1 << 12); // Fator de conversão do ADC
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;

    // criação da resposta HTML com os controles e a temperatura interna
    char html[1024];
    sniprintf(html, sizeof(html),
             "HTTP/1.1 200 OK\r\n" "Content-Type: text/html\r\n" "\r\n" "!DOCTYPE html>\n"
             "<html>\n" "<head>\n" "<title>LED Control</title\n>" "<style>\n"
             "body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
             "h1 {font-size: 64px; margin-botton: 30px; }\n"
             "button { font-size: 36px; margin: 10px; padding: 20px 40px; border-radius: 10px; }\n"
             ".temperature { font-size: 48px; margin-top: 30px; color: #333; }\n"
             "</style>\n" "</head>\n" "<body>\n" "<h1>LED Control</h1>\n"
             "<form action=\"./blue_on\"><button>Ligar Azul</button></form>\n"
             "<form action=\"./blue_off\"><button>Desligar Azul</button></form>\n"
             "<form action=\"./green_on\"><button>Ligar Green</button></form>\n"
             "<form action=\"./green_off\"><button>Desligar Green</button></form>\n"
             "<form action=\"./red_on\"><button>Ligar Vermelho</button></form>\n"
             "<form action=\"./red_off\"><button>Desligar Vermelho</button></form>\n"
             "<p class=\"temperature\">Temperatura Interna: %.2f &deg;C</p>\n"
             "</body>\n" "</html>\n", temperature);
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY); // Envia a resposta ao cliente
    tcp_output(tpcb); // força o envio imediato
    free(request); // libera a memória alocada para requisição
    pbuf_free(p); // libera bufer de pacotes
    return ERR_OK;
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;  
}


// Função principal
int main()
{
    stdio_init_all(); // inicializa a entrada/saída padrão

    // Configuração dos pinos dos leds com saída
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);

    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);

    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);

    sleep_ms(10000);

    while (cyw43_arch_init()) { // inicializa o wi-fi
        printf("Falha ao inicializar Wi-fi\n");
        sleep_ms(100);
        return -1;        
    }

    cyw43_arch_gpio_put(LED_PIN, 0);
    // configura o wi-fi no modo estação
    cyw43_arch_enable_sta_mode();
    printf("Conectando o wi-fi...\n");


    int tentativas = 0;
    const int MAX_RETRIES = 5;

    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
       printf("Falha ao conectar ao Wi-Fi (tentativa %d de %d)\n", tentativas + 1, MAX_RETRIES);
       sleep_ms(100);
       tentativas++;
       if (tentativas >= MAX_RETRIES) {
           return -1;
       }
}
    printf("Conectado o wi-fi.\n");
    
    if (netif_default) {  // exibe o IP atribuído ao dispositivo
        printf("IP do dispositivo: %s\n", ip4addr_ntoa(&netif_default->ip_addr));        
    }

    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }
    
}
