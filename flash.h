#ifndef PICO_RELAY_FLASH_H
#define PICO_RELAY_FLASH_H

typedef struct
{
    uint32_t start;
    uint32_t end;
} flash_erase_cmd_t;

typedef struct
{
    uint32_t offset;
    const uint8_t *data;
    size_t size;
} flash_program_cmd_t;

#ifdef HAVE_FLASH

void flash_init(void);
void call_flash_get_unique_id(void *param);
void call_flash_erase(void *param);
void call_flash_program(void *param);

#else /* HAVE_FLASH */

#define flash_init()
#define call_flash_get_unique_id(param)
#define call_flash_erase(param)
#define call_flash_program(param)

#endif /* HAVE_FLASH */

#endif /* PICO_RELAY_FLASH_H */
