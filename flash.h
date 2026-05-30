#ifndef PICO_RELAY_FLASH_H
#define PICO_RELAY_FLASH_H

#ifdef HAVE_FLASH

void flash_init(void);

#else /* HAVE_FLASH */

#define flash_init()


#endif /* HAVE_FLASH */

#endif /* PICO_RELAY_FLASH_H */
