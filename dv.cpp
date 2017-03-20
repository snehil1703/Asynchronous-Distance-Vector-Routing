//
//  Mine12.cpp
//  Project 4
//
//  Created by Snehil Vishwakarma on 12/6/15.
//  Copyright © 2015 Indiana University Bloomington. All rights reserved.
//

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>
#include <pthread.h>

int countr,val,position;
bool check;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

std::vector<struct sockaddr_in> sadd_send;
//std::vector<struct sockaddr_in> cadd_send;
std::vector<int> sfd_send;
struct sockaddr_in sadd_recv;
int sfd_recv;
std::vector<struct hostent*> hp;
std::vector<char> neighbours;

struct Field
{
    char filename[100];  //File name is ALWAYS config.txt
    int portno;  //Integral value of port number
    unsigned short ttl;  //Metric is SECONDS. Default ttl 90 seconds
    int maxhop; //Infinity (look above in comments)
    long period; //Periodic Update. Metric in SECONDS. Default period update is 30 seconds
    bool split; //Split Horizon. I think so it should be boolean
    Field()
    {
        ttl=90;
        maxhop=16;
        period=30;
    }
}parameters;

struct graph
{
    char node_r,node_c;
    int weight;
};
std::vector<graph> v1d;
std::vector<std::vector<graph> > v2d;

struct routing
{
    char dest_node,next_node;
    int cost;
    unsigned short ttl;
};

std::vector<routing> rt;
std::vector<routing> rt_temp;
std::vector<routing> rt_recv;

std::vector<char> s1h;
std::vector<std::vector<char> > s2h;

int counter_recv;

void initialize();
void bellman_ford();
void update(int);
void send_advertisement(char*,int);
void *periodic_updates(void *);
void *triggered_updates(void *);
void get_ip();

int main(int argc, char* argv[])
{
    if (argc != 7)       //Check for having exactly 6 command line arguments
    {
        std::cout<<"\nUser Error: Exactly 6 command line argument needed!\n";
        exit(0);
    }
    
    strcpy(parameters.filename,argv[1]);
    parameters.portno=atoi(argv[2]);
    parameters.ttl=(unsigned short)atol(argv[3]);
    parameters.maxhop=atoi(argv[4]);
    parameters.period=atoi(argv[5]);
    if(strcmp(argv[6],"1") || strcmp(argv[6],"true"))
        parameters.split=true;
    else
        parameters.split=false;
    
    initialize();
    
    //periodic updates using thread
    pthread_t pupdates,tupdates;
    const char *msg1 = "Periodic Update Thread";
    const char *msg2 = "Triggered Update Thread";
    int ret1,ret2;
    
    ret1 = pthread_create( &pupdates, NULL, periodic_updates, (void *)msg1);
    if(ret1)
    {
        exit(EXIT_FAILURE);
    }
    
    ret2 = pthread_create( &tupdates, NULL, triggered_updates, (void *)msg2);
    if(ret2)
    {
        exit(EXIT_FAILURE);
    }
    
    pthread_join(pupdates,NULL);
    pthread_join(tupdates,NULL);
    return 0;
}

void update()
{
    int i,j,k;
    long int n;
    socklen_t len;
    
    char mem[1024];
    
    std::cout<<"\n\n Waiting for a Routing Table ...";
    
    struct sockaddr_in cadd_recv;
    
    len = sizeof(cadd_recv);
    bzero(mem,1024);
    n = recvfrom(sfd_recv,mem,1024,0,(struct sockaddr *)&cadd_recv,&len); //Receiving File Name required
    if (n < 0)
    {
        std::cout<<"\nInternal Error: Reading from socket!";
    }
    
    else
    {
        std::cout<<"\n\n Received a Routing Table : ";
        
        i=0;
        k=0;
        while(mem[k]!=';')
            k++;
        
        counter_recv=(int)mem[i]-48;
        i++;
        while(i<k)
        {
            counter_recv=(((int)mem[i]-48)+(counter_recv*10));
            i++;
        }
        k++;
        i=k;
        rt_recv.resize(counter_recv);
        for(j=0;j<counter_recv;j++)
        {
            rt_recv[j].dest_node=(char)mem[k];
            rt_recv[j].next_node=(char)mem[k+2];
            k=k+4;
            i=k;
            while(mem[k]!=' ')
                k++;
            rt_recv[j].cost=(int)mem[i]-48;
            i++;
            while(i<k)
            {
                rt_recv[j].cost=(((int)mem[i]-48)+(rt_recv[j].cost*10));
                i++;
            }
            k++;
            i=k;
            while(mem[k]!=';')
                k++;
            rt_recv[j].ttl=(int)mem[i]-48;
            i++;
            while(i<k)
            {
                rt_recv[j].ttl=(((int)mem[i]-48)+(rt_recv[j].ttl*10));
                i++;
            }
            k++;
            i=k;
        }
        std::cout<<"\n";
        for(j=0;j<counter_recv;j++)
            std::cout<< " " << rt_recv[j].dest_node<<" "<<rt_recv[j].next_node<<" "<<rt_recv[j].cost<<" "<<rt_recv[j].ttl<<"\n";
        
        bellman_ford();   //REMOVE COMMENTS FROM HERE
    }
}

void *periodic_updates(void *threadnm) //periodic advertisements
{
    int j,k,l;
    bool ns;
    while(1)
    {
        //this_thread::sleep_for (std::chrono::seconds(1));
        
        sleep((unsigned int)parameters.period);
        
        //reduce ttl by period
        for(j=0;j<rt.size();j++)
        {
            ns=false;
            for(k=0;k<position;k++)
            {
                if(rt[j].dest_node == neighbours[k])
                {
                    ns=true;
                    break;
                }
            }
            if(ns==true)
            {
                if(rt[j].ttl <= 0)
                {
                    rt[j].cost=parameters.maxhop;
                    rt[j].next_node='!';
                    for(l=0;l<rt.size();l++)
                    {
                        if(rt[l].next_node==rt[j].dest_node)
                        {
                            rt[l].next_node='!';
                            rt[l].cost=parameters.maxhop;
                        }
                    }
                }
                else
                    rt[j].ttl=rt[j].ttl - parameters.period;
            }
        }
        
        get_ip();
        
        //set timer for periodic advertisement of rt (10 million = 1 sec)
        std::cout<<"\n";
    }
}

void *triggered_updates(void *threadnm) //triggered advertisements
{
    int j,k,l;
    bool ns;
    while(1)
    {
        check=false;
        
        //In UPDATE, Call BELLMAN FORD and SET CHECK: TRUE if there is a change in Routing Table
        update();
        
        if(check) //if any updates then send triggered adv
        {
            for(j=0;j<rt.size();j++)
            {
                ns=false;
                for(k=0;k<position;k++)
                {
                    if(rt[j].dest_node == neighbours[k])
                    {
                        ns=true;
                        break;
                    }
                }
                if(ns==true)
                {
                    if(rt[j].ttl <= 0)
                    {
                        //rt[j].cost=parameters.maxhop;
                        //rt[j].next_node='!';
                        for(l=0;l<rt.size();l++)
                        {
                            if(rt[l].next_node==rt[j].dest_node)
                            {
                                rt[l].next_node='!';
                                rt[l].cost=parameters.maxhop;
                            }
                        }
                    }
                    else
                        rt[j].ttl=rt[j].ttl - parameters.period;
                }
            }
            get_ip();
            check=false;
        }
    }
}

void send_advertisement(char* ipadd,int pos)
{
    int i,j;
    long int n;
    socklen_t len;
    bool sh;
    
    char mem[1024];
    
    len=sizeof(sadd_send[pos]);
    
    std::ostringstream convert;
    convert << countr<<";";
    
    if(parameters.split)
    {
        for(i=0;i<countr;i++)
        {
            sh=false;
            for(j=0;j<s2h[pos].size();j++)
            {
                if(rt[i].dest_node == s2h[pos][j])
                {
                    sh=true;
                    break;
                }
            }
            if(sh==true)
                convert <<rt[i].dest_node<<" "<<"!"<<" "<<parameters.maxhop<<" "<<rt[i].ttl<<";";
            else
                convert <<rt[i].dest_node<<" "<<rt[i].next_node<<" "<<rt[i].cost<<" "<<rt[i].ttl<<";";
        }
        s2h[pos].resize(0);
    }
    else
    {
        for(i=0;i<countr;i++)
        {
            convert <<rt[i].dest_node<<" "<<rt[i].next_node<<" "<<rt[i].cost<<" "<<rt[i].ttl<<";";
        }
    }
    
    strcpy(mem,convert.str().c_str());
    //   std::cout<<"\n mem: "<<mem;
    
    n=sendto(sfd_recv,mem,
             strlen(mem),0,(const struct sockaddr *)&sadd_send[pos],len);     //Sending required file's file_name to the server
    if (n < 0)
    {
        std::cout<<"\n Internal Error: Writing to socket!\n";
        return;
    }
    
    //close(sfd);     //Closing the communicating socket
}

void initialize()
{
    int i,j,k,y;
    
    char temp[100];
    countr=0;
    std::ifstream f(parameters.filename);
    if (f.is_open())
    {
        char ch='a',tempch_r,tempch_c;
        while (f.getline(temp,100,'\n'))
        {
            if(ch==temp[0])
                ch++;
            countr++;
        }
        k=((int)ch)-97;
        val=k;
        countr++;
        f.clear();
        f.seekg(0,std::ios::beg);
        
        v1d.resize(countr);
        v2d.resize(countr,v1d);
        
        tempch_r='a';
        tempch_c='a';
        for(i=0;i<countr;i++,tempch_r++)
        {
            tempch_c='a';
            for(j=0;j<countr;j++,tempch_c++)
            {
                v2d[i][j].node_r=tempch_r;
                v2d[i][j].node_c=tempch_c;
            }
        }
        i=0;
        for(i=0;i<countr;i++)
        {
            if(i==k)
            {
                for(j=0;j<countr;j++)
                {
                    if(j==k)
                        v2d[i][j].weight=0;
                    else
                    {
                        f.getline(temp,100,'\n');
                        for(y=0;y<countr;y++)
                        {
                            if(temp[0]==v2d[i][y].node_c)
                            {
                                if(temp[2]=='y' || temp[2]=='Y')
                                    v2d[i][y].weight=1;
                                else
                                    v2d[i][y].weight=parameters.maxhop;
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                for(j=0;j<countr;j++)
                    v2d[i][j].weight=parameters.maxhop;
            }
        }
        
        f.close();
        rt.resize(countr);
        
        int pos=0;
        char ipadd[100],temp[100];
        std::ifstream f1;
        
        //Update _ SOCKET CREATION
        
        sfd_recv=socket(AF_INET, SOCK_DGRAM, 0);      //Creating DATAGRAM Socket for UDP connection
        if (sfd_recv < 0)
        {
            std::cout<<"\n Internal Error: Socket cannot be opened!\n";
            exit(0);
        }
        
        bzero((char *) &sadd_recv, sizeof(sadd_recv));
        sadd_recv.sin_family=AF_INET;
        sadd_recv.sin_addr.s_addr=INADDR_ANY;
        sadd_recv.sin_port=htons(parameters.portno);
        if (bind(sfd_recv,(struct sockaddr *)&sadd_recv,sizeof(sadd_recv))<0)     //Binding the socket to SERVER Address
        {
            std::cout<<"\n Internal Error: Cannot Bind!\n";
            close(sfd_recv);
            return;
        }
        
        for(j=0;j<countr;j++)
        {
            rt[j].dest_node=v2d[val][j].node_c;
            rt[j].cost=v2d[val][j].weight;
            if(rt[j].cost==1)
            {
                sadd_send.resize(sadd_send.size()+1);
                //cadd_send.resize(cadd_send.size()+1);
                sfd_send.resize(sfd_send.size()+1);
                hp.resize(hp.size()+1);
                neighbours.resize(neighbours.size()+1);
                
                neighbours[pos]=rt[j].dest_node;
                
                f1.open("ip.txt");
                bzero(temp,100);
                do
                {
                    f1.getline(temp,100,'\n');
                }while (temp[0]!=neighbours[pos]);
                f1.close();
                
                bzero(ipadd,100);
                strcpy(ipadd,temp+2);
                
                hp[pos] = gethostbyname(ipadd);        //Getting ther server's name from CLI arguments
                if (hp[pos] == NULL)
                {
                    std::cout<<"\n Internal Error: no such host\n";
                    return;
                }
                
                bzero((char *) &sadd_send[pos], sizeof(sadd_send[pos]));
                sadd_send[pos].sin_family = AF_INET;
                bcopy((char *)hp[pos]->h_addr,
                      (char *)&sadd_send[pos].sin_addr,
                      hp[pos]->h_length);
                sadd_send[pos].sin_port = htons(parameters.portno);
                
                //Position UPDATE
                
                pos++;
            }
            if(rt[j].cost>=parameters.maxhop)
                rt[j].next_node='!';
            else
                rt[j].next_node=v2d[val][j].node_c;
            rt[j].ttl=parameters.ttl;
        }
        position=pos;
        
        s1h.resize(0);
        s2h.resize(countr,s1h);
        
        get_ip();
    }
    else
        std::cout<<"\n Unable to open IP_CONFIG file!";
}

void get_ip()
{
    int i,j;
    char ipadd[100],temp[100];
    std::ifstream f1("ip.txt");
    if (f1.is_open())
    {
        std::cout<<"\n Advertising our own Routing Table : "<<"\n";
        for(j=0;j < countr;j++)
            std::cout << " " << rt[j].dest_node << " " << rt[j].next_node << " " << rt[j].cost << " " << rt[j].ttl << "\n";
        char ch1=(val+97);
        while (f1.getline(temp,100,'\n'))
        {
            if(ch1!=temp[0])
            {
                for(i=0;i<countr;i++)
                {
                    if(rt[i].dest_node==temp[0])
                        break;
                }
                if(rt[i].cost==1)
                {
                    bzero(ipadd,100);
                    strcpy(ipadd,temp+2);
                    std::cout<<"\n\n Advertisement to : "<<ipadd<<"\n";
                    
                    for(j=0;j<neighbours.size();j++)
                    {
                        if(rt[i].dest_node==neighbours[j])
                            break;
                    }
                    send_advertisement(ipadd,j);
                }
            }
        }
    }
    else
        std::cout<<"\n Unable to open IP_ADDRESS file!";
    f1.close();
}

void bellman_ford() //updated
{
    int j,k,p,l;
    char r;
    //std::vector<int> temp;
    char rt_recv_node='#',rt_node;
    
    rt_node=(char)(97+val);
    
    for(k=0;k<countr;k++)
    {
        if(rt_recv[k].cost == 0)
        {
            rt_recv_node = (char)(97+k);
            rt[k].ttl = parameters.ttl;
            p = k;
        }
    }
    //rt_recv_node = (char)(97+val);
    //rt[val].ttl = parameters.ttl;
    
    rt[p].cost=1;
    rt[p].next_node=rt_recv_node;
    rt[p].ttl=parameters.ttl;
    
    for(k=0;k<countr;k++)
    {
        if((rt_recv[k].dest_node != rt_node) && (rt_recv[k].dest_node != rt_recv_node))
        {
            for(l=0;l<position;l++)
            {
                if(neighbours[l]==rt[k].dest_node || neighbours[l]==rt[k].next_node)
                {
                    break;
                }
            }
            if(l<position)
            {
                if(rt[k].cost==parameters.maxhop)
                {
                    rt[k].next_node='!';
                }
            }
            else if(rt[k].cost>(rt[p].cost+rt_recv[k].cost))
            {
                rt[k].cost=rt[p].cost+rt_recv[k].cost;
                rt[k].next_node=rt_recv_node;
                check=true;
                rt[j].ttl=parameters.ttl;
                s2h[p].push_back(rt[k].dest_node);
            }
        }
    }
    
    /*
     for(k=0;k<countr;k++)
     {
     temp.push_back(val);
     while(temp.size()!=0)
     {
     i=temp[0];
     for(j=0;j<countr;j++)
     {
     if(rt_recv[j].dest_node != rt_recv[j].next_node && rt[j].cost > rt_recv[j].cost)
     {
     check = 1;
     rt[j].cost= rt[p].cost + rt_recv[j].cost;
     rt[j].ttl = parameters.ttl;
     rt[j].next_node= rt_recv_node;
     temp.push_back(j);
     }
     }
     temp.erase(temp.begin());
     }
     }
     */
    std::cout<<"\n ** After Bellman **:\n";
    for(j=0;j < countr;j++)
        std::cout << " " << rt[j].dest_node << " " << rt[j].next_node << " " << rt[j].cost << " " << rt[j].ttl << "\n";
}
