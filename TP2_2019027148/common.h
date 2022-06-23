#pragma once

#include <stdlib.h>

#include <arpa/inet.h>

int addrparse(const char *addrstr, const char *portstr, struct sockaddr_storage *storage);
void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);
int server_sockaddr_init(const char *port, const char *portstr, struct sockaddr_storage *storage);
void logexit(const char *msg);
float get_round(float var);
float get_random();