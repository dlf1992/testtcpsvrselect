/******************************************************************
* Copyright(c) 2020-2028 ZHENGZHOU Tiamaes LTD.
* All right reserved. See COPYRIGHT for detailed Information.
*
* @fileName: tcpserver.cpp
* @brief: TCP通信服务端实现
* @author: dinglf
* @date: 2020-12-19
* @history:
*******************************************************************/
#include "tcpserver.h"
TcpServer* TcpServer::m_pTcpServer = NULL;
bool TcpServer::init(unsigned short svrport,ptcpFun callback,pNotifyFun notifycallback)
{
	m_port = svrport;
	Task::task_callback = callback;
	m_notifyfunc = notifycallback;
	bzero(&ServerAddr, sizeof(ServerAddr));
	//配置ServerAddr
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(m_port);
    ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //创建Socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        //printf("TcpServer socket init error\n");
        return false;
    }
    int opt = 1;
    // sockfd为需要端口复用的套接字
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt));
	
    int ret = bind(sockfd, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr));
    if(ret < 0)
    {
        //printf("TcpServer bind init error\n");
        return false;
    }
    ret = listen(sockfd, 10);
    if(ret < 0)
    {
        //printf("TcpServer listen init error\n");
        return false;
    }
	//设置非阻塞
	setnonblocking(sockfd);
	printf("sockfd = %d\n",sockfd);
	//printf("TcpServer init success.\n");
	return true;
    //pool = new threadpool<BaseTask>(threadnum);  //创建线程池	
}
bool TcpServer::startpool()
{
    pool = new threadpool<BaseTask>(threadnum);  //创建线程池
	if(NULL == pool)
	{
		//printf("pool=NULL.\n");
		return false;
	}
    pool->start();   //线程池启动
	return true;
}
void TcpServer::addclienttime(int fd,const char* clientinfo) 
{
	unsigned long timestamp;
	timestamp = GetSysTime();
	string client  = clientinfo;
	activeconnectmaplocker.mutex_lock();
	m_activeclient.insert(pair<int,unsigned long>(fd,timestamp));
	m_connecttime.insert(pair<int,unsigned int>(fd,timestamp));
	m_clientinfo.insert(pair<int,string>(fd,client));
	activeconnectmaplocker.mutex_unlock();	
}
void TcpServer::removeclienttime(int fd) 
{
	activeconnectmaplocker.mutex_lock();
	map<int,unsigned long>::iterator iter;
	iter = m_activeclient.find(fd);
	if(iter != m_activeclient.end())
	{
		m_activeclient.erase(iter);
	}
	map<int,unsigned int>::iterator iter1;
	iter1 = m_connecttime.find(fd);
	if(iter1 != m_connecttime.end())
	{
		m_connecttime.erase(iter1);
	}
	map<int,string>::iterator iter2;
	iter2 = m_clientinfo.find(fd);
	if(iter2 != m_clientinfo.end())
	{
		m_clientinfo.erase(iter2);
	}	
	activeconnectmaplocker.mutex_unlock();	
}
void TcpServer::clearclienttime()
{
	activeconnectmaplocker.mutex_lock();
	map<int,unsigned long>::iterator iter;
	for(iter=m_activeclient.begin();iter!=m_activeclient.end();)
	{
		m_activeclient.erase(iter++);
	}
	map<int,unsigned int>::iterator iter1;
	for(iter1=m_connecttime.begin();iter1!=m_connecttime.end();)
	{
		m_connecttime.erase(iter1++);
	}
	map<int,string>::iterator iter2;
	for(iter2=m_clientinfo.begin();iter2!=m_clientinfo.end();)
	{
		m_clientinfo.erase(iter2++);
	}	
	activeconnectmaplocker.mutex_unlock();		
}
void TcpServer::updateclienttime(int fd) 
{
	unsigned long timestamp;
	timestamp = GetSysTime();
	activeconnectmaplocker.mutex_lock();
	map<int,unsigned long>::iterator iter;
	iter = m_activeclient.find(fd);
	if(iter != m_activeclient.end())
	{
		m_activeclient[fd] = timestamp;
	}
	else
	{
		m_activeclient.insert(pair<int,unsigned long>(fd,timestamp));
	}
	activeconnectmaplocker.mutex_unlock();	
}
void TcpServer::setclientfd()
{
	activeconnectmaplocker.mutex_lock();
	map<int,string>::iterator iter;
	for(iter=m_clientinfo.begin();iter!=m_clientinfo.end();++iter)
	{
		//printf("setclientfd fd = %d\n",iter->first);
		FD_SET(iter->first,&rest);
	}	
	activeconnectmaplocker.mutex_unlock();	
}
int TcpServer::getclientfd()
{
	int fd = -1;
	activeconnectmaplocker.mutex_lock();
	map<int,string>::iterator iter;
	for(iter=m_clientinfo.begin();iter!=m_clientinfo.end();++iter)
	{
		if(FD_ISSET(iter->first,&rest) == 1)
		{
			fd = iter->first;
			break;
		}
	}	
	activeconnectmaplocker.mutex_unlock();
	//printf("getclientfd fd = %d\n",fd);
	return fd;
}
int TcpServer::getmaxfd()
{
	int maxfd = -1;
	activeconnectmaplocker.mutex_lock();
	map<int,string>::iterator iter;
	for(iter=m_clientinfo.begin();iter!=m_clientinfo.end();++iter)
	{
		if(maxfd < iter->first)
		{
			maxfd = iter->first;
		}
	}	
	activeconnectmaplocker.mutex_unlock();
	if(maxfd < sockfd)
	{
		maxfd = sockfd;
	}
	//printf("getmaxfd maxfd = %d\n",maxfd);
	return maxfd;
}
void TcpServer::selectloop()
{
    //pool->start();   //线程池启动
    char tmp[32];
	char notifydata[64];
    while(!is_stop)
    {
		FD_ZERO(&rest);//empty
		//FD_ZERO(&west);
		FD_SET(sockfd,&rest);//set
		//fd_set客户端fd
		setclientfd();		
		//FD_SET(sockfd,&west);//
		//switch(select(sockfd+1,&rest,&west,NULL,NULL))
        //select超时3s
        //struct timeval timeout={3,0};
        //取消超时时间
        int maxfd = -1;
		maxfd = getmaxfd();
		if(maxfd == -1)
		{
			continue;
		}
		switch(select(maxfd+1,&rest,NULL,NULL,NULL))
		{
			 case 0:  //timeout	
				printf("timeout...\n");
				break;
			 case -1: //error
				printf("there are bug\n");
				printf("errno = %d\n",errno);
				break;
			 default: //data ready
				{
					if(FD_ISSET(sockfd,&rest))//新连接
					{
						printf("new client connect.\n");
		                struct sockaddr_in clientAddr;
		                socklen_t len = sizeof(clientAddr);
		                int confd = accept(sockfd, (struct sockaddr *)
		                        &clientAddr, &len);
		                int port = ntohs(clientAddr.sin_port);
		                struct in_addr in  = clientAddr.sin_addr;
		                char str[INET_ADDRSTRLEN];   //INET_ADDRSTRLEN这个宏系统默认定义 16
		                //成功的话此时IP地址保存在str字符串中。
		                inet_ntop(AF_INET,&in, str, sizeof(str));
		                printf("client ip:port  %s : %d,confd: %d\n",str,port,confd);
		                //printf("client confd: %d\n",confd);
						if(confd == -1)
						{
							//printf("errno = %d.\n",errno);
						}
						//设置非阻塞
						setnonblocking(confd);
						addclienttime(confd,(const char*)str);
						if(m_notifyfunc != NULL)
						{
							memset(&m_eventnotify,0,sizeof(EVENTNOTIFY));
							m_eventnotify.cmd = 0x01;//连接
							m_eventnotify.fd = confd;
							memset(tmp,0,sizeof(tmp));
							sprintf(tmp,"%s:%d",str,port);
							memcpy(&m_eventnotify.clientinfo,tmp,32);
							memset(notifydata,0,sizeof(notifydata));
							memcpy(notifydata,&m_eventnotify,sizeof(EVENTNOTIFY));
							m_notifyfunc(notifydata,sizeof(EVENTNOTIFY));
						}
					}
					else //有客户端数据到来
					{
						printf("client data reached.\n");
						int fd = getclientfd();
						if(fd != -1)
						{
							char buffer[MAX_BUFFER];
			                readagain:	memset(buffer, 0, sizeof(buffer));
			                int ret = read(fd, buffer, MAX_BUFFER - 1);
			                if(ret == 0)  //某个fd关闭了连接，从Epoll中删除并关闭fd
			                {
			                    shutdown(fd, SHUT_RDWR);
								close(fd);
			                    printf("%d logout\n", fd);
								removeclienttime(fd);
								if(m_notifyfunc != NULL)
								{
									memset(&m_eventnotify,0,sizeof(EVENTNOTIFY));
									m_eventnotify.cmd = 0x02;//断开
									m_eventnotify.fd = fd;
									memset(notifydata,0,sizeof(notifydata));
									memcpy(notifydata,&m_eventnotify,sizeof(EVENTNOTIFY));
									m_notifyfunc(notifydata,sizeof(EVENTNOTIFY));
								}
								Task::clearRingBuffer(fd);
			                    continue;
			                }
			                else if(ret < 0)//读取出错，尝试再次读取
			                {
			                    if(errno == EAGAIN)
			                    {
			                        //printf("read error! errno = EAGAIN,continue.\n");
			                        break;
			                    }
								else if(errno == EINTR)
								{
			                        //printf("read error! errno = EINTR,readagain.\n");
			                        goto readagain;						
								}
								else 
								{
									//printf("read error! errno = %d,break.\n",errno);
									break;
								}
			                }
			                else//成功读取，向线程池中添加任务
			                {
			                    //printf("received data,fd = %d,datalen = %d\n",fd,ret);
								BaseTask *task = NULL;
			                    task = new Task(buffer,ret,fd);
								if(NULL == task)
								{
									//printf("task=NULL.\n");
									continue;
								}
								if(NULL == pool)
								{
									//printf("pool=NULL.\n");
									break;
								}
								////printf("epoll task = %p\n",task);
			                    pool->append_task(task);
								updateclienttime(fd);
			                }
						}
					}
				}				
				break;
		}
    }
    close(sockfd);//结束。

    //pool->stop();
}
void TcpServer::disconnect(int fd)
{
	char notifydata[64];
    shutdown(fd, SHUT_RDWR);
    close(fd);
	removeclienttime(fd);
	if(m_notifyfunc != NULL)
	{
		memset(&m_eventnotify,0,sizeof(EVENTNOTIFY));
		m_eventnotify.cmd = 0x02;//断开
		m_eventnotify.fd = fd;
		memset(notifydata,0,sizeof(notifydata));
		memcpy(notifydata,&m_eventnotify,sizeof(EVENTNOTIFY));
		m_notifyfunc(notifydata,sizeof(EVENTNOTIFY));
	}	
	Task::clearRingBuffer(fd);
    //printf("%d logout\n", fd);
}
void TcpServer::stoppool()
{
	if(NULL == pool)
	{
		//printf("pool=NULL.\n");
		return;
	}
    pool->stop();   //线程池停止	
}
int  TcpServer::senddata(int fd,const char *data,int datalen)
{
	bool  ret = false;
	activeconnectmaplocker.mutex_lock();
	map<int,string>::iterator iter;
	iter = m_clientinfo.find(fd);
	if(iter != m_clientinfo.end())
	{
		////printf("found fd,fd = %d\n",fd);
		ret = true;
	}
	else
	{
		ret = false;
	}
	activeconnectmaplocker.mutex_unlock();
	if(ret)
	{
		int send_len = 0;
		send_len = write(fd,data,datalen);
		return send_len;
	}
	else
	{
		return -1;
	}
}