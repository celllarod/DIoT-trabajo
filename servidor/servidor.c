
#include "config.h" // config especifica
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <stdlib.h> // Para usar atoi
#include "dev/leds.h"
#include "lib/sensors.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY 1
#define UDP_CLIENT_1_PORT 8765
#define UDP_CLIENT_2_PORT 8766
#define UDP_SERVER_PORT 5678

#define UMBRAL_TEMPERATURA 37.0

#define CLIENTE_1 "C1"
#define CLIENTE_2 "C2"
#define EMERGENCIA "EM"
#define ASISTENCIA "AS"
#define TEMPERATURA "TP"

#define ALERTA_CLI_2 "1"
#define ALERTA_URGENTE_CLI_2 "2"
#define ALERTA_FIN_CLI_2 "3"

#define ALERTA_TEMPERATURA_CLI_1 "1"
#define ALERTA_TEMPERATURA_FIN_CLI_1 "2"

#define NUM_CLIENTES 2

static struct simple_udp_connection udp_conn[NUM_CLIENTES];
static uip_ipaddr_t cli1_ipaddr;
static uip_ipaddr_t cli2_ipaddr;
static uip_ipaddr_t receiver_ipaddr;
static bool alerta = false;
static bool alerta_emergencia = false;

// Declaracion de funciones
void separarCadena(char *cadena, char *delimitador, char *partes[], int numPartes);

void mqtt_temp_export(char *data_celsius_str);

PROCESS(udp_server_process, "UDP server");
PROCESS(alerta_proccess, "Proceso que envia alerta al enfermero");
PROCESS(periodic_process, "Evento periodico process");
PROCESS(parpadeo_process, "Parpadeo LED process");
// AUTOSTART_PROCESSES(&udp_server_process, &periodic_process);
AUTOSTART_PROCESSES(&udp_server_process, &alerta_proccess, &periodic_process, &parpadeo_process);

/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr,
                uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr,
                uint16_t receiver_port,
                const uint8_t *data,
                uint16_t datalen)
{

  // Convertir el resultado a una cadena
  LOG_INFO("Info recibida del nodo: %.*s \n", datalen, (char *)data);
  LOG_INFO("Direccion = ");
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");
  LOG_INFO("Recibido = %.*s \n", datalen, (char *)data);

  // El primer mensaje con cada cliente siempre sera CLIENTE_N. Cuando se reciba
  // este mensaje, se guardara la direccion del cliente en una variable global para
  // poder enviarle mensajes posteriormente.
  if (strcmp((char *)data, CLIENTE_1) == 0)
  {
    cli1_ipaddr = *sender_addr;
    LOG_INFO("Direccion cliente 1 = ");
    LOG_INFO_6ADDR(&cli1_ipaddr);
    LOG_INFO_("\n");
  }
  if (strcmp((char *)data, CLIENTE_2) == 0)
  {
    cli2_ipaddr = *sender_addr;
    LOG_INFO("Direccion cliente 2 = ");
    LOG_INFO_6ADDR(&cli2_ipaddr);
    LOG_INFO_("\n");
    receiver_ipaddr = *receiver_addr;
    LOG_INFO("Direccion servidor: ");
    LOG_INFO_6ADDR(&receiver_ipaddr);
    LOG_INFO_("\n");
  }

#if WITH_SERVER_REPLY

  // Los datos siempre se reciben en formato: "etiqueta:valor".
  // Se separa el mensaje recibido en dos partes: etiqueta y valor.
  // Segun la etiqueta, se realiza una accion u otra.

  char *datos_rx[2] = {NULL, NULL};
  char msg[32];
  snprintf(msg, sizeof(msg), "%s", (char *)data);
  separarCadena(msg, ":", datos_rx, 2);

  // EMERGENCIA significa que el paciente ha pulsado el boton de emergencia.
  // Se envia al cliente 2 (enfermero) la alerta de emergencia en cliente 1 (paciente).
  if (strcmp(datos_rx[0], EMERGENCIA) == 0)
  {
    LOG_INFO("RX: EMERGENCIA\n");
    alerta_emergencia = true;
    printf("mqtt-alerta_em:1\n");

    simple_udp_sendto(&udp_conn[1], ALERTA_CLI_2, sizeof(ALERTA_CLI_2), &cli2_ipaddr);
    LOG_INFO("TX-CLI2: ALERTA\n");
  }

  // TEMPERATURA significa que el cliente 1 (paciente) ha enviado su temperatura.
  else if (strcmp(datos_rx[0], TEMPERATURA) == 0)
  {
    LOG_INFO("RX: TEMPERATURA\n");

    mqtt_temp_export(datos_rx[1]);

    // Comprobar umbral de temperatura.
    // Si supera el umbral, enviar alerta ambos clientes
    if (atof(datos_rx[1]) > UMBRAL_TEMPERATURA && alerta == false)
    {
      alerta = true;
      printf("mqtt-alerta_temp:1\n");

      // Enviar al cliente 1 la alerta
      char *msg = ALERTA_TEMPERATURA_CLI_1;
      simple_udp_sendto(&udp_conn[0], msg, strlen(msg), &cli1_ipaddr);
      LOG_INFO("TX-CLI1: ALERTA_TEMPERATURA\n"); // debug

      // Enviar al cliente 2 la alerta
      simple_udp_sendto(&udp_conn[1], ALERTA_CLI_2, sizeof(ALERTA_CLI_2), &cli2_ipaddr);
      LOG_INFO("TX-CLI2: ALERTA");
    }

    // Si no supera el umbral, la alerta de temperatura estaba activa y la alerta de emergencia no esta activa,
    // significa que la temperatura ha vuelto a la normalidad y se envia a los clientes fin de la alerta.
    else if (atof(datos_rx[1]) < UMBRAL_TEMPERATURA && alerta == true && alerta_emergencia == false)
    {
      alerta = false;
      printf("mqtt-alerta_temp:2\n");

      // Enviar al cliente 1 la alerta para que este encienda led verde
      char *msg = ALERTA_TEMPERATURA_FIN_CLI_1;
      simple_udp_sendto(&udp_conn[0], msg, strlen(msg), &cli1_ipaddr);

      // Enviar al cliente 2 fin alerta temperatura
      simple_udp_sendto(&udp_conn[1], ALERTA_FIN_CLI_2, sizeof(ALERTA_FIN_CLI_2), &cli2_ipaddr);
    }
  }

  // ASISTENCIA significa que el enfermero ha pulsado el boton de asistencia en camino.
  // Se desactivan alerta de emergencia y alerta de temperatura.
  else if (strcmp(datos_rx[0], ASISTENCIA) == 0)
  {
    LOG_INFO("RX: ASISTENCIA\n"); // debug
    alerta_emergencia = false;
    alerta = false;
    printf("mqtt-alerta_em:2\n");
    printf("mqtt-alerta_temp:2\n");
  }

#endif /* WITH_SERVER_REPLY */
}
/*---------------------------------------------------------------------------
 * Proceso que inicializa el servidor UDP e inicializa las conexiones UDP con
 * los dos clientes existentes: cliente 1 (paciente) y cliente 2 (enfermero).
 * --------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  PROCESS_BEGIN();

  /* Initialize DAG root */
  NETSTACK_ROUTING.root_start();

  /* Initialize UDP connection */
  // Conexion con cliente 1
  simple_udp_register(&udp_conn[0], UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_1_PORT, udp_rx_callback);
  // Conexion con cliente 2
  simple_udp_register(&udp_conn[1], UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_2_PORT, udp_rx_callback);

  PROCESS_END();
}
/*---------------------------------------------------------------------------
 * Proceso  que comprueba continuamente si hay alerta y si la hay, le envia un
 * evento a periodic_process para que este temporice 10s. Si finaliza la temporizacion
 * y la alerta sigue activa, se envia aviso de alerta urgente al enfermero.
 * --------------------------------------------------------------------------*/
PROCESS_THREAD(alerta_proccess, ev, data)
{
  static struct etimer periodic_timer;

  PROCESS_BEGIN();

  // Timer para que el proceso se despierte cada 2 segundos
  etimer_set(&periodic_timer, CLOCK_SECOND * 2);

  while (1)
  {

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if (alerta == true || alerta_emergencia == true)
    {
      LOG_INFO("Alerta activa --> Temporizar\n");
      process_poll(&periodic_process);

      // Esperamos hasta recibir evento de periodic_process (fin temporizacion 10s)
      PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);

      // Si la alerta sigue activa al finalizar la temporizacion, se envia la alerta urgente al enfermero
      if (alerta == true || alerta_emergencia == true)
      {
        LOG_INFO("Alerta sigue activa --> TX-CLI2-ALERTA_URGENTE\n");
        char *msg = ALERTA_URGENTE_CLI_2;
        simple_udp_sendto(&udp_conn[1], msg, strlen(msg), &cli2_ipaddr);
      } 
    }

    etimer_reset(&periodic_timer);
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------
 * Proceso que temporiza 10 segundos y envia evento a alerta_proccess al
 * finalizar.
 * --------------------------------------------------------------------------*/

PROCESS_THREAD(periodic_process, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();

  LOG_INFO("COMIENZA PROCESO TEMPORIZADOR\n");

  while (1)
  {

    // Esperamos hasta recibir evento de alerta_proccess (alerta activa) para comenzar la temporizacion
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
    LOG_INFO("------> Temporizando 10s\n");

    // Configuramos el timer para que expire en 10 segundos.
    etimer_set(&timer, CLOCK_SECOND * 10);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    etimer_reset(&timer);
    LOG_INFO("Fin temporizacion 10s\n");
    // Se envia un evento al proceso alerta_proccess indicando que ha finalizado la temporizacion
    process_poll(&alerta_proccess);
  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------
 * Proceso que hace parpadear los LEDS del servidor
 * --------------------------------------------------------------------------*/

PROCESS_THREAD(parpadeo_process, ev, data)
{
  static struct etimer timer_2;
  
  PROCESS_BEGIN();


  etimer_set(&timer_2, CLOCK_SECOND * 2);


  while(1) {

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer_2));
    leds_single_on(LEDS_LED1); 
    etimer_reset(&timer_2); 
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer_2)); 
    leds_single_off(LEDS_LED1);
    etimer_reset(&timer_2);
   
    }
    PROCESS_END();
}
/*---------------------------------------------------------------------------
 * Funcion que separa una cadena en partes, utilizando un delimitador; y guarda
 * cada parte en un array.
 * --------------------------------------------------------------------------*/
void separarCadena(char *cadena, char *delimitador, char *partes[], int numPartes)
{
  char *token = strtok(cadena, delimitador);

  int i = 0;
  while (token != NULL && i < numPartes)
  {
    partes[i] = token;
    // LOG_INFO("Parte %d = %.*s \n", i, strlen(partes[i]), (char *)partes[i]);
    i++;
    token = strtok(NULL, delimitador);
  }
}

/*---------------------------------------------------------------------------
 * Funcion que transforma la temperatura de grados Celsius a Fahrenheit para
 * exportarla mediante MQTT.
 * --------------------------------------------------------------------------*/
void mqtt_temp_export(char *data_celsius_str)
{
  // Convertimos la cadena a int
  float data_celsius = atof((char *)data_celsius_str);

  // Convertimos grados Celsius a Fahrenheit
  float data_fahrenheit = data_celsius * 2 + 32;

  // Convertir el resultado a una cadena
  char data_fahrenheit_str[32];
  snprintf(data_fahrenheit_str, sizeof(data_fahrenheit_str), "%d.%02d",
           (int)data_fahrenheit,
           (int)((data_fahrenheit - (int)data_fahrenheit) * 100));

  printf("mqtt-temp_c:%.*s;temp_f:%.*s;umbral:%d.%02d\n",
         strlen(data_celsius_str), (char *)data_celsius_str,
         strlen(data_fahrenheit_str), (char *)data_fahrenheit_str,
         (int)UMBRAL_TEMPERATURA,
         (int)((UMBRAL_TEMPERATURA - (int)UMBRAL_TEMPERATURA) * 100));
}

/*---------------------------------------------------------------------------*/
