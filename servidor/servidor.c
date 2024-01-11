
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
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678
#define UMBRAL_TEMPERATURA 29.0

#define ALERTA_CLI_2 "1"
#define ALERTA_URGENTE_CLI_2 "2"


#define ALERTA_TEMPERATURA_CLI_1 "1"
#define ALERTA_TEMPERATURA_FIN_CLI_1 "2"


static struct simple_udp_connection udp_conn;
static uip_ipaddr_t cli1_ipaddr;
static uip_ipaddr_t cli2_ipaddr;
static bool alerta = false;
static bool alerta_emergencia = false;

// Declaracion de funciones
void separarCadena(char *cadena, char *delimitador, char *partes[], int numPartes);

PROCESS(udp_server_process, "UDP server");
PROCESS(periodic_process, "Evento periodico process");
AUTOSTART_PROCESSES(&udp_server_process, &periodic_process);
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
  } else if (strcmp((char *) data, "CLIENTE-2") == 0) {
    cli2_ipaddr = *sender_addr;
    LOG_INFO("Direccion cliente 2 = ");
    LOG_INFO_6ADDR(&cli2_ipaddr);
    LOG_INFO_("\n");
  } 

#if WITH_SERVER_REPLY

 
  char *datos_rx[2] ={NULL, NULL};

  // Llamar a la función para separar la cadena
  char msg[32];
  snprintf(msg, sizeof(msg), "%s", (char *) data);
  separarCadena(msg, ":", datos_rx, 2);

  // Debug
  LOG_INFO("%.*s \n", strlen(datos_rx[0]), (char *) datos_rx[0]);
  LOG_INFO("%.*s \n", strlen(datos_rx[1]), (char *) datos_rx[1]);

  if (strcmp(datos_rx[0], "EMERGENCIA")==0){
    LOG_INFO("RX: EMERGENCIA\n"); //debug
    // Enviar al cliente 2 la alerta
    alerta_emergencia = true;
    alerta = true;
    simple_udp_sendto(&udp_conn, ALERTA_CLI_2, sizeof(ALERTA_CLI_2), &cli2_ipaddr); //1: ALERTA

  } else if (strcmp(datos_rx[0], "TEMPERATURA")==0){
    LOG_INFO("RX: TEMPERATURA\n"); //debug
    // Comprobar que no supere el umbral. Si lo supera, enviar alerta a los clientes
    if (atof(datos_rx[1])  > UMBRAL_TEMPERATURA && alerta == false) {
      // Enviar al cliente 1 la alerta para que este encienda led rojo
      char * msg = ALERTA_TEMPERATURA_CLI_1;
      simple_udp_sendto(&udp_conn, msg, strlen(msg), &cli1_ipaddr);
      alerta = true;
      LOG_INFO("Alerta temp Destino = ");
      LOG_INFO_6ADDR(sender_addr);
      LOG_INFO_("\n");

      // Enviar al cliente 2 la alerta 
      simple_udp_sendto(&udp_conn, ALERTA_CLI_2, sizeof(ALERTA_CLI_2), &cli2_ipaddr); 

    }
    // Si no supera el umbral y la alerta es true, se envia al cliente para avisar de que ya no hay alerta
    else if (atof(datos_rx[1]) < UMBRAL_TEMPERATURA && alerta == true && alerta_emergencia == false) {
      // Enviar al cliente 1 la alerta para que este encienda led rojo
      char * msg = ALERTA_TEMPERATURA_FIN_CLI_1; 
      simple_udp_sendto(&udp_conn, msg, strlen(msg), &cli1_ipaddr);
      alerta = false;
      LOG_INFO("Alerta fin Destino = ");
      LOG_INFO_6ADDR(sender_addr);
      LOG_INFO_("\n");
  
      // Enviar al cliente 2 fin alerta 
      // simple_udp_sendto(&udp_conn, "ALERTA", sizeof("ALERTA"), &cli2_ipaddr);
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
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  while(1) {

    // Esperamos hasta recibir evento de periodic_process (fin temporizacion 10s)
    PROCESS_WAIT_EVENT();
    LOG_INFO("Enviando ALERTA_URGENTE a enfermero\n");
    char * msg = ALERTA_URGENTE_CLI_2;
    simple_udp_sendto(&udp_conn, msg, strlen(msg), &cli2_ipaddr);

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(periodic_process, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();
  
  while(1) {
    if(alerta==true){
       LOG_INFO("Alerta true --> Temporizando 10s\n");
      // Esperamos a que expire el timer periodico y luego lo reseteamos.
      // Configuramos el timer periodico para que expire en 10 segundos.
      etimer_set(&timer, CLOCK_SECOND * 10);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
      etimer_reset(&timer);
      LOG_INFO("Fin temporizacion 10s\n");
      if(alerta==true){
        // Se genera evento hacia el proceso udp_server_process.
         LOG_INFO("Alerte Sigue true --> Se envia evento fin temporizacion\n");
        process_poll(&udp_server_process);
      }
    } 
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