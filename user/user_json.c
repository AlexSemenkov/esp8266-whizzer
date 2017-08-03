/*
 * user_json.c
 *
 *  Created on: 10 dec 2016
 *      Author: Espressif Systems (Shanghai) Pte., Ltd.
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "mem.h"

#include "user_json.h"

LOCAL char *json_buf;
LOCAL int pos;
LOCAL int size;


/******************************************************************************
 * FunctionName : find_json_path
 * Description  : find the JSON format tree's path
 * Parameters   : json -- a pointer to a JSON set up
 *                path -- a pointer to the JSON format tree's path
 * Returns      : a pointer to the JSON format tree
*******************************************************************************/
struct jsontree_value *ICACHE_FLASH_ATTR find_json_path(struct jsontree_context *json, const char *path) {

    struct jsontree_value *value;
    const char *start;
    const char *end;
    int len;

    value = json->values[0];
    start = path;

    do {
        end = (const char *)os_strstr(start, "/");

        if (end == start) {
            break;
        }

        if (end != NULL) {
            len = end - start;
            end++;
        } else {
            len = os_strlen(start);
        }

        if (value->type != JSON_TYPE_OBJECT) {
        	value = NULL;
        } else {
            struct jsontree_object *object;
            int i;

            object = (struct jsontree_object *)value;
            value = NULL;

            for (i = 0; i < object->count; i++) {
                if (os_strncmp(start, object->pairs[i].name, len) == 0) {
                	value = object->pairs[i].value;
                    json->index[json->depth] = i;
                    json->depth++;
                    json->values[json->depth] = value;
                    json->index[json->depth] = 0;
                    break;
                }
            }
        }

        start = end;
    } while (end != NULL && *end != '\0' && value != NULL);

    json->callback_state = 0;
    return value;
}


/******************************************************************************
 * FunctionName : json_putchar
 * Description  : write the value to the JSON  format tree
 * Parameters   : c -- the value which write the JSON format tree
 * Returns      : result
*******************************************************************************/
int ICACHE_FLASH_ATTR json_putchar(int c) {

    if (json_buf != NULL && pos <= size) {
        json_buf[pos++] = c;
        return c;
    }

    return 0;
}


/******************************************************************************
 * FunctionName : json_ws_send
 * Description  : set up the JSON format tree for string
 * Parameters   : tree -- a pointer to the JSON format tree
 *                path -- a pointer to the JSON format tree's path
 *                bufPtr -- a pointer for the data sent
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR json_ws_send(struct jsontree_value *tree, const char *path, char *bufPtr) {

	struct jsontree_context json;
    /* maxsize = 128 bytes */
    json_buf = (char *)os_malloc(JSON_SIZE);

    /* reset state and set max-size */
    /* NOTE: packet will be truncated at 512 bytes */
    pos = 0;
    size = JSON_SIZE;

    json.values[0] = (struct jsontree_value *)tree;
    jsontree_reset(&json);
    find_json_path(&json, path);
    json.path = json.depth;
    json.putchar = json_putchar;

    while (jsontree_print_next(&json) && json.path <= json.depth);

    json_buf[pos] = 0;
    os_memcpy(bufPtr, json_buf, pos);
    os_free(json_buf);
}


/******************************************************************************
 * FunctionName : json_parse
 * Description  : parse the data as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 *                ptrJSONMessage -- A pointer to the data
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR json_parse(struct jsontree_context *json, char *jsonMessagePtr) {

    /* Set value */
    struct jsontree_value *value;
    struct jsontree_callback *cb;
    struct jsontree_callback *callBack = NULL;

    while ((value = jsontree_find_next(json, JSON_TYPE_CALLBACK)) != NULL) {
    	cb = (struct jsontree_callback *)value;

        if (cb == callBack) {
            continue;
        }

        callBack = cb;

        if (cb->set != NULL) {
            struct jsonparse_state js;

            jsonparse_setup(&js, jsonMessagePtr, os_strlen(jsonMessagePtr));
            cb->set(json, &js);
        }
    }
}
