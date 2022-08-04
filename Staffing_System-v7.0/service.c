#include <stdio.h>
#include <sys/types.h>         
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <strings.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>


#define IP    "192.168.250.100"
#define PORT  8888

struct user
{
    int sig; //发送的信号，给服务器
    int search_sig; //进入查找状态，按照名字查找的信号
    int mod_sig; //进入修改状态，要修改的属性的信号
    char usname[128];  //缓存用户名，删除时使用
    char password[128]; //缓存密码
    char mod_num[128]; //修改时只认的工号,和删除时
    char search_name[128]; //查询时按照名字来查询
};

struct staff
{
    char num[128]; //工号
    char user_type[128]; //用户类型
    char name[128]; //姓名
    char password[128]; //密码
    char age[128]; //年龄	
    char phone[128]; //电话	
    char addr[128];//地址	
    char position[128];//职位	
    char time[128];//入职年月	
    char grade[128];//等级	 
    char salary[128];//工资
};

struct logname
{
    char log1[128];
};

struct history
{
    char time[128];
    char user[128];
    char event[128];
};

int sfd;  //套接字
struct sockaddr_in sins; //绑定服务器端口结构体
struct sockaddr_in cinss; //绑定服务器端口结构体
struct sockaddr_in cins[128]; //接收客户端端口结构体
struct logname lognamel[128];   //登录名单
int res; //
fd_set rfds;    //创建读表
fd_set temp;    //创建读表
int maxfd;      //最大文件描述符
char rev_buf[128]="";   //接收的buf
char sed_buf[128]="";   //发送的buf
struct user user_buf; //接收的数据包buf
struct staff staff_buf; //发送staff的结构体
struct history history_buf; //发送history的结构体
char *errmsg = NULL; //sql接收错误码的
char **result = NULL; //get_table
int row, column; //get_table 
sqlite3 *db = NULL;
char user_type_if[10]="1";
char user_type_if_0[10]="0";

int root_log(struct user* log,int fd);
int root_search(struct user* log,int fd);
int root_mod(struct user* log,int fd);
int root_insert(struct user* log,int fd);
int root_history(struct user* log,int fd);
int root_del(struct user* log,int fd);
int user_log(struct user* log,int fd);
int user_search(struct user* log,int fd);
int user_mod(struct user* log,int fd);
int root_exit(struct user* log,int fd);
int user_exit(struct user* log,int fd);

int main(int argc, char const *argv[])
{
    //3.1.数据库创建并打开
    db = NULL;
	if(sqlite3_open("./my.db", &db) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ code:%d sqlite3_open:%s\n", __LINE__, \
				sqlite3_errcode(db), sqlite3_errmsg(db));
		return -1;
	}
	printf("数据库打开成功\n");
    //创建表格
	char sql[256] = "create table if not exists staff (\
    num char primary key, user_type char, name char,\
    password char,age char,phone char,addr char,position char,\
    time char,grade char,salary char)";
	
	
	if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_exec:%s\n", __LINE__, errmsg);
		return -1;
	}
	printf("创建staff表格成功\n");



    sprintf(sql,"select name , password from staff where name='%s' and num=\'%s\';",
    "admin","0");
    printf("sql:%s\n",sql);
    if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
		return -1;
	}
    if(row==0)  //查不到在表中
    { 
         sprintf(sql,"insert into staff values (\'0\',\'0\',\'admin\',\'admin\',\'18\',\'123456\',\
        \'北京\',\'boss\',\'1988/7/22\',\'10\',\'2000\');");
        if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	    {
		    fprintf(stderr, "__%d__ sqlite3_exec:%s\n", __LINE__, errmsg);
		    return -1;
	    }
	    printf("admin添加成功\n");
    }


    sprintf(sql,"create table if not exists history (\
    nowtime char, handuser char , event char);");
    if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_exec:%s\n", __LINE__, errmsg);
		return -1;
	}
	printf("创建history表格成功\n");
    //1.1.socket
    sfd=socket(AF_INET,SOCK_STREAM,0);
    if(sfd<0)
    {
        perror("socket error");
        return -1;
    }
    //允许端口快速重用
	int reuse = 1;
	if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	{
		perror("setsockopt");
		return -1;
	}
	printf("允许端口快速重用\n");
    //1.2.bind
    sins.sin_family=AF_INET;
    sins.sin_port=htons(PORT);
    sins.sin_addr.s_addr=inet_addr(IP);
    res=bind(sfd,( struct sockaddr *)&sins,sizeof(sins));
    if(res<0)
    {
        perror("bind error");
        return -1;
    }
    //1.3.listen
    listen(sfd,10);

    //2.1.清空表
    FD_ZERO(&rfds);
    FD_ZERO(&temp);
    //2.2.加表
    FD_SET(sfd,&rfds);
    maxfd=sfd;  //最大文件描述符

    

    while(1)
    {
        bzero(rev_buf,sizeof(rev_buf));
        //2.3创建一个临时表存原表
        temp=rfds;
        //2.4.select筛选
        select(maxfd+1,&temp,NULL,NULL,NULL);
        //2.5。for遍历到maxfd
        for(int i=0;i<maxfd+1;i++)
        {
            if(!FD_ISSET(i,&temp))
            {
                continue;
            }
            if(FD_ISSET(i,&temp))
            {
                if(i==sfd)  //如果是套接字触发
                {
                    printf("套接字触发\n");
                    //1.4.accept
                    int size_cins=sizeof(cinss);
                    int nsfd=accept(sfd,( struct sockaddr *)&cinss,&size_cins);
                    cins[nsfd]=cinss;
                    FD_SET(nsfd,&rfds);   //新的套接字加入表中
                    maxfd=maxfd>nsfd?maxfd:nsfd;   //更新套接字最大文件描述符的大小
                    printf("[%s:%d]:newfd=%d connect success\n",inet_ntoa(cins[nsfd].sin_addr),ntohs(cins[nsfd].sin_port),nsfd);
                }
                else
                {
                    //如果不是sfd文件描述符，那就是新套接字文件描述符nsfd
                    //1.5.recv
                    memset(&user_buf,0,sizeof(user_buf));
                    res=recv(i,&user_buf,sizeof(user_buf),0);
                    if(res<0)
                    {
                        perror("recv error");
                        return -1;
                    }
                    if(res==0)   //对方套接字关闭，客户端关闭
                    {
                        //删除表中的套接字
                        FD_CLR(i,&rfds);
                        //更新maxfd
                        if(maxfd==i)  //只有i等于maxfd时要更新maxfd
                        {
                            //遍历表来找到最大文件描述符
                            for(int j=maxfd;j>=0;j--)
                            {
                                if(FD_ISSET(j,&rfds))
                                {
                                    maxfd=j;
                                }
                            }
                        }
                    }
                    else //正常接收到数据
                    {
                        printf("收到命令包 user_buf.sig=%d user_buf.search_sig=%d\n",user_buf.sig,user_buf.search_sig);
                        printf("user_buf.sig=%d\n user_buf.search_sig=%d\
                        \nuser_buf.mod_sig=%d\n user_buf.usname=%s\n user_buf.password=%s\
                        \nuser_buf.mod_num=%s\n user_buf.search_name=%s\n",user_buf.sig,\
                        user_buf.search_sig,user_buf.mod_sig,user_buf.usname,\
                        user_buf.password,user_buf.mod_num,user_buf.search_name);
                        switch(user_buf.sig)
                        {
                            case 0:
                                user_log(&user_buf,i);  //普通用户登录
                                break;
                            case 1:
                                user_search(&user_buf,i);  //普通用户查询
                                break;
                            case 2:
                                user_mod(&user_buf,i);  //普通用户修改模式
                                break;
                            case 3:
                                user_exit(&user_buf,i);  //普通用户修改模式
                                break;  

                            case 10:
                                root_log(&user_buf,i);  //超级用户登录
                                break;
                            case 11:
                                root_search(&user_buf,i);  //超级用户查询模式
                                break;
                            case 12:
                                root_mod(&user_buf,i);  //超级用户修改模式
                                break;
                            case 13:
                                root_insert(&user_buf,i);  //超级用户添加
                                break;
                            case 14:
                                root_del(&user_buf,i);  //超级用户查询历史记录
                                break;
                            case 15:  //15什么都没选等待状态
                                root_history(&user_buf,i);
                                break;
                            case 16:  //15什么都没选等待状态
                                root_exit(&user_buf,i);
                                break;
                        }
                        //printf("[%s:%d]:newfd=%d:%s \n",inet_ntoa(cins[i].sin_addr),ntohs(cins[i].sin_port),i,rev_buf);
                    }
                    //1.6.send
                }
            }
           
        }
    }
    
    return 0;
}

//超级用户登录
int root_log(struct user* log,int fd)
{
    printf("root_log\n");
    char getname[128]="";
    strcpy(getname,log->usname);
    //遍历登录名单，看是否已经被登录
    for(int a=0;a<128;a++)
    {
        if(!strcmp(lognamel[a].log1,getname))
        {
            sprintf(sed_buf,"no");
            printf("sed_buf=%s\n",sed_buf);
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“no”
            printf("回答了no\n");
            return -1;
        }
    }
    
    char sql[612] = "";
    sprintf(sql,"select name , password from staff where name='%s' and password='%s' and user_type=\'%s\';",
    log->usname,log->password,user_type_if_0);
    printf("sql:%s\n",sql);
    if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
		return -1;
	}
    if(row==0)  //查不到在表中
    {
        sprintf(sed_buf,"no");
        printf("sed_buf=%s\n",sed_buf);
        send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“no”
        printf("回答了no\n");
        return -1;
    }
    sprintf(sed_buf,"ok");
    send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
    printf("回答了ok\n");
    //登录成功用fd作为下标，存入登陆名单中
    strcpy(lognamel[fd].log1,getname);
    printf("%s\n",lognamel[fd].log1);
    return 0;
}

int root_search(struct user* log,int fd)
{
    printf("root_search\n");
    char sql[612] = "";
    switch(log->search_sig)
    {
        case 0:
            //按名字查
            sprintf(sql,"select * from staff where name=\'%s\';",
            log->search_name);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            int ii = 0, jj=0;
	        int index = 0;
            for(ii=0; ii<(row+1); ii++)
            {
                strcpy(staff_buf.num,result[0+ii*column]);
                strcpy(staff_buf.user_type,result[1+ii*column]);
                strcpy(staff_buf.name,result[2+ii*column]);
                strcpy(staff_buf.password,result[3+ii*column]);
                strcpy(staff_buf.age,result[4+ii*column]);
                strcpy(staff_buf.phone,result[5+ii*column]);
                strcpy(staff_buf.addr,result[6+ii*column]);
                strcpy(staff_buf.position,result[7+ii*column]);
                strcpy(staff_buf.time,result[8+ii*column]);
                strcpy(staff_buf.grade,result[9+ii*column]);
                strcpy(staff_buf.salary,result[10+ii*column]);
                //已经打包完要发送的数据包staff
                printf("按名字查询数据包打包成功\n");
                send(fd,&staff_buf,sizeof(staff_buf),0);
            }
            //释放查询到的结果
            sqlite3_free_table(result);
            break;
        case 1: //读全部
            sprintf(sql,"select * from staff ;");
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            int iii = 0;
	      
            for(iii=0; iii<(row+1); iii++)
            {
                strcpy(staff_buf.num,result[0+iii*column]);
                strcpy(staff_buf.user_type,result[1+iii*column]);
                strcpy(staff_buf.name,result[2+iii*column]);
                strcpy(staff_buf.password,result[3+iii*column]);
                strcpy(staff_buf.age,result[4+iii*column]);
                strcpy(staff_buf.phone,result[5+iii*column]);
                strcpy(staff_buf.addr,result[6+iii*column]);
                strcpy(staff_buf.position,result[7+iii*column]);
                strcpy(staff_buf.time,result[8+iii*column]);
                strcpy(staff_buf.grade,result[9+iii*column]);
                strcpy(staff_buf.salary,result[10+iii*column]);
                //已经打包完要发送的数据包staff
                printf("按名字查询数据包打包成功\n");
                send(fd,&staff_buf,sizeof(staff_buf),0);
            }
            //读完以后发这个表示读完
            sprintf(staff_buf.num,"nack");
            send(fd,&staff_buf,sizeof(staff_buf),0);
            //释放查询到的结果
            sqlite3_free_table(result);
            break;
    }
    return 0;
}

int root_mod(struct user* log,int fd)
{
    printf("root_mod\n");
    char sql[612] = "";
    //选择工号
    sprintf(sql,"select name , password from staff where num='%s';",
    log->mod_num);
    printf("sql:%s\n",sql);
    if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
		return -1;
	}
    if(row==0)  //查不到在表中
    {
        sprintf(sed_buf,"no");
        printf("sed_buf=%s\n",sed_buf);
        send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“no”
        printf("回答了no\n");
        return -1;
    }
    sprintf(sed_buf,"ok");
    send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
    printf("回答了ok\n");
    //确认工号存在,开始修改
    recv(fd,&user_buf,sizeof(user_buf),0); //提取mod_sig
    sprintf(sed_buf,"ok");
    send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
    printf("回答了ok\n"); 
    char toevent[612]=""; //发送给数据库的事件
    char mod_item[128]="";  //修改项
    char mod_result[128]=""; //修改结果
    switch(log->mod_sig)
    {
        case 3:  //姓名
            sprintf(mod_item,"姓名");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set name=\'%s\' where num='%s';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
        case 5:  //年龄
            sprintf(mod_item,"年龄");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set age=\'%s\' where num='%s';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
        case 7:  //地址
            sprintf(mod_item,"地址");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set addr=\'%s\' where num='%s';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
        case 6:  //电话
            sprintf(mod_item,"电话");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set phone=\'%s\' where num='%s';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
        case 8:  //职位
            sprintf(mod_item,"职位");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set position=\'%s\' where num='%s';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
        case 11:  //工资
            sprintf(mod_item,"工资");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set salary=\'%s\' where num='%s';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
        case 9:  //入职年月
            sprintf(mod_item,"入职年月");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set time=\'%s\' where num='%s';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
        case 10:  //等级
            sprintf(mod_item,"等级");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set grade=\'%s\' where num='%s';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
        case 4:  //密码
            sprintf(mod_item,"密码");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set password=\'%s\' where num='%s';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
    }
    strcpy(mod_result,rev_buf);  //历史记录中的修改结果
    //获取时间
    char totime[128]=""; //发送给数据库的时间
    time_t buf;
    time(&buf);
    struct tm* info = localtime(&buf);
	sprintf(totime,"%d年%d月%d日 %02d:%02d:%02d ",info->tm_year+1900, info->tm_mon+1, info->tm_mday, \
			info->tm_hour, info->tm_min,info->tm_sec);
    sprintf(toevent,"%s对%s工号,修改了%s为%s\n",lognamel[fd].log1,\
    log->mod_num,mod_item,mod_result);

    //开始添加历史记录
    sprintf(sql,"insert into history values (\'%s\',\'%s\',\'%s\'\
    );",totime,lognamel[fd].log1,toevent);
    if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_exec:%s\n", __LINE__, errmsg);
		return -1;
	}
	printf("历史记录添加成功\n");
    
}


int root_insert(struct user* log,int fd)
{
    char sql[3000]="";
    recv(fd,&staff_buf,sizeof(staff_buf),0); //拿staff_buf结构体
    //存入数据库
    sprintf(sql,"insert into staff values (\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\
    \'%s\',\'%s\',\'%s\',\'%s\',\'%s\');",staff_buf.num,\
    staff_buf.user_type,staff_buf.name,staff_buf.password,\
    staff_buf.age,staff_buf.phone,staff_buf.addr,staff_buf.position,\
    staff_buf.time,staff_buf.grade,staff_buf.salary);
    if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_exec:%s\n", __LINE__, errmsg);
		return -1;
	}
	printf("admin添加成功\n");
    //成功以后应答ok
    sprintf(sed_buf,"ok");
    send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
    printf("回答了ok\n");

    //添加历史记录
    //获取时间
    char totime[128]=""; //发送给数据库的时间
    char toevent[512]="";
    time_t buf;
    time(&buf);
    struct tm* info = localtime(&buf);
	sprintf(totime,"%d年%d月%d日 %02d:%02d:%02d ",info->tm_year+1900, info->tm_mon+1, info->tm_mday, \
			info->tm_hour, info->tm_min,info->tm_sec);
    sprintf(toevent,"工号%s 添加了 工号%s\n",lognamel[fd].log1,\
    staff_buf.num);
    sprintf(sql,"insert into history values (\'%s\',\'%s\',\'%s\'\
    );",totime,lognamel[fd].log1,toevent);
    if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_exec:%s\n", __LINE__, errmsg);
		return -1;
	}
	printf("历史记录添加成功\n");
}

int root_del(struct user* log,int fd)
{
    printf("root_del\n");
    char sql[612]="";
    //先查看一下是否存在
    sprintf(sql,"select * from staff where num=\'%s\' and name=\'%s\';",\
    user_buf.mod_num,user_buf.usname);
    if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_exec:%s\n", __LINE__, errmsg);
		return -1;
	}
    if(row==0)  //查不到在表中
    {
        sprintf(sed_buf,"no");
        printf("sed_buf=%s\n",sed_buf);
        send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“no”
        printf("回答了no\n");
        return -1;
    }

    //
    sprintf(sql,"delete from staff where num=\'%s\' and name=\'%s\';",\
    user_buf.mod_num,user_buf.usname);
    if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_exec:%s\n", __LINE__, errmsg);
		return -1;
	}
    sprintf(sed_buf,"ok");
    send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
    printf("回答了ok\n");
}



int root_history(struct user* log,int fd)
{
    printf("root_history\n");
    char sql[612]="";
    sprintf(sql,"select * from history ;");
    if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
    {
        fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
        return -1;
    }
    int iii = 0;
    
    for(iii=0; iii<(row+1); iii++)
    {
        strcpy(history_buf.time,result[0+iii*column]);
        strcpy(history_buf.user,result[1+iii*column]);
        strcpy(history_buf.event,result[2+iii*column]);

        //已经打包完要发送的数据包staff
        printf("history数据包打包成功\n");
        send(fd,&history_buf,sizeof(history_buf),0);
    }
    //读完以后发这个表示读完
    sprintf(history_buf.time,"nack");
    send(fd,&history_buf,sizeof(history_buf),0);
    //释放查询到的结果
    sqlite3_free_table(result);
    
}

int root_exit(struct user* log,int fd)
{
    bzero(lognamel[fd].log1,sizeof(lognamel[fd].log1));
    return 0;
}

/************************普通用户*****************************/
int user_log(struct user* log,int fd)
{
    printf("user_log\n");
    char getname[128]="";
    strcpy(getname,log->usname);

    for(int a=0;a<128;a++)
    {
        if(!strcmp(lognamel[a].log1,getname))
        {
            sprintf(sed_buf,"no");
            printf("sed_buf=%s\n",sed_buf);
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“no”
            printf("回答了no\n");
            return -1;
        }
    }

    char sql[612] = "";
    sprintf(sql,"select name , password from staff where name='%s' and password='%s' and user_type=%s;",
    log->usname,log->password,user_type_if);
    printf("sql:%s\n",sql);
    if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
		return -1;
	}
    if(row==0)  //查不到在表中
    {
        sprintf(sed_buf,"no");
        printf("sed_buf=%s\n",sed_buf);
        send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“no”
        printf("回答了no\n");
        return -1;
    }
    sprintf(sed_buf,"ok");
    send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
    printf("回答了ok\n");
    //登录成功用fd作为下标，存入登陆名单中
    strcpy(lognamel[fd].log1,getname);
    printf("%s\n",lognamel[fd].log1);
    return 0;
}


int user_search(struct user* log,int fd)
{
    printf("user_search\n");
    char sql[612]="";
    printf("%s\n",lognamel[fd].log1);
    sprintf(sql,"select * from staff where name=\'%s\';",
    lognamel[fd].log1);
    if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
    {
        fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
        return -1;
    }
    int ii = 0, jj=0;
    int index = 0;
    printf("row=%d column=%d\n",row,column);
    for(ii=0; ii<(row+1); ii++)
    {
        strcpy(staff_buf.num,result[0+ii*column]);
        strcpy(staff_buf.user_type,result[1+ii*column]);
        strcpy(staff_buf.name,result[2+ii*column]);
        strcpy(staff_buf.password,result[3+ii*column]);
        strcpy(staff_buf.age,result[4+ii*column]);
        strcpy(staff_buf.phone,result[5+ii*column]);
        strcpy(staff_buf.addr,result[6+ii*column]);
        strcpy(staff_buf.position,result[7+ii*column]);
        strcpy(staff_buf.time,result[8+ii*column]);
        strcpy(staff_buf.grade,result[9+ii*column]);
        strcpy(staff_buf.salary,result[10+ii*column]);
        //已经打包完要发送的数据包staff
        printf("按名字查询数据包打包成功\n");
        send(fd,&staff_buf,sizeof(staff_buf),0);
    }
    //释放查询到的结果
    sqlite3_free_table(result);
}

int user_mod(struct user* log,int fd)
{
    printf("user_mod\n");
    char sql[612] = "";
    //选择工号
    sprintf(sql,"select name , password from staff where num='%s' and name='%s';",
    log->mod_num,lognamel[fd].log1);
    printf("sql:%s\n",sql);
    if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
		return -1;
	}
    if(row==0)  //查不到在表中
    {
        sprintf(sed_buf,"no");
        printf("sed_buf=%s\n",sed_buf);
        send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“no”
        printf("回答了no\n");
        return -1;
    }
    sprintf(sed_buf,"ok");
    send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
    printf("回答了ok\n");
    //确认工号存在,开始修改
    recv(fd,&user_buf,sizeof(user_buf),0); //提取mod_sig
    sprintf(sed_buf,"ok");
    send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
    printf("回答了ok\n"); 
    char toevent[612]=""; //发送给数据库的事件
    char mod_item[128]="";  //修改项
    char mod_result[128]=""; //修改结果
    switch(log->mod_sig)
    {
        case 7:  //地址
            sprintf(mod_item,"地址");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set addr=\'%s\' where num=\'%s\';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
        case 6:  //电话
            sprintf(mod_item,"电话");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set phone=\'%s\' where num=\'%s\';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
        case 4:  //密码
            sprintf(mod_item,"密码");
            recv(fd,rev_buf,sizeof(rev_buf),0); //提取要改成的值
            //开始修改
            sprintf(sql,"update staff set password=\'%s\' where num=\'%s\';",
            rev_buf,log->mod_num);
            if(sqlite3_get_table(db, sql, &result, &row, &column, &errmsg) != SQLITE_OK)
            {
                fprintf(stderr, "__%d__ sqlite3_get_table:%s\n", __LINE__, errmsg);
                return -1;
            }
            //成功以后应答ok
            sprintf(sed_buf,"ok");
            send(fd,sed_buf,sizeof(sed_buf),0);   //回答这个“ok”
            printf("回答了ok\n");
            break;
    }
    strcpy(mod_result,rev_buf);  //历史记录中的修改结果
    //获取时间
    char totime[128]=""; //发送给数据库的时间
    time_t buf;
    time(&buf);
    struct tm* info = localtime(&buf);
	sprintf(totime,"%d年%d月%d日 %02d:%02d:%02d ",info->tm_year+1900, info->tm_mon+1, info->tm_mday, \
			info->tm_hour, info->tm_min,info->tm_sec);
    sprintf(toevent,"%s对%s工号,修改了%s为%s\n",lognamel[fd].log1,\
    log->mod_num,mod_item,mod_result);

    //开始添加历史记录
    sprintf(sql,"insert into history values (\'%s\',\'%s\',\'%s\'\
    );",totime,lognamel[fd].log1,toevent);
    if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		fprintf(stderr, "__%d__ sqlite3_exec:%s\n", __LINE__, errmsg);
		return -1;
	}
	printf("历史记录添加成功\n");
}

int user_exit(struct user* log,int fd)
{
    bzero(lognamel[fd].log1,sizeof(lognamel[fd].log1));
    return 0;
}