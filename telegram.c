
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "lwip/dns.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/http_client.h"

#include "network.h"

#include "telegram.h"
#include "json.h"
#include "fsdb.h"

#define TELEGRAM_SERVER "api.telegram.org"
#define T_REQ "/bot" TELEGRAM_TOKEN "/"
#define T_UPDATE "https://" TELEGRAM_SERVER T_REQ "getUpdates?offset=%llu&limit=15"

typedef struct {
    bool init_done;
    bool offset_init;
    bool request_pending;
    bool ready;

    char *url;
    int url_len;

    const char *hostname;
    struct altcp_pcb *pcb; // protocol control block
    struct altcp_tls_config * tls_config;
    altcp_allocator_t tls_allocator; 

    httpc_connection_t settings;

    absolute_time_t retry_after;

    unsigned long long offset;

    unsigned long content_len;
    unsigned long content_off;
    uint8_t *content;
} telega_t;

telega_t telega;

bool telegram_read_offset(telega_t *state)
{
    const uint8_t *ptr = NULL;
    uint32_t ret = fsdb_read(Fsdb_Telegram_Offset, &ptr);
   
    if (ret != sizeof(state->offset) || ptr == NULL)
    {
        return false;
    }

    memcpy((uint8_t *)&state->offset, ptr, sizeof(state->offset));
    printf("telega: read offset 0x%llu\n", state->offset);

    return true;
}

void telegram_store_offset(telega_t *state)
{
    printf("telega: store offset %llu\n", state->offset);
    fsdb_write(Fsdb_Telegram_Offset, (uint8_t *)&state->offset, sizeof(state->offset));
}

void telegram_send(const char *message)
{
    if (!telega.ready)
    {
        return;
    }
}

static void complete_request(telega_t *state, int delay_ms)
{
    if (state->url) {
        free(state->url);
        state->url = NULL;
    }

    state->retry_after = get_absolute_time() + delay_ms * 1000;
    state->request_pending = false;
    state->ready = true;

    printf("telega: complete, retry in %dus\n", delay_ms);
}

static err_t internal_recv_fn(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err) 
{
    assert(arg);
    telega_t *state = (telega_t *)arg;

    printf("telega: body err=%d len=%d\n", err, p ? p->tot_len : 0);

    if (err != ERR_OK) {
        return ERR_OK;
    }
    
    if (state->content == NULL)
    {
        printf("telega: body len %d drop the data\n");
        state->content_off += p->tot_len;
        return ERR_OK;
    }

    if (state->content_off + p->tot_len > state->content_len)
    {
        printf("telega: body content off %d + tot_len %d = %d > len %d -> DROP",
                state->content_off, p->tot_len, 
                state->content_off + p->tot_len,
                state->content_len);
        return ERR_OK;
    }

    pbuf_copy_partial(p, &state->content[state->content_off], p->tot_len, 0);

    state->content[state->content_off + p->tot_len + 1] = '\0';
    
    //printf("telega: body data=<%s>\n", &state->content[state->content_off]);

    state->content_off += p->tot_len;

    if (state->content_off == state->content_len)
    {
        printf("telega: json data: %s\n", state->content);

        json_data_t *d = json_parse_data(state->content, state->content_len);

        if (!d) {
            printf("telega: failed to parse data\n");
        }
        else {
            json_data_t *e;
            e = json_find(d, "update_id");
            if (e && e->type == JSON_NUMBER)
            {
                state->offset = (unsigned long long)e->u.number;
                printf("telega: update_id=%llu\n", state->offset);
                state->offset += 1;
                telegram_store_offset(state);
            }

            json_free_data(d);
        }

        free(state->content);
        state->content = NULL;

        state->content_len = 0;
        state->content_off = 0;
    }

    return ERR_OK;
}

static err_t internal_header_fn(httpc_state_t *connection, void *arg, struct pbuf *hdr, u16_t hdr_len, u32_t content_len) 
{
    assert(arg);
    telega_t *state = (telega_t *)arg;

    printf("telega: header received, content length=%d header length=%d\n",
        content_len, hdr_len);

    if (state->content)
    {
        printf("telega: free stale request\n");
        free(state->content);
        state->content = NULL;
    }

    state->content_len = content_len;
    state->content_off = 0;
    state->content = malloc(state->content_len+1);

    if (state->content == NULL)
    {
        printf("telega: malloc(%d) failed, drop request\n", state->content_len+1);
    }

#if 0
    char * data = malloc(hdr->tot_len);

    pbuf_copy_partial(hdr, data, hdr->tot_len, 0);
    printf("telega: header data=<%s>\n", data);
    
    free(data);
#endif

    return ERR_OK;
 }


static void internal_result_fn(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err) 
{
    assert(arg);

    telega_t *state = (telega_t *)arg;
    printf("telega: result %u len %u server_response %u err %d\n", httpc_result, rx_content_len, srv_res, err);

    int delay = 
        (httpc_result == HTTPC_RESULT_OK) ? 30000 : 10000;

    complete_request(state, delay);
}


// Override altcp_tls_alloc to set sni
static struct altcp_pcb *altcp_tls_alloc_sni(void *arg, u8_t ip_type) 
{
    assert(arg);
    telega_t *state = (telega_t *)arg;
    struct altcp_pcb *pcb = altcp_tls_alloc(state->tls_config, ip_type);
    if (!pcb) {
        printf("telega: failed to allocate PCB\n");
        return NULL;
    }
    printf("telega: altcp_tls_alloc_sni hostname=<%s> ip_type=%d\n", state->hostname, ip_type);
    if (mbedtls_ssl_set_hostname(altcp_tls_context(pcb), state->hostname)!=0)
    {
        printf("mbedtls_ssl_set_hostname failed\n");
    }
    return pcb;
}

static void send_request(telega_t *state) 
{
    printf("telega: send request\n");

    state->request_pending = true;
    state->retry_after = 0;

    if (!state->offset_init)
    {
        if (!telegram_read_offset(state))
        {
            printf("telega: no offset found, resetting to 0\n");
            state->offset = 0;
            telegram_store_offset(state);
        }
        state->offset_init = 1;
    }

    // offset is "unsigned long long" - that is 20 decimal digits
    state->url_len = sizeof(T_UPDATE) + 20;
    state->url = malloc(state->url_len);
    snprintf(state->url, state->url_len, T_UPDATE, state->offset);
    
    state->hostname = TELEGRAM_SERVER;

    static const uint8_t cert_data[] = TELEGRAM_CERT;

    if (!state->tls_config) {
        state->tls_config = altcp_tls_create_config_client(cert_data, sizeof(cert_data));

        state->tls_allocator.alloc = altcp_tls_alloc_sni;
        state->tls_allocator.arg = state;

        state->settings.headers_done_fn = internal_header_fn;
        state->settings.result_fn = internal_result_fn;
        state->settings.altcp_allocator = &state->tls_allocator;
    }

    if (state->content)
    {
        printf("telega: free stale request\n");
        free(state->content);
        state->content = NULL;
        state->content_off = 0;
        state->content_len = 0;
    }

    const uint16_t port = 443;

    async_context_t *context = cyw43_arch_async_context();
    
    async_context_acquire_lock_blocking(context);
    
    err_t ret = httpc_get_file_dns(state->hostname, port, state->url, 
            &state->settings, internal_recv_fn, state, NULL);
    
    async_context_release_lock(context);

    if (ret != ERR_OK) {
        printf("telega: http request failed: %d\n", ret);
    }
}

void telegram_periodic(void)
{
    if (!telega.init_done) {
        return;
    }

    if (!rnet.connected) {
        return;
    }

    if (!rnet.ready) {
        return;
    }

    if (!telega.request_pending) {
        if (!telega.retry_after ||
            absolute_time_diff_us(telega.retry_after, get_absolute_time()) > 0)
        {
            send_request(&telega);
        }
    }
}

void telegram_init()
{
    printf("telegra: init\n");
    telega.offset_init = 0;
    telega.init_done = 1;
}

