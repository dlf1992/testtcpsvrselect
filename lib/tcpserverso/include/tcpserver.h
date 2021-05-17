/******************************************************************
* Copyright(c) 2020-2028 ZHENGZHOU Tiamaes LTD.
* All right reserved. See COPYRIGHT for detailed Information.
*
* @fileName: tcpserver.h
* @brief: TCP通信服务
* @author: dinglf
* @date: 2020-12-19
* @history:
*******************************************************************/
#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
//#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <map>
#include <sys/time.h>
#include <netinet/tcp.h>
#include "thread_pool.h"
#include "taskprocess.h"
#include "pub.h"
#include "clientbuffer.h"

using namespace std;

#define THREAD_NUM 5
typedef struct EVENT_NOTIFY
{
	int cmd;//0x01 连接 0x02 断开
	int fd;
	char clientinfo[32];//ip:port
}__attribute__((packed)) EVENTNOTIFY; 
typedef  int (*pNotifyFun)(const char *,int);
class TcpServer
{
private:
	bool is_stop;   //是否停止select的标志
	int threadnum;   //线程数目
	int sockfd;     //监听的fd
	unsigned short m_port; //端口
	fd_set rest, west;
	threadpool<BaseTask> *pool;   //线程池的指针
	struct sockaddr_in ServerAddr;   //绑定的sockaddr
	map<int,unsigned long> m_activeclient; //map存储收到客户端信息时间戳
	map<int,unsigned int> m_connecttime;    //存储客户端初次连接时间
	map<int,string> m_clientinfo;			//存储客户端信息fd ip:port
	mutex_locker activeconnectmaplocker;	//上面map的互斥锁
	EVENTNOTIFY m_eventnotify;
	pNotifyFun m_notifyfunc;//客户端连接断开事件通知函数
	CientBuffer *pclient_buffer;//客户端数据缓冲区类指针对象
public://构造函数
	TcpServer()
	{}
	TcpServer(int thread) : is_stop(false) , threadnum(thread) , pool(NULL),m_notifyfunc(NULL),pclient_buffer(NULL)
	{
	}
	~TcpServer()  //析构
	{
		is_stop = true;
		if(pool != NULL)
		{
			pool->stop();
			delete pool;
			pool = NULL;
		}
		if(pclient_buffer != NULL)
		{
			delete pclient_buffer;
			pclient_buffer = NULL;
		}		
	}
	bool init(unsigned short svrport,ptcpFun callback,pNotifyFun notifycallback,pReadPacketFun readpacket);
	void setclientfd();
	int  getclientfd();
	int  getmaxfd();
	void selectloop();
    bool startpool();
	void addclienttime(int fd,const char* clientinfo);
	void removeclienttime(int fd);
	void clearclienttime();
	void updateclienttime(int fd);
    void disconnect(int fd);
	void stoppool();
	int  senddata(int fd,const char *data,int datalen);

	static int setnonblocking(int fd)  //将fd设置称非阻塞
	{
		int old_option = fcntl(fd, F_GETFL);
		int new_option = old_option | O_NONBLOCK;
		fcntl(fd, F_SETFL, new_option);
		return old_option;
	}
 
};
#endif