
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"

#include "pico/flash.h"
#include "hardware/flash.h"

#include "flash.h"

void call_flash_get_unique_id(void *param)
{
    uint8_t *id_out = (uint8_t *)param;

    flash_get_unique_id(id_out);
}

void call_flash_erase(void *param)
{
    flash_erase_cmd_t *cmd = (flash_erase_cmd_t *)param;
    for (uint32_t off = cmd->start;
            off < cmd->end;
            off += FLASH_SECTOR_SIZE)
    {
        flash_range_erase(off, FLASH_SECTOR_SIZE);
    }
}

void call_flash_program(void *param)
{
    flash_program_cmd_t *cmd = (flash_program_cmd_t *)param;

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
            array[0], array[1], array[2], array[3],
            array[4], array[5], array[6], array[7]);
}
