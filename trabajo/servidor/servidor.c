
#include "config.h" // config especifica
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <stdlib.h> // Para usar atoi

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  0
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678


static struct simple_udp_connection udp_conn;

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
  LOG_INFO("\n");
  LOG_INFO("Valor recibido = %.*s \n", datalen, (char *) data);

#if WITH_SERVER_REPLY
  LOG_INFO("Enviando temperatura en Fahrenheit = %.*s\n", sizeof(data_fahrenheit_str), (char *) data_fahrenheit_str);
  LOG_INFO("Destino = ");
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO("\n");
  
  // Enviamos respuesta al cliente: temperatura convertida de Celcius a Fahrenheit
  simple_udp_sendto(&udp_conn, data_fahrenheit_str, sizeof(data_fahrenheit_str), sender_addr);
 
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
