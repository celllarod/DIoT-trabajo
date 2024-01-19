
#include "config.h" // config especifica
#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "lib/sensors.h"
#include "arch/platform/nrf52840/common/temperature-sensor.h"
#include "dev/button-hal.h"
#include "dev/leds.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define SEND_INTERVAL		  (60 * CLOCK_SECOND)

#define PROCESS_TIMER_TEMP 245
#define PROCESS_EVENT_TEMPERATURA 246
#define PROCESS_EVENT_BOTON 247

#define CLIENTE_1 "C1"
#define TEMPERATURA "TP"
#define EMERGENCIA "EM"

static struct simple_udp_connection udp_conn;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
PROCESS(temperature_sensor_process, "Lee el sensor de temperatura periodicamente");
PROCESS(timer_process, "Temporiza 3 segundos");
PROCESS(button_hal_client_process, "Button HAL");

AUTOSTART_PROCESSES(&udp_client_process, &temperature_sensor_process, &timer_process, &button_hal_client_process);
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

  LOG_INFO("Recibido: '%.*s' ", datalen, (char *) data);
  
  if (strcmp((char *) data, "1")==0) {//Alerta temperatura
    LOG_INFO("led_rojo");
    leds_single_off(LEDS_LED1);
    leds_single_on(LEDS_LED2);

  }
  if (strcmp((char *) data, "2")==0) {//Alerta temperatura fin
    LOG_INFO("led_verde");
    leds_single_off(LEDS_LED2);
    leds_single_on(LEDS_LED1);
    
  }


#if LLSEC802154_CONF_ENABLED
  LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#endif
  LOG_INFO_("\n");

}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  uip_ipaddr_t dest_ipaddr;
  static char str[32];
  static bool primera_conexion = true;

  PROCESS_BEGIN();

 // Inicializamos el led con color verde (todo ok)
  leds_single_on(LEDS_LED1);

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, udp_rx_callback);

  while(1) {

      // Esperamos hasta recibir evento 
      PROCESS_WAIT_EVENT_UNTIL((ev == PROCESS_EVENT_TEMPERATURA) || (ev == PROCESS_EVENT_BOTON));
      if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {

        if (primera_conexion == true){
          // Enviar mensaje de que se ha conectado al servidor
          char * msg = CLIENTE_1;
          LOG_INFO("Enviando msg 1ª conexion al servidor\n");
          simple_udp_sendto(&udp_conn, msg, strlen(msg), &dest_ipaddr);
          primera_conexion = false;
        }

      if (ev==PROCESS_EVENT_TEMPERATURA) {
        snprintf(str, sizeof(str), "%s:%s", (char *) TEMPERATURA, (char *) data);
        LOG_INFO("Enviando = %.*s\n", sizeof(str), (char *) str);
        simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);
      } 
      
      if (ev==PROCESS_EVENT_BOTON) {
        LOG_INFO("Enviando solicitud de emergencia\n");
        char *msg = EMERGENCIA;
        simple_udp_sendto(&udp_conn, msg, strlen(msg), &dest_ipaddr);
      }

    } else {
      LOG_INFO("Not reachable yet\n");
      primera_conexion = true;
    }

    /* Add some jitter */
    etimer_set(&periodic_timer, SEND_INTERVAL- CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------
* Proceso que temporiza 3 segundos periodicamente y envia evento 
*---------------------------------------------------------------------------*/
PROCESS_THREAD(timer_process, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();

  // Configuramos el timer periodico para que expire en 3 segundos.
  etimer_set(&timer, CLOCK_SECOND * 3);


  while(1) {

    // Esperamos a que expire el timer periodico y luego lo reseteamos.
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    etimer_reset(&timer);
    process_post(&temperature_sensor_process, PROCESS_TIMER_TEMP, NULL);
  } 

  PROCESS_END();
}

/*---------------------------------------------------------------------------
 * Proceso que lee la temperatura del sensor
 *--------------------------------------------------------------------------*/
PROCESS_THREAD(temperature_sensor_process, ev, data)
{
  static char str[32];

  PROCESS_BEGIN();

  while(1) {
   
     // Esperamos hasta recibir evento de timer_proccess
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_TIMER_TEMP);

    // Activamos el sensor de temperatura
    SENSORS_ACTIVATE(temperature_sensor);

    // Leemos la temperatura del sensor 
    int16_t temp = temperature_sensor.value(0);
    

    // Desactivamos el sensor de temperatura
    SENSORS_DEACTIVATE(temperature_sensor);

    // Imprimimos la temperatura con 2 decimales teniendo en cuenta que es un entero y que la resolucion es de 0.25 haciendo cast a float
    // Dividimos entre 4 porque la resolucion del sensor es de 0.25 (1/4)
    snprintf(str, sizeof(str), "%d.%d", temp/4, temp%4*25);

    LOG_INFO("[temp] Temperatura del paciente = %.*s\n", sizeof(str), (char *) str);

    process_post(&udp_client_process, PROCESS_EVENT_TEMPERATURA, str);
    
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(button_hal_client_process, ev, data)
{
  button_hal_button_t *btn;

  PROCESS_BEGIN();

  btn = button_hal_get_by_index(0);

  while(1) {

    PROCESS_YIELD();

     if(ev == button_hal_periodic_event) {
      btn = (button_hal_button_t *)data;
  
      LOG_INFO("Boton pulsado: posible emergencia\n");

      if(btn->press_duration_seconds > 3) {
        LOG_INFO("Boton pulsado durante 3 segundos: emergencia\n");
        process_post(&udp_client_process, PROCESS_EVENT_BOTON, NULL);
      } 
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
