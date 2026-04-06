#ifndef JSON_H
#define JSON_H

typedef enum
{
    JSON_STRING, // <"some string">
    JSON_BOOL, // <true>, <false>
    JSON_NUMBER, // <34324325>
    JSON_ARRAY, // [34, "text", true]
    JSON_NULL, // <null>
    JSON_OBJECT // <{"name": 12, "name1", "value1"}>

} json_type_t;

typedef struct json_data_t
{
    char *name;
    json_type_t type;

    union {
        char *string;
        long double number;
        bool b;
        struct json_data_t *data;
    } u;

    struct json_data_t *next;
    struct json_data_t *up;
} json_data_t;

json_data_t *json_parse_data(char *data_str, uint size);
json_data_t *json_find(json_data_t *data, char *name);
void json_print(json_data_t *data);
void json_free_data(json_data_t *data);

#endif /* JSON_H */
