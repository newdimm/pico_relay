
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"

#include "pico/flash.h"
#include "hardware/flash.h"

#include "flash.h"

#define FLASH_TARGET_OFFSET (1024*1024)
const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);


static void flash_dump(void)
{
    printf("flash_data: <");

    for (int i=0; i < 16; i++)
    {
        printf("%c", flash_target_contents[i]);
    }

    printf(">\n");
}


static void call_flash_get_unique_id(void *param)
{
    uint8_t *id_out = (uint8_t *)param;

    flash_get_unique_id(id_out);
} 

static void call_flash_erase(void *param)
{
    uint32_t flash_offset = (uint32_t)param;
    flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
}

typedef struct
{
    uint32_t offset;
    const uint8_t *data;
    size_t size;
} flash_cmd_t;


static void call_flash_program(void *param)
{
    flash_cmd_t *cmd = (flash_cmd_t *)param;

    flash_range_program(cmd->offset, cmd->data, cmd->size);
}

void flash_init(void)
{
    printf("flash: init\n");

    uint8_t array[8] = {0};
    int rc = flash_safe_execute(call_flash_get_unique_id, (void*)array, UINT32_MAX);
    
    if (rc != PICO_OK)
    {
        printf("flash: failed to read ID\n");
        return;
    }

    printf("flash: ID=={%02x %02x %02x %02x %02x %02x %02x %02x}\n",
            array[0],
            array[1],
            array[2],
            array[3],
            array[4],
            array[5],
            array[6],
            array[7]);


    flash_dump();

    printf("flash: erase off=0x%08x\n", FLASH_TARGET_OFFSET);
    rc = flash_safe_execute(call_flash_erase, (void*)FLASH_TARGET_OFFSET, UINT32_MAX);
    if (rc != PICO_OK)
    {
        printf("flash: failed to erase\n");
        return;
    }


    flash_dump();

    uint8_t *data = malloc(FLASH_PAGE_SIZE);
    memset(data, 0xff, FLASH_PAGE_SIZE);
    snprintf(data, FLASH_PAGE_SIZE, "Hello World!");

    flash_cmd_t cmd;
    cmd.offset = FLASH_TARGET_OFFSET;
    cmd.data = data;
    cmd.size = FLASH_PAGE_SIZE;

    printf("flash: program off=0x%08x\n", FLASH_TARGET_OFFSET);
    rc = flash_safe_execute(call_flash_program, (void*)&cmd, UINT32_MAX);
    if (rc != PICO_OK)
    {
        printf("flash: failed to program\n");
        return;
    }
    free(data);

    flash_dump();



}
