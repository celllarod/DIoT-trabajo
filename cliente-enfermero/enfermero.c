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
#define UDP_CLIENT_PORT	8766
#define UDP_SERVER_PORT	5678

#define SEND_INTERVAL		  (10 * CLOCK_SECOND)

#define PROCESS_EVENT_BOTON 247

static struct simple_udp_connection udp_conn;
static bool alerta = false;
static bool parpadeo=false;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
PROCESS(button_hal_client_process, "Button HAL");
PROCESS(parpadeo_process, "parpadeo");
PROCESS(control_parpadeo_process, "control parpadeo");

AUTOSTART_PROCESSES(&udp_client_process, &button_hal_client_process,&parpadeo_process,&control_parpadeo_process);
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
  
  if (strcmp((char *) data, "1")==0) { // 1: ALERTA
    LOG_INFO("led_rojo");
    leds_single_off(LEDS_LED1);
    leds_single_on(LEDS_LED2);
    alerta = true;
  }
  if (strcmp((char *) data, "2")==0) {//Alerta urgente
    LOG_INFO("rojo_parpadeo");
    parpadeo=true;
    
  }

  if (strcmp((char *) data, "3")==0) {//Alerta temperatura fin
    
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
//   static char str[32];
  static bool primera_conexion = true;

  PROCESS_BEGIN();

 // Inicializamos el led con color verde (todo ok)
  leds_single_on(LEDS_LED1);

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, udp_rx_callback);
  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
  while(1) {
      LOG_INFO("ante de conectar\n");
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));  
      if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
        LOG_INFO("conectado\n");
        if (primera_conexion == true){
          // Enviar mensaje de que se ha conectado al servidor
          char * msg = "CLIENTE-2";
          LOG_INFO("Enviando msg 1ª conexion al servidor\n");
          simple_udp_sendto(&udp_conn, msg, strlen(msg), &dest_ipaddr);
          primera_conexion = false;
        }
      
      if (alerta == true){
        // Esperamos hasta recibir evento 
        PROCESS_WAIT_EVENT_UNTIL((ev == PROCESS_EVENT_BOTON));
          if (ev==PROCESS_EVENT_BOTON) {
            LOG_INFO("Enviando respuesta de asistencia\n");
            char *msg = "ASISTENCIA";
            simple_udp_sendto(&udp_conn, msg, strlen(msg), &dest_ipaddr);
            LOG_INFO("direccion servidor");
            LOG_INFO_6ADDR(&dest_ipaddr);
            LOG_INFO("\n");
          }

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

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(button_hal_client_process, ev, data)
{
  PROCESS_BEGIN();

  while(1) {

    PROCESS_YIELD();

      if(ev == button_hal_press_event) {
        LOG_INFO("Boton pulsado: asistencia en camino\n");
        process_post(&udp_client_process, PROCESS_EVENT_BOTON, NULL);

        // Fin de alerta
        alerta = false;
        parpadeo = false;
        LOG_INFO("led_verde");
        leds_single_off(LEDS_LED2);
        leds_single_on(LEDS_LED1);

    }
  }
  PROCESS_END();
}
// PROCESS_THREAD(control_parpadeo_process,ev,data)
// {
//   static struct etimer control_timer;

//   PROCESS_BEGIN();

//   // Timer para que el proceso se despierte cada 1 segundos
//   etimer_set(&control_timer, CLOCK_SECOND * 2);

//   while(1) {
//     //LOG_INFO("ALERTA_PROCCESS\n");

//     PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&control_timer)); 

//     if (parpadeo == true) {
//       LOG_INFO("Alerta true --> Temporizar\n");
//       process_poll(&parpadeo_process);
//       // Esperamos hasta recibir evento de periodic_process (fin temporizacion 10s)
      
//     }
    
//     etimer_reset(&control_timer);
//   }
//   PROCESS_END();
// }

PROCESS_THREAD(parpadeo_process, ev, data)
{
 static struct etimer pap_timer;
 

  PROCESS_BEGIN();

  /* Setup a periodic timer that expires after 2 seconds. */
  etimer_set(&pap_timer, CLOCK_SECOND * 0.5);
   
  while(1) {
      LOG_INFO("esperando process poll\n");
      // PROCESS_WAIT_EVENT_UNTIL(ev==PROCESS_EVENT_POLL);

      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&pap_timer));

      while(parpadeo == true) {
      leds_single_toggle(LEDS_LED2) ; 
       etimer_set(&pap_timer, CLOCK_SECOND * 0.5);
       
        // LOG_INFO("parapadeo off\n");
        // leds_single_off(LEDS_LED2);
        // etimer_reset(&pap_timer);
        // PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&pap_timer));
        // leds_single_on(LEDS_LED2);
        // LOG_INFO("parpadeo on\n");
        // etimer_reset(&pap_timer);
      }
    }
   

  PROCESS_END();
}

// 
