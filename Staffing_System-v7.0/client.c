#include <stdio.h>
#include <sys/types.h>         
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <strings.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>



#define IP    "192.168.250.100"
#define PORT  8888

int sfd;  //套接字
struct sockaddr_in cins; //connect要使用的
int res; //
char rev_buf[128]="";   //接收的buf
char sed_buf[128]="";   //发送的buf
int choose;  //进入菜单的选择
char no[10]="no";
char ok[10]="ok";
int flag_2=0; //二级菜单退出标志位
int flag_3=0; //三级菜单退出标志位
int flag_root_2; //root模式的二级菜单退出标志位

/*
    sig=0;  //普通用户登录
    sig=1;  //普通用户查询
    sig=2;  //普通用户修改模式
        *mod_sig=4; //密码
        *mod_sig=6; //电话	
        *mod_sig=7; //地址	

    sig=10;  //超级用户登录
    sig=11;  //超级用户查询模式
        *search_sig=0; 按名字查
        *search_sig=1; 全部查
    sig=12;  //超级用户修改模式
        *mod_sig=1; //工号
        *mod_sig=2; //用户类型
        *mod_sig=3; //姓名
        *mod_sig=4; //密码
        *mod_sig=5; //年龄	
        *mod_sig=6; //电话	
        *mod_sig=7; //地址	
        *mod_sig=8; //职位	
        *mod_sig=9; //入职年月	
        *mod_sig=10; //等级	 
        *mod_sig=11; //工资
    sig=13;  //超级用户添加
    sig=14;  //超级用户查询历史记录
*/

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

struct history
{
    char time[128];
    char user[128];
    char event[128];
};

int user_search(struct user* sch1);
int user_mod(struct user* mod1);
int root_search(struct user* sch1);
int root_mod(struct user* mod1);
int root_insert(struct user* mod1);
int root_del(struct user* del1);
int root_history(struct user* his1);
int root_exit(struct user* exit1);
int user_exit(struct user* exit1);

int main(int argc, char const *argv[])
{
    //1.1.socket
    sfd=socket(AF_INET,SOCK_STREAM,0);
    if(sfd<0)
    {
        perror("socket error");
        return -1;
    }
    //1.2.connect
    cins.sin_family=AF_INET;
    cins.sin_port=htons(PORT);
    cins.sin_addr.s_addr=inet_addr(IP);
    res=connect(sfd,( struct sockaddr *)&cins,sizeof(cins));
    if(res<0)
    {
        perror("connect error");
        return -1;
    }
    while(1)
    {
        //一级菜单
        printf("****************************\n");
        printf("*******1.root用户登录********\n");
        printf("*******2.普通用户登录********\n");
        printf("****************************\n");
        scanf("%d",&choose);
        //
        if(choose==1) //root用户登录
        {
            //1.输入用户名和密码
            struct user us2;
            printf("请输入用户名：");
            scanf("%s",us2.usname);
            getchar();
            printf("请输入密码：");
            scanf("%s",us2.password);
            getchar();
            printf("\n");
            us2.sig=10; //表示进入超级用户登录模式
            send(sfd,&us2,sizeof(us2),0); //发送用户名和密码给服务器判断
            printf("发送出去了\n");
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("账户或者密码错误\n");
                continue; //回到一级菜单
            }
            printf("%s\n",rev_buf);   //成功登录，打印服务器返回的字符串

            //进入二级菜单
            while(1)
            {
                printf("*********************\n");
                printf("*******1.查询********\n");
                printf("*******2.修改********\n");
                printf("*******3.添加********\n");
                printf("*******4.删除********\n");
                printf("*******5.查询历史记录*\n");
                printf("*******6.退出********\n");
                printf("*********************\n");
                scanf("%d",&choose);
                switch(choose)
                {
                    case 1:
                        root_search(&us2);
                        break;
                    case 2:
                        root_mod(&us2);
                        break;
                    case 3:
                        root_insert(&us2);
                        break;
                    case 4:
                        root_del(&us2);
                        break;
                    case 5:
                        root_history(&us2);
                        break;
                    case 6:
                        flag_root_2=1;
                        root_exit(&us2);
                        break;
                }
                if(flag_root_2==1)
                {
                    flag_root_2=0;
                    break; //退回一级菜单
                }
            }
        }
        if(choose==2) //普通用户登录
        {
            //1.输入用户名和密码
            struct user us1;
            printf("请输入用户名：");
            scanf("%s",us1.usname);
            getchar();
            printf("请输入密码：");
            scanf("%s",us1.password);
            getchar();
            printf("\n");
            us1.sig=0; //表示进入普通用户登录模式
            send(sfd,&us1,sizeof(us1),0); //发送用户名和密码给服务器判断
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("账户或者密码错误\n");
                continue; //回到一级菜单
            }
            printf("%s\n",rev_buf);   //成功登录，打印服务器返回的字符串
           
            //进入二级菜单
            while(1)
            {
                printf("*********************\n");
                printf("*******1.查询********\n");
                printf("*******2.修改********\n");
                printf("*******3.退出********\n");
                printf("*********************\n");
                scanf("%d",&choose);
                switch(choose)
                {
                    case 1:
                        user_search(&us1); //进入查询处理函数
                        break;
                    case 2:
                        user_mod(&us1); //进入修改处理函数,三级菜单
                        user_search(&us1); //修改完打印一遍
                        break;
                    case 3:
                        user_exit(&us1);
                        flag_2=1;
                        break;
                }
                if(flag_2==1) //二级菜单退出标志位为1，退出二级回到一级
                {
                    flag_2=0; //清除标志位
                    break;
                }
                
            }
        }
    }
    
    return 0;
}

int root_search(struct user* sch1)
{
    struct staff rev_staff;
    bzero(rev_buf,sizeof(rev_buf));
    sch1->sig=11;  //表示服务器进入超级用户查询模式
    

    printf("************************\n");
    printf("*******1.按名字*********\n");
    printf("*******2.全部***********\n");
    printf("*******3.退出***********\n");
    printf("************************\n");
    scanf("%d",&choose);
    switch(choose)
    {
        case 1:
            printf("请输入要查询的名字:");
            scanf("%s",sch1->search_name);
            getchar();
            sch1->search_sig=0; //告诉服务器是按名字来查找
            printf("sig=%d\n",sch1->sig);
            send(sfd,sch1,sizeof(struct user),0); //发送修改请求
            printf("发送查询名字成功\n");
            recv(sfd,&rev_staff,sizeof(rev_staff),0);
            printf("%s %s %s %s %s %s %s %s %s %s %s\n",\
            rev_staff.num,rev_staff.user_type,rev_staff.name,\
            rev_staff.password,rev_staff.age,rev_staff.phone,\
            rev_staff.addr,rev_staff.position,rev_staff.time,\
            rev_staff.grade,rev_staff.salary); //打印查询结果
            recv(sfd,&rev_staff,sizeof(rev_staff),0);
            printf("%s %s %s %s %s %s %s %s %s %s %s\n",\
            rev_staff.num,rev_staff.user_type,rev_staff.name,\
            rev_staff.password,rev_staff.age,rev_staff.phone,\
            rev_staff.addr,rev_staff.position,rev_staff.time,\
            rev_staff.grade,rev_staff.salary); //打印查询结果
            break;
        case 2:
            sch1->search_sig=1; //告诉服务器是全部查找
            send(sfd,sch1,sizeof(struct user),0); //发送修改请求
            while(1)
            {
                res=recv(sfd,&rev_staff,sizeof(rev_staff),0); //接收查询结果
                if(!strcmp(rev_staff.num,"nack")) //暂时这样判断是否读完表？？
                {
                    printf("读完表\n"); 
                    break;
                }
                printf("%s %s %s %s %s %s %s %s %s %s %s\n",\
                rev_staff.num,rev_staff.user_type,rev_staff.name,\
                rev_staff.password,rev_staff.age,rev_staff.phone,\
                rev_staff.addr,rev_staff.position,rev_staff.time,\
                rev_staff.grade,rev_staff.salary); //打印查询结果
            }
            break;
        case 3:
            break;
    }

    
    return 0;
}

int root_mod(struct user* mod1)
{
    mod1->sig=12;  //表示服务器进入超级用户修改模式
    printf("请输入您要修改只认的工号：");
    scanf("%s",mod1->mod_num);
    getchar();
    send(sfd,mod1,sizeof(struct user),0); //发送修改请求
    recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
    if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
    {
        printf("工号错误\n");
        return -1; //回到一级菜单
    }
    //成功就进入三级菜单
    printf("******************************************************\n");
    printf("*******1.姓名	 2.年龄	  3.家庭住址   4.电话***********\n");
    printf("*******5.职位	 6.工资   7.入职年月   8.评级***********\n");
    printf("*******9.密码	 10.退出*******************************\n");
    printf("******************************************************\n");
    scanf("%d",&choose);
    switch(choose)
    {
        case 1:
            mod1->mod_sig=3; //表示要修改的是姓名
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入姓名：");
            scanf("%s",sed_buf); //要修改为的姓名
            send(sfd,&sed_buf,sizeof(sed_buf),0); //发送姓名
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 2:
            mod1->mod_sig=5; //表示要修改的是年龄
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入年龄：");
            scanf("%s",sed_buf); //要修改为的年龄
            send(sfd,&sed_buf,sizeof(sed_buf),0); //发送年龄
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 3:
            mod1->mod_sig=7; //表示要修改的是地址
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入地址：");
            scanf("%s",sed_buf); //要修改为的地址
            send(sfd,&sed_buf,sizeof(sed_buf),0); //发送地址
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 4:
            mod1->mod_sig=6; //表示要修改的是电话
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入电话：");
            scanf("%s",sed_buf); //要修改为的电话
            send(sfd,&sed_buf,sizeof(sed_buf),0); //发送电话
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 5:
            mod1->mod_sig=8; //表示要修改的是职位
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入职位：");
            scanf("%s",sed_buf); //要修改为的职位
            send(sfd,&sed_buf,sizeof(sed_buf),0); //发送职位
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 6:
            mod1->mod_sig=11; //表示要修改的是工资
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入工资：");
            scanf("%s",sed_buf); //要修改为的工资
            send(sfd,&sed_buf,sizeof(sed_buf),0); //发送工资
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 7:
            mod1->mod_sig=9; //表示要修改的是入职年月
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入入职年月：");
            scanf("%s",sed_buf); //要修改为的入职年月
            send(sfd,&sed_buf,sizeof(sed_buf),0); //发送入职年月
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 8:
            mod1->mod_sig=10; //表示要修改的是等级
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入等级：");
            scanf("%s",sed_buf); //要修改为的等级
            send(sfd,&sed_buf,sizeof(sed_buf),0); //发送等级
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 9:
            mod1->mod_sig=4; //表示要修改的是密码
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入密码：");
            scanf("%s",sed_buf); //要修改为的密码
            send(sfd,&sed_buf,sizeof(sed_buf),0); //发送密码
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 10:
            break;
    }
}

int root_insert(struct user* mod1)
{
    mod1->sig=13;  //表示服务器进入超级用户添加模式
    send(sfd,mod1,sizeof(struct user),0); //发送修改请求
    //初始化staff结构体储存发送的员工信息
    struct staff us;
    printf("请输入工号：");
    scanf("%s",us.num);
    getchar();
    printf("\n请输入用户类型:");
    scanf("%s",us.user_type);
    getchar();
    printf("\n请输入姓名:");
    scanf("%s",us.name);
    getchar();
    printf("\n请输入密码:");
    scanf("%s",us.password);
    getchar();
    printf("\n请输入年龄:");
    scanf("%s",us.age);
    getchar();
    printf("\n请输入电话:");
    scanf("%s",us.phone);
    getchar();
    printf("\n请输入地址:");
    scanf("%s",us.addr);
    getchar();
    printf("\n请输入职位:");
    scanf("%s",us.position);
    getchar();
    printf("\n请输入入职年月:");
    scanf("%s",us.time);
    getchar();
    printf("\n请输入等级:");
    scanf("%s",us.grade);
    getchar();
    printf("\n请输入工资:");
    scanf("%s",us.salary);
    getchar();
    printf("\n");
    //发送成员信息
    send(sfd,&us,sizeof(us),0); //发送成员信息
    recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
    if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
    {
        printf("添加没有成功\n");
        return -1; //回到一级菜单
    }
    printf("添加成功\n");
    return 0;
}

int root_del(struct user* del1)
{
    del1->sig=14;  //表示服务器进入超级用户删除模式
    printf("要删除的工号:");
    scanf("%s",del1->mod_num);
    getchar();
    printf("\n要删除的名字:");
    scanf("%s",del1->usname);
    send(sfd,del1,sizeof(struct user),0); //发送删除的成员信息
    recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
    if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
    {
        printf("删除没有成功\n");
        return -1; //回到一级菜单
    }
    printf("删除成功\n");
    return 0;
}

int root_history(struct user* his1)
{
    //
    his1->sig=15;  //表示服务器进入超级用户删除模式
    send(sfd,his1,sizeof(struct user),0); //发送删除的成员信息
    struct history his;

    while(1)
    {
        res=recv(sfd,&his,sizeof(his),0); //接收查询结果
        if(!strcmp(his.time,"nack")) //暂时这样判断是否读完表？？
        {
            printf("读完表\n"); 
            break;
        }
        printf("%s %s %s \n",\
        his.time,his.user,his.event); //打印查询结果
    }
    return 0;
}

int root_exit(struct user* exit1)
{
    exit1->sig=16;  //表示服务器进入超级用户删除模式
    send(sfd,exit1,sizeof(struct user),0); 
}

/************用户函数*******************/
int user_search(struct user* sch1)
{
    struct staff rev_staff;
    bzero(rev_buf,sizeof(rev_buf));
    sch1->sig=1;  //表示服务器进入普通用户查询模式
    send(sfd,sch1,sizeof(struct user),0); //发送查询请求
    recv(sfd,&rev_staff,sizeof(rev_staff),0); //接收查询结果
    printf("%s %s %s %s %s %s %s %s %s %s %s\n",\
    rev_staff.num,rev_staff.user_type,rev_staff.name,\
    rev_staff.password,rev_staff.age,rev_staff.phone,\
    rev_staff.addr,rev_staff.position,rev_staff.time,\
    rev_staff.grade,rev_staff.salary); //打印查询结果
    recv(sfd,&rev_staff,sizeof(rev_staff),0); //接收查询结果
    printf("%s %s %s %s %s %s %s %s %s %s %s\n",\
    rev_staff.num,rev_staff.user_type,rev_staff.name,\
    rev_staff.password,rev_staff.age,rev_staff.phone,\
    rev_staff.addr,rev_staff.position,rev_staff.time,\
    rev_staff.grade,rev_staff.salary); //打印查询结果
    return 0;
}

int user_mod(struct user* mod1)
{
    
    mod1->sig=2;  //表示服务器进入普通用户修改模式
    printf("请输入您要修改只认的工号：");
    scanf("%s",mod1->mod_num);
    getchar();
    send(sfd,mod1,sizeof(struct user),0); //发送修改请求
    recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
    if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
    {
        printf("工号错误\n");
        return -1; //回到一级菜单
    }
    //成功就进入三级菜单
    printf("************************\n");
    printf("*******1.家庭住址********\n");
    printf("*******2.电话***********\n");
    printf("*******3.密码***********\n");
    printf("*******4.退出***********\n");
    printf("************************\n");
    scanf("%d",&choose);
    switch(choose)
    {
        case 1:
            mod1->mod_sig=7; //表示要修改的是住址
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入家庭住址：");
            scanf("%s",sed_buf); //要修改为的地址
            send(sfd,sed_buf,sizeof(sed_buf),0); //发送地址
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 2:
            mod1->mod_sig=6; //表示要修改的是电话
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入电话：");
            scanf("%s",sed_buf); //要修改为的地址
            send(sfd,&sed_buf,sizeof(sed_buf),0); //发送地址
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 3:
            mod1->mod_sig=4; //表示要修改的是密码
            send(sfd,mod1,sizeof(struct user),0); //发送修改请求
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器的判断结果 
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改信号没有接收到\n");
                return -1; //回到一级菜单
            }
            printf("请输入密码：");
            scanf("%s",sed_buf); //要修改为的地址
            send(sfd,&sed_buf,sizeof(sed_buf),0); //发送地址
            recv(sfd,rev_buf,sizeof(rev_buf),0); //接收服务器应答
            if(!strcmp(rev_buf,no)) //如果时返回no，就是登录失败
            {
                printf("修改没有成功\n");
                return -1; //回到一级菜单
            }
            printf("修改成功\n");

            break;
        case 4:
            flag_3=1;
            break;
    }
}

int user_exit(struct user* exit1)
{
    exit1->sig=3;  //表示服务器进入超级用户删除模式
    send(sfd,exit1,sizeof(struct user),0); 
}