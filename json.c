#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "json.h"

/*
 * {"ok":true,
 *  "result":[{"update_id":663886605,
 *             "message":{"message_id":4,
 *                        "from":{"id":430218609,"is_bot":false,"first_name":"Dima (dimm)","last_name":"Vasilchenko","username":"newdimm","language_code":"en"},
 *                        "chat":{"id":430218609,"first_name":"Dima (dimm)","last_name":"Vasilchenko","username":"newdimm","type":"private"},
 *                        "date":1775241627,
 *                        "text":"/start",
 *                        "entities":[{"offset":0,"length":6,"type":"bot_command"}]
 *                       }
 *            }
 *           ]
 * }
*/

//#define dprintf printf
#define dprintf(...)

json_data_t *json_parse_data(char *data_str, uint size)
{
    if (size == 0)
    {
        return NULL;
    }

    bool result = true;

    char *s = data_str;

    bool skip = false;
    bool in_string = false;
    bool expect_name = false;
    bool expect_value = false;

    uint pos = 0;

    json_data_t *p = malloc(sizeof(*p));
    memset(p, 0, sizeof(*p));
    p->name = "root";
    expect_value = true;

    dprintf("<%s>\n", s);

    while (result && *s != '\0')
    {
        dprintf("[%d] == <%c>\n", pos, *s);
        if (skip) {
            skip = false;
        }

        else if (in_string) {
            if (*s == '"') {
                // end of a string
                *s = '\0';
                in_string = false;
            }
            else if (*s == '\\') {
                skip = true;
            }
        }

        else if (*s <= ' ') {
            // skip white space
        }

        else if (expect_name)
        {
            if (*s == '"')
            {
                dprintf("[%d] read name\n", pos);

                if (!p->name)
                {
                    in_string = true;
                    p->name = s+1;
                }
                else {
                    dprintf("[%d] error 2nd name\n", pos);
                    result = false;
                    break;
                }
            }
            else if (*s == ':') {
                expect_value = true;
                expect_name = false;
            }
            else
            {
                dprintf("[%d] error name\n", pos);
                result = false;
                break;
            }
        }

        else if (expect_value) {
            switch (*s) {
                case 't':
                    dprintf("[%d] read value <true>\n", pos);
                    if (size - pos >= 4 &&
                            strncmp(s, "true", 4) == 0) {
                        p->type = JSON_BOOL;
                        p->u.b = true;
                        s += 4;
                        pos += 4;
                        continue;
                    }
                    dprintf("[%d] error not <true>\n", pos);
                    result = false;
                    break;
                case 'f':
                    dprintf("[%d] read value <false>\n", pos);
                    if (size - pos >= 5 &&
                            strncmp(s, "false", 5) == 0) {
                        p->type = JSON_BOOL;
                        p->u.b = false;
                        s += 5;
                        pos += 5;
                        continue;
                    }
                    dprintf("[%d] error not <false>\n", pos);
                    result = false;
                    break;
                case '"':
                    dprintf("[%d] read value string\n", pos);
                    p->type = JSON_STRING;
                    p->u.string = s+1;
                    in_string = true;
                    break;
                case 'n':
                    dprintf("[%d] read value <null>\n", pos);
                    if (size - pos >= 4 &&
                            strncmp(s, "null", 4) == 0) {
                        p->type = JSON_NULL;
                        s += 4;
                        pos += 4;
                        continue;
                    }
                    dprintf("[%d] error value not <null>\n", pos);
                    break;

                case '{':
                {
                    dprintf("[%d] read value object\n", pos);

                    json_data_t *np = malloc(sizeof(*p->u.data));
                    if (!np)
                    {
                        printf("json: malloc failed\n");
                        result = false;
                        break;
                    }
                    p->type = JSON_OBJECT;
                    p->u.data = np;
                    memset(np, 0, sizeof(*np));
                    np->up = p;

                    p = np;
                    expect_name = true;
                    break;
                }
                case '[':
                {
                    dprintf("[%d] read value array\n", pos);

                    json_data_t *np = malloc(sizeof(*np));
                    if (!np) {
                        printf("json: malloc failed\n");
                        result = false;
                        break;
                    }
                    p->type = JSON_ARRAY;
                    p->u.data = np;
                    memset(np, 0, sizeof(*np));
                    np->up = p;

                    p = np;
                    break;
                }

                case ',':
                    if (p->name) {
                        dprintf("[%d] read another value object\n", pos);

                        json_data_t *np = malloc(sizeof(*p->u.data));
                        if (!np)
                        {
                            printf("json: malloc failed\n");
                            result = false;
                            break;
                        }
                        memset(np, 0, sizeof(*np));
                        np->up = p->up;

                        p->next = np;

                        p = np;
                        expect_name = true;
                    }
                    else {
                        dprintf("[%d] read another array value\n", pos);

                        json_data_t *np = malloc(sizeof(*np));
                        if (!np) {
                            printf("json: malloc failed\n");
                            result = false;
                            break;
                        }
                        memset(np, 0, sizeof(*np));
                        np->up = p->up;

                        p->next = np;

                        p = np;
                    }
                    break;

                case '}':
                    dprintf("[%d] closed object %s\n", pos, p->name ? p->name : "");
                    if (p->up)
                    {
                        p = p->up;
                    }

                    break;

                case ']':
                    dprintf("[%d] closed array\n", pos);
                    p = p->up;
                    break;

                default:
                    dprintf("[%d] read number\n", pos);
                    if (*s >='0' && *s <= '9')
                    {
                        p->type = JSON_NUMBER;
                        if (sscanf(s, "%Lf", &p->u.number) == 1)
                        {
                            s++;
                            while ((*s >= '0' && *s <= '9') || *s == '.')
                            {
                                s++;
                            }
                            continue;
                        }
                    }
                    
                    dprintf("[%d] error value\n", pos);
                    result = false;
                    break;
            }
        }

        else 
        {
            dprintf("[%d] error options\n", pos);
            result = false;
        }

        if (result)
        {
            s++;
            pos++;
        }
    }

    if (!result && p)
    {
        printf("json: failed at pos %d - <%s>\n", pos, s);
        json_free_data(p);
        p = NULL;
    }
    
    return p;
}

json_data_t *json_find(json_data_t *data, char *name)
{
    json_data_t *root = data;

    while (data)
    {
        json_data_t *new_data = NULL;

        if (data->name && 
                strcmp(data->name, name) == 0) {
            return data;
        }

        switch (data->type)
        {
            case JSON_ARRAY:
                new_data = data->u.data;
                break;
            case JSON_OBJECT:
                new_data = data->u.data;
                break;
            default:
                break;
        }

        while (!new_data) {
            if (data == root)
            {
                break;
            }

            if (data->next)
            {
                new_data = data->next;
            }
            else if (data->up) {
                data = data->up;
            }
            else {
                break;
            }
        }
        data = new_data;
    }

    return NULL;
}

void json_print(json_data_t *data)
{
    json_data_t *root = data;
    
    int level = 0;

    while (data)
    {
        json_data_t *new_data = NULL;

        for (int i=0; i<level; i++)
            printf("    ");
        printf("<%s> : ", data->name ? data->name : "");
        switch (data->type)
        {
            case JSON_STRING:
                printf("%s", data->u.string);
                break;
            case JSON_BOOL:
                printf("%s", data->u.b ? "true" : "false");
                break;
            case JSON_NUMBER:
                printf("%Lf", data->u.number);
                break;
            case JSON_NULL:
                printf("null");
                break;
            case JSON_ARRAY:
                printf("[");
                level += 1;
                new_data = data->u.data;
                break;
            case JSON_OBJECT:
                printf("{");
                level += 1;
                new_data = data->u.data;
                break;
            default:
                printf("ERROR");
                break;
        }
        printf("\n");

        while (!new_data) {
            if (data == root)
            {
                break;
            }

            if (data->next)
            {
                new_data = data->next;
            }
            else if (data->up) {
                data = data->up;
                level--;
            }
            else {
                break;
            }
        }
        data = new_data;
    }
}

void json_free_data(json_data_t *data)
{
}
