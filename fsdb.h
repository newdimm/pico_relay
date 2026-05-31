#ifndef RELAY_FSDB_H
#define RELAY_FSDB_H

enum
{
    Fsdb_Signature = 0x6b1d8c9a,
    Fsdb_Telegram_Offset = 0x12000000
};

#ifdef HAVE_FSDB

void fsdb_init(void);
void fsdb_write(uint32_t key, const uint8_t *data, uint32_t data_size);
uint32_t fsdb_read(uint32_t key, const uint8_t **data_ptr);

#else /* HAVE_FSDB */

#define fsdb_init()
#define fsdb_write(key, data, data_size)
#define fsdb_read(key, data_ptr)

#endif /* HAVE_FSDB */

#endif /* RELAY_FSDB_H */
