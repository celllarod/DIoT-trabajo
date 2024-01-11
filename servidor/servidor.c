
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


static struct simple_udp_connection udp_conn;
static uip_ipaddr_t cli1_ipaddr;
static uip_ipaddr_t cli2_ipaddr;
static bool alerta = false;

// Declaracion de funciones
void separarCadena(char *cadena, char *delimitador, char *partes[], int numPartes);

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);
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
 
// TODO: Problema aqui, se imprime TEMP en vez de TEMPERATURA. tiene que haber algo mal
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
    // simple_udp_sendto(&udp_conn, "ALERTA", sizeof("ALERTA"), &cli2_ipaddr);

  } else if (strcmp(datos_rx[0], "TEMPERATURA")==0){
    LOG_INFO("RX: TEMPERATURA\n"); //debug
    // Comprobar que no supere el umbral. Si lo supera, enviar alerta a los clientes
    if (atof(datos_rx[1])  > UMBRAL_TEMPERATURA && alerta == false) {
      // Enviar al cliente 1 la alerta para que este encienda led rojo
      char * msg = "1"; //ALERTA_TEMPERATURA
      simple_udp_sendto(&udp_conn, msg, strlen(msg), &cli1_ipaddr);
      alerta = true;
      LOG_INFO("Alerta temp Destino = ");
      LOG_INFO_6ADDR(sender_addr);
      LOG_INFO_("\n");
  
      // Enviar al cliente 2 la alerta 
      // simple_udp_sendto(&udp_conn, "ALERTA", sizeof("ALERTA"), &cli2_ipaddr);
    }
    // Si no supera el umbral y la alerta es true, se envia al cliente para avisar de que ya no hay alerta
    else if (atof(datos_rx[1]) < UMBRAL_TEMPERATURA && alerta == true) {
      // Enviar al cliente 1 la alerta para que este encienda led rojo
      char * msg = "2"; //ALERTA_TEMPERATURA_FIN
      simple_udp_sendto(&udp_conn, msg, strlen(msg), &cli1_ipaddr);
      alerta = false;
      LOG_INFO("Alerta fin Destino = ");
      LOG_INFO_6ADDR(sender_addr);
      LOG_INFO_("\n");
  
      // Enviar al cliente 2 la alerta 
      // simple_udp_sendto(&udp_conn, "ALERTA", sizeof("ALERTA"), &cli2_ipaddr);
    }
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