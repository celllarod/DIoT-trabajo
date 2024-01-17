
#include "config.h" // config especifica
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <stdlib.h> // Para usar atoi

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_1_PORT	8765
#define UDP_CLIENT_2_PORT	8766
#define UDP_SERVER_PORT	5678

#define UMBRAL_TEMPERATURA 33.0

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

PROCESS(udp_server_process, "UDP server");
PROCESS(alerta_proccess, "Proceso que envia alerta al enfermero");
PROCESS(periodic_process, "Evento periodico process");
// AUTOSTART_PROCESSES(&udp_server_process, &periodic_process);
AUTOSTART_PROCESSES(&udp_server_process, &alerta_proccess, &periodic_process);

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
  LOG_INFO("Info recibida del nodo: %.*s \n", datalen, (char *) data);
  LOG_INFO("Direccion = ");
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");
  LOG_INFO("Recibido = %.*s \n", datalen, (char *) data);

  if (strcmp((char *) data, "CLIENTE-1") == 0) {
    cli1_ipaddr = *sender_addr;
    LOG_INFO("Direccion cliente 1 = ");
    LOG_INFO_6ADDR(&cli1_ipaddr);
    LOG_INFO_("\n");
  } 
  if (strcmp((char *) data, "CLIENTE-2") == 0) {
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

 
  char *datos_rx[2] ={NULL, NULL};

  // Llamar a la función para separar la cadena
  char msg[32];
  snprintf(msg, sizeof(msg), "%s", (char *) data);
  separarCadena(msg, ":", datos_rx, 2);

  // Debug
  //LOG_INFO("%.*s \n", strlen(datos_rx[0]), (char *) datos_rx[0]);
  //LOG_INFO("%.*s \n", strlen(datos_rx[1]), (char *) datos_rx[1]);

  if (strcmp(datos_rx[0], "EMERGENCIA")==0){
    LOG_INFO("RX: EMERGENCIA\n"); //debug
    // Enviar al cliente 2 la alerta
    alerta_emergencia = true;
    alerta = true;
    simple_udp_sendto(&udp_conn[1], ALERTA_CLI_2, sizeof(ALERTA_CLI_2), &cli2_ipaddr); //1: ALERTA
    LOG_INFO("TX-CLI2: ALERTA\n"); //debug
    LOG_INFO_6ADDR(&cli2_ipaddr);
    LOG_INFO_("\n");

  } else if (strcmp(datos_rx[0], "TEMPERATURA")==0){
    LOG_INFO("RX: TEMPERATURA\n"); //debug

    // Comprobar que no supere el umbral. Si lo supera, enviar alerta a los clientes
    if (atof(datos_rx[1])  > UMBRAL_TEMPERATURA && alerta == false) {
      // Enviar al cliente 1 la alerta para que este encienda led rojo
      char * msg = ALERTA_TEMPERATURA_CLI_1;
      simple_udp_sendto(&udp_conn[0], msg, strlen(msg), &cli1_ipaddr);
      LOG_INFO("TX-CLI1: ALERTA_TEMPERATURA\n"); //debug
      alerta = true;

      // Enviar al cliente 2 la alerta 
      simple_udp_sendto(&udp_conn[1], ALERTA_CLI_2, sizeof(ALERTA_CLI_2), &cli2_ipaddr); 
      LOG_INFO("TX-CLI2: ALERTA -- "); //debug
      LOG_INFO_6ADDR(&cli2_ipaddr);
      LOG_INFO_("\n");
    }
    // Si no supera el umbral y la alerta es true, se envia al cliente para avisar de que ya no hay alerta
    else if (atof(datos_rx[1]) < UMBRAL_TEMPERATURA && alerta == true && alerta_emergencia == false) {
      // Enviar al cliente 1 la alerta para que este encienda led verde
      char * msg = ALERTA_TEMPERATURA_FIN_CLI_1; 
      simple_udp_sendto(&udp_conn[0], msg, strlen(msg), &cli1_ipaddr);
      alerta = false;
  
      // Enviar al cliente 2 fin alerta 
      simple_udp_sendto(&udp_conn[1], ALERTA_FIN_CLI_2, sizeof(ALERTA_FIN_CLI_2), &cli2_ipaddr);
    }
  } else if (strcmp(datos_rx[0], "ASISTENCIA")==0) {
    LOG_INFO("RX: ASISTENCIA\n"); //debug
    alerta_emergencia = false;
  }

#endif /* WITH_SERVER_REPLY */
}
/*---------------------------------------------------------------------------*/
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
  

  // while(1) {

  //   // Esperamos hasta recibir evento de periodic_process (fin temporizacion 10s)
  //   PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
  //   LOG_INFO("Enviando ALERTA_URGENTE a enfermero\n");
  //   char * msg = ALERTA_URGENTE_CLI_2;
  //   simple_udp_sendto(&udp_conn[1], msg, strlen(msg), &cli2_ipaddr);

  // }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
// PROCESS_THREAD(alerta_proccess, ev, data)
// {
//   PROCESS_BEGIN();

//   LOG_INFO("COMIENZA ALERTA_PROCCESS\n");  

//   while(1) {
//     LOG_INFO("ALERTA_PROCCESS\n");

//     // Esperamos hasta recibir evento de periodic_process (fin temporizacion 10s)
//     PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
//     LOG_INFO("Enviando ALERTA_URGENTE a enfermero\n");
//     char * msg = ALERTA_URGENTE_CLI_2;
//     simple_udp_sendto(&udp_conn[1], msg, strlen(msg), &cli2_ipaddr);

//   }

//   PROCESS_END();
// }
/*---------------------------------------------------------------------------*/
// Proceso  que comprueba continuamente si hay alerta y si la hay, le envia un 
// evento al proceso periodic_process para que este temporice 10s. Si finaliza 
// la temporizacion y la alerta sigue activa, se envia la alerta urgente al enfermero
PROCESS_THREAD(alerta_proccess, ev, data)
{
  static struct etimer periodic_timer;

  PROCESS_BEGIN();

  // Timer para que el proceso se despierte cada 1 segundos
  etimer_set(&periodic_timer, CLOCK_SECOND * 1);

  while(1) {
    //LOG_INFO("ALERTA_PROCCESS\n");

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer)); 

    if (alerta == true) {
      LOG_INFO("Alerta true --> Temporizar\n");
      process_poll(&periodic_process);

      // Esperamos hasta recibir evento de periodic_process (fin temporizacion 10s)
      PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);

      // Si la alerta sigue activa al finalizar la temporizacion, se envia la alerta urgente al enfermero
      if(alerta==true){
        LOG_INFO("Alerta sigue true --> TX-CLI2-ALERTA_URGENTE\n");
        char * msg = ALERTA_URGENTE_CLI_2;
        simple_udp_sendto(&udp_conn[1], msg, strlen(msg), &cli2_ipaddr);
      }
    }

    etimer_reset(&periodic_timer);
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(periodic_process, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();
  
  
  LOG_INFO("COMIENZA PROCESO TEMPORIZADOR\n");

  while(1) {

    // Esperamos hasta recibir evento de alerta_proccess (alerta activa) para comenzar la temporizacion
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
    LOG_INFO("------> Temporizando 10s\n");
    // Configuramos el timer periodico para que expire en 1 segundos.
    etimer_set(&timer, CLOCK_SECOND * 10);

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    etimer_reset(&timer);
    LOG_INFO("Fin temporizacion 10s\n");
    // Se envia un evento al proceso alerta_proccess indicando que ha finalizado la temporizacion
    process_poll(&alerta_proccess);
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

void separarCadena(char *cadena, char *delimitador, char *partes[], int numPartes) {
    char *token = strtok(cadena, delimitador);
    
    int i = 0;
    while (token != NULL && i < numPartes) {
        partes[i] = token;
        LOG_INFO("Parte %d = %.*s \n", i, strlen(partes[i]), (char *) partes[i]);
        i++;
        token = strtok(NULL, delimitador);
        
    }
}


// TODO: Funciona todo menos la temporizacion. 
//La idea es que uando alerta sea true se envie un evento desde alerta_proccess a periodic_process para que este temporice 10s y luego envie la alerta al enfermero.
//EL problema es que cuando alerta es false, se queda en el proceso alerta eternamente. Tenemos que buscar una forma de que se salga.

// Otro problema es que cuando enfermero envia "ASISTENCIA", el servidor no recibe el mensaje.