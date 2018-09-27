#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <arpa/inet.h>
#include "libevent_http.h"
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <signal.h>
#define DEFAULT_PORT 9999
#define PATH "./"
int main(int argc, const char* argv[])
{
    int port=DEFAULT_PORT;
    const char*workpath=PATH;
    if(argc==3)
    {
        port=atoi(argv[1]);
        workpath=argv[2];
    }
    printf("use port:%d, workpath:%s\n",port,workpath);

    chdir(workpath);

    struct event_base *base=NULL;
    struct evconnlistener*listener=NULL;
    struct event*signal_event=NULL;//信号事件????

    struct sockaddr_in serv;
    memset(&serv,0,sizeof(serv));
    serv.sin_family=AF_INET;
    serv.sin_port=htons(port);


    base=event_base_new();//创建事件框架
    
    if(NULL==base)
    {
        printf("框架创建出错\n");
        exit(1);
    }
    
    //创建事件连接监听器,即listener
    listener=evconnlistener_new_bind(base,listen_cb,(void*)base,LEV_OPT_CLOSE_ON_FREE | LEV_OPT_CLOSE_ON_FREE,-1,(struct sockaddr*)&serv,sizeof(serv));
    if(NULL==listener)
    {
        perror("50 err");
        printf("%d listener_new_bind err\n",__LINE__);
        exit(1);
    }

    //创建信号捕捉
    signal_event=evsignal_new(base,SIGINT,signal_cb,(void*)base);
    if(NULL==signal_event)
    {
        printf("%d listener_new_bind err\n",__LINE__);
        exit(1);
    }

    //事件循环
    event_base_dispatch(base);

    evconnlistener_free(listener);
    event_free(signal_event);
    event_base_free(base);

    printf("%s主资源全部释放%d\n",__FUNCTION__,__LINE__);
    return 0;
}
