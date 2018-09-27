#pragma once
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
void listen_cb(struct evconnlistener*listener,evutil_socket_t cfd,struct sockaddr*server,int len,void*arg);
void event_cb(struct bufferevent* bev,short events,void* arg);
void read_cb(struct bufferevent* bev,void* arg);
void response_http(struct bufferevent*bev,char*path);
void send_dir(const char*dirname,struct bufferevent*bev);
void send_header(struct bufferevent *bev,int no,const char*statinfo,const char*type,long len);
void send_file(const char*filename,struct bufferevent*bev);
void strencode(char* to, size_t tosize, const char* from);
void strdecode(char *to,char*from);
int hexit(char c);
void signal_cb(evutil_socket_t sig, short events, void *user_data);
const char *get_file_type(const char *name);
