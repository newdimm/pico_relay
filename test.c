#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "json.h"

char *sample = " {\"ok\":true,\
  \"result\":[{\"update_id\":663886605, \
             \"message\":{\"message_id\":4,\
                         \"from\":{\"id\":430218609,\"is_bot\":false,\"first_name\":\"Dima (dimm)\",\"last_name\":\"Vasilchenko\",\"username\":\"newdimm\",\"language_code\":\"en\"},\
                         \"chat\":{\"id\":430218609,\"first_name\":\"Dima (dimm)\",\"last_name\":\"Vasilchenko\",\"username\":\"newdimm\",\"type\":\"private\"},\
                         \"date\":1775241627,\
                         \"text\":\"/start\",\
                         \"entities\":[{\"offset\":0,\"length\":6,\"type\":\"bot_command\"}]\
                        }\
             }\
            ]\
  }";

int main(int argc, char *argv[])
{
    size_t size = strlen(sample) + 1;
    char *s = malloc(size);
    strncpy(s, sample, size);
    s[size-1] = '\0';
    json_data_t *d = json_parse_data(s, size);

    json_print(d);

    json_data_t *chat = json_find(d, "chat");
    json_print(chat);

    json_data_t *id = json_find(d, "id");
    json_print(id);

    if (d) {
        json_free_data(d);
    }
}

