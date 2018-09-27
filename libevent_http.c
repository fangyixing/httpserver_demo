#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <strings.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <event2/listener.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include "libevent_http.h"

void listen_cb(struct evconnlistener*listener,evutil_socket_t  cfd,struct sockaddr*server,int len,void*arg)
{
    printf("-----监听器回调-----%s\n",__FUNCTION__);
    struct event_base*base=(struct event_base*)arg;
    struct bufferevent*bev=NULL;
    //将cfd封装成带缓冲区的event,并怼到框架上
    bev=bufferevent_socket_new(base,cfd,BEV_OPT_CLOSE_ON_FREE);//创建带缓冲区的event
    if(!bev)
    {
        printf("bev err\n");
        return;
    }
    bufferevent_flush(bev,EV_READ | EV_WRITE,BEV_NORMAL);
    bufferevent_setcb(bev,read_cb,NULL,event_cb,NULL);
    bufferevent_enable(bev,EV_READ);
}



void signal_cb(evutil_socket_t sig,short events,void *arg)
{
    struct event_base*base=(struct event_base*)arg;
    struct timeval delay={1,0};
    printf("caught an interrup\n");
    event_base_loopexit(base,&delay);
    return ;
}




void event_cb(struct bufferevent* bev,short events,void* arg)
{
    printf("-------begin call%s----\n",__FUNCTION__);
    if(events & BEV_EVENT_EOF)
    {
        printf("Connection closed \n");
    }
    else if(events & BEV_EVENT_ERROR)
    {
        printf("an error happened\n");
    }
    bufferevent_free(bev);
    printf("------end call%s------\n",__FUNCTION__);
}





void read_cb(struct bufferevent* bev,void* arg)
{
    printf("读回调函数启用%s\n",__FUNCTION__);
    char buf[4096]={0};
    char method[64],path[64],protocol[32];
    bufferevent_read(bev,buf,sizeof(buf));
    printf("buf:%s\n",buf);
    sscanf(buf,"%[^ ] %[^ ] %[^ ]",method,path,protocol);
    printf("%d method: %s path:%s protocol:%s",__LINE__,method,path,protocol);
    if(strcasecmp(method,"GET")==0)
    {
        response_http(bev,path);
    }
    printf("读回调执行完成\n");
}




void response_http(struct bufferevent*bev,char*path)
{
    //已经判断为GET模式
    //解码
    strdecode(path,path);
    const char*filename=path+1;
    printf("%d %s\n",__LINE__,path);
    if(strcmp(path,"/")==0)
    {
        filename="./";
    }
    

    struct stat sb;
    if(stat(filename,&sb)<0)
    {
        perror("open err\n");
        
        
       // send_error(bev);//估计是发送信号
        
        return ;
    }
    if(S_ISREG(sb.st_mode))
    {
        send_header(bev,200,"OK",get_file_type(filename),(long)sb.st_size);
        send_file(filename,bev);
    }
    else if(S_ISDIR(sb.st_mode))
    {
        send_header(bev,200,"OK",get_file_type(".html"),(long)sb.st_size);
        send_dir(filename,bev);
    }
}





void send_dir(const char*dirname,struct bufferevent*bev)
{
    char path[64]={0};
    char buf[4096]={0};
    char enstrname[64]={0};
    struct stat sb;

    sprintf(buf,"<html><head><title>%s</title></head>",dirname);
    sprintf(buf+strlen(buf),"<body><h1>当前目录:%s</h1><table>",dirname);
    
    //添加目录下内容
    struct dirent** ptr=NULL;
    int num=scandir(dirname,&ptr,NULL,alphasort);
    for(int i=0;i<num;++i)
    {
        //对目录下内容进行编码
        strencode(enstrname,sizeof(enstrname),ptr[i]->d_name);
        sprintf(path,"%s/%s",dirname,ptr[i]->d_name);
        printf("--------path=%s\n",path);
        if(lstat(path,&sb)<0)
        {
            printf("%d err",__LINE__);
            return ;
        }
        else
        {
            if(S_ISDIR(sb.st_mode))
            {
                sprintf(buf+strlen(buf),"<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>\n",
                        enstrname,ptr[i]->d_name,sb.st_size);
            }
            else if(S_ISREG(sb.st_mode))
            {
                sprintf(buf+strlen(buf),"<tr><td><a href=\"%s\">%s</a><td>%ld</td></tr>\n",
                        enstrname,ptr[i]->d_name,sb.st_size);
            }
        }
        bufferevent_write(bev,buf,strlen(buf));
        memset(buf,0,sizeof(buf));
    }
    sprintf(buf,"</table></body></html>");
    bufferevent_write(bev,buf,strlen(buf));
    printf("------dir send ok-----\n");
    return ;
}





void send_header(struct bufferevent *bev,int no,const char*statinfo,const char*type,long len)
{
    char buf[1024]={0};
    sprintf(buf,"http/1.1 %d %s\r\n",no,statinfo);
    sprintf(buf+strlen(buf),"Content-Type:%s\r\n",type);
    sprintf(buf+strlen(buf),"Content-Lenth:%ld\r\n",len);
    sprintf(buf+strlen(buf),"\r\n");
    //三连发

    bufferevent_write(bev,buf,strlen(buf));
    memset(buf,0,sizeof(buf));
}




//写文件
void send_file(const char*filename,struct bufferevent*bev)
{
    int fd=open(filename,O_RDONLY);
    if(fd== -1)
    {
        perror("openfile error");
        printf("error line:%d\n",__LINE__);
        exit(1);
    }
    int size=0;
    char buf[4096]={0};
    while((size=read(fd,buf,sizeof(buf)))>0)
    {
        bufferevent_write(bev,buf,size);
        memset(buf,0,sizeof(buf));
    }
    if(size== -1)
    {
        perror("read file error");
        printf("error line:%d\n",__LINE__);
        bufferevent_free(bev);
    }
}





//编码
void strencode(char* to, size_t tosize, const char* from)
{
    int tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from)
    {
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0)
        {
            *to = *from;
            ++to;
            ++tolen;

        }
        else
        {
            sprintf(to, "%%%02x", (int) *from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}





//解码过程
//%20URL编码中的' '(space)
//%21 '!' %22 '"' %23 '#' %24 '$'
//%25 '%' %26 '&' %27 ''' %28 '('......
//相关知识html中的‘ ’(space)是&nbsp')''

void strdecode(char *to,char*from)
{
    for(;*from !='\0';++to,++from)
    {
        if(from[0]=='%'&&isxdigit(from[1]&&isxdigit(from[2])))
        {
            //依次判断from中%20三个字符
            *to =hexit(from[1])*16+ hexit(from[2]);
            //移过已经处理的两个字符
            from += 2;
        }
        else
        {
            *to=*from;
        }
    }
    *to='\0';
}

//16-->10
int hexit(char c)
{
    if(c >='0' && c<='9')
    {
        return c-'0';
    }
    if(c >='0' && c<='f')
        return c-'a'+10;
    if(c >='A' && c<='F')
        return c-'A'+10;
    return 0;
}



// 通过文件名获取文件的类型
const char *get_file_type(const char *name)
{
    char* dot;

    // 自右向左查找‘.’字符, 如不存在返回NULL
    dot = strrchr(name, '.');   
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav" ) == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}
