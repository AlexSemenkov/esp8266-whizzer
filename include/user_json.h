/*
 * user_json.h
 *
 *  Created on: 10 dec 2016
 *      Author: asemenkov
 */

#ifndef TEST_T_USER_JSON_H_
#define TEST_T_USER_JSON_H_

#include "json/jsontree.h"
#include "json/jsonparse.h"

#define JSON_SIZE   		2 * 1024

void json_parse(struct jsontree_context *json, char *ptrJSONMessage);
void json_ws_send(struct jsontree_value *tree, const char *path, char *pbuf);
struct jsontree_value *find_json_path(struct jsontree_context *json, const char *path);
int json_putchar(int c);

#endif /* TEST_T_USER_JSON_H_ */
