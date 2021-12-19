#ifndef CLIENT_H
#define CLIENT_H

#include "structs.h"

/**
 * Client_connection:
 * gives request
 * makes decision about server_connection
 * connects to cache and starts reading
 */

Request *make_request();
int free_request(Request *r);

Client_connection *init_connection();
int free_connection(Client_connection *c);

void * client_body(void *raw_struct);

#endif