
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"

#include "pico/flash.h"
#include "hardware/flash.h"

#include "flash.h"
#include "fsdb.h"

#define FLASH_TARGET_OFFSET (1024*1024)
const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

typedef struct 
{
    uint32_t key;
    uint16_t length;
    uint16_t crc;
    uint8_t data[];
} fsdb_entry_t;

#define FSDB_ENTRY_HEADER_SIZE 8

const fsdb_entry_t *fsdb;
const fsdb_entry_t *fsdb_limit;

typedef struct
{
    uint32_t start;
    uint32_t end;
    uint32_t free_space;
} fsdb_info_t;

fsdb_info_t fsdb_info;

#define FSDB_FREE 0xffffffff
#define FSDB_SIZE (FLASH_SECTOR_SIZE * 32)

uint32_t fsdb_read(uint32_t key, const uint8_t **data_ptr)
{
    const fsdb_entry_t *entry = fsdb;
    const fsdb_entry_t *found = NULL;

    while (entry < fsdb_limit &&
            entry->key != FSDB_FREE)
    {
        if (entry->key == key)
        {
            found = entry;
        }
        entry = (const fsdb_entry_t *)(((const uint8_t *)entry) + entry->length);
    }

    if (found)
    {
        *data_ptr = found->data;
        return found->length - FSDB_ENTRY_HEADER_SIZE;
    }

    *data_ptr = NULL;
    return 0;
}

void fsdb_scan(void)
{
    const fsdb_entry_t *entry = fsdb;
    uint32_t free_space = fsdb_info.start;

    printf("fsdb: scan\n");

    while (entry < fsdb_limit &&
            entry->key != FSDB_FREE)
    {
        free_space += entry->length;
        entry = (const fsdb_entry_t *)(((const uint8_t *)entry) + entry->length);
    }

    fsdb_info.free_space = free_space;

    uint32_t percent_used = 
        (fsdb_info.free_space - fsdb_info.start) * 100 / (fsdb_info.end - fsdb_info.start);

    printf("fsdb: [0x%x-0x%x] free 0x%x %u%% used\n",
            fsdb_info.start, fsdb_info.end, fsdb_info.free_space,
            percent_used);
}

void fsdb_write(uint32_t key, const uint8_t *data, uint32_t data_size)
{
    int rc;

    if (fsdb_info.free_space + FSDB_ENTRY_HEADER_SIZE + data_size >= fsdb_info.end)
    {
        printf("fsdb: error writing key 0x%x length %d - no more space\n",
                key, data_size);
        return;
    }

    uint32_t page_start = fsdb_info.free_space & ~(FLASH_PAGE_SIZE-1);
    uint32_t page_offset = fsdb_info.free_space & (FLASH_PAGE_SIZE - 1);
    uint8_t page_data[FLASH_PAGE_SIZE];
    const uint8_t *ptr = (const uint8_t *)(XIP_BASE + page_start);
    memcpy(page_data, ptr, page_offset);

    uint8_t header_data[FSDB_ENTRY_HEADER_SIZE];
    uint32_t header_offset = 0;
    uint32_t header_size = FSDB_ENTRY_HEADER_SIZE;
    fsdb_entry_t *header = (fsdb_entry_t *)header_data;
    header->key = key;
    header->length = data_size + FSDB_ENTRY_HEADER_SIZE;
    header->crc = 0xffff;

    uint32_t data_offset = 0;

    while (header_size || data_size)
    {
        if (header_size)
        {
            uint32_t header_chunk_size  = MIN(FLASH_PAGE_SIZE - page_offset, header_size - header_offset);
            memcpy(&page_data[page_offset], &header_data[header_offset], header_chunk_size);
            page_offset += header_chunk_size;
            header_offset += header_chunk_size;
            header_size -= header_chunk_size;
        }
        else if (data_size)
        {
            uint32_t data_chunk_size  = MIN(FLASH_PAGE_SIZE - page_offset, data_size - data_offset);
            memcpy(&page_data[page_offset], &data[data_offset], data_chunk_size);
            page_offset += data_chunk_size;
            data_offset += data_chunk_size;
            data_size -= data_chunk_size;
        }

        if (page_offset == FLASH_PAGE_SIZE ||
                (header_size == 0 && data_size == 0))
        {
            memset(&page_data[page_offset], 0xff, FLASH_PAGE_SIZE - page_offset);

            flash_program_cmd_t cmd_prog;
            cmd_prog.offset = page_start;
            cmd_prog.data = page_data;
            cmd_prog.size = FLASH_PAGE_SIZE;

            printf("fsdb: program off=0x%x length=%d\n", cmd_prog.offset, cmd_prog.size);

            rc = flash_safe_execute(call_flash_program, (void *)&cmd_prog, UINT32_MAX);
            if (rc != PICO_OK)
            {
                printf("flash: failed to program\n");
                return;
            }

            printf("fsdb: program completed\n");

            page_start += FLASH_PAGE_SIZE;
            page_offset = 0;
        }
    }

    fsdb_scan();
}

void fsdb_prepare(void)
{
    int rc;

    flash_erase_cmd_t cmd_erase;
    cmd_erase.start = fsdb_info.start;
    cmd_erase.end = fsdb_info.end;

    printf("fsdb: erase 0x%x-0x%x\n", cmd_erase.start, cmd_erase.end);

    rc = flash_safe_execute(call_flash_erase, (void *)&cmd_erase, UINT32_MAX);
    if (rc != PICO_OK)
    {
        printf("flash: failed to erase\n");
        return;
    }

    printf("fsdb: erase completed\n");

    uint8_t data[FLASH_PAGE_SIZE];
    memset(data, 0xff, sizeof(data));

    fsdb_entry_t *entry = (fsdb_entry_t *)data;
    entry->key = Fsdb_Signature;
    entry->length = FSDB_ENTRY_HEADER_SIZE;
    entry->crc = 0;

    flash_program_cmd_t cmd_prog;
    cmd_prog.offset = fsdb_info.start;
    cmd_prog.data = data;
    cmd_prog.size = FLASH_PAGE_SIZE;

    printf("fsdb: program off=0x%x length=%d\n", cmd_prog.offset, cmd_prog.size);

    rc = flash_safe_execute(call_flash_program, (void *)&cmd_prog, UINT32_MAX);
    if (rc != PICO_OK)
    {
        printf("flash: failed to program\n");
        return;
    }

    printf("fsdb: program completed\n");

}

void fsdb_init(void)
{
    fsdb_info.start = FLASH_TARGET_OFFSET;
    fsdb_info.end = fsdb_info.start + FSDB_SIZE - 1;

    fsdb = (fsdb_entry_t *)(XIP_BASE + fsdb_info.start);
    fsdb_limit = (const fsdb_entry_t *)(XIP_BASE + fsdb_info.end - FSDB_ENTRY_HEADER_SIZE);

    if (fsdb->key != Fsdb_Signature)
    {
        fsdb_prepare();
    }
    else
    {
        printf("fsdb: data base is ready\n");
    }

    fsdb_scan();
}
