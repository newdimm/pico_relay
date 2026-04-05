#ifndef RELAY_MQTT_H
#define RELAY_MQTT_H

#ifdef HAVE_MQTT

extern void mqtt_init(void);
extern void mqtt_periodic(void);
extern void mqtt_send(const char *topic, const char *message);
 
#else /* HAVE_MQTT */

#define mqtt_init()
#define mqtt_periodic()
#define mqtt_send(topic, message)

#endif /* HAVE_MQTT */

#endif /* RELAY_MQTT_H */
