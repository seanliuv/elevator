/********************************************************************************
 * Author:		liuxg.
 * Date:		2017.3.1
 * Description:	a realtime elevator scheduling algorithm.
 ********************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>

#include "bsem.h"
#include "xmlparser.h"

using std::vector;
//--------------------------------------------------------------------------------
//defines.
#define USLEEP(n)		do\
						{\
							usleep(n * 10000);\
						}while(0)

#define SLEEP(n, clock)	do\
						{\
							usleep(10000);\
							(clock)++;\
						}while((clock) < (n))

#define LOG_SIZE		128	
//--------------------------------------------------------------------------------
//structure defines.
typedef enum en_status
{
	MOVING_DOWN = 0,	//moving down.
	MOVING_UP,			//moving up.
	STOPPED,			//stopped.
}status_m;

typedef struct st_elevator
{
	//elevator's status.
	status_m		cur_status;		//current status.
	unsigned int	cur_pos;		//current position.
	int				trigger;		//trigger to control elevator.
	unsigned int	dest;			//record passenger's destination floor.
	//elevator's property.
	int				c_o_time;		//door open or close time.
}elevator_t;

typedef enum en_direct
{
	DIRECT_DOWN = 0,
	DIRECT_UP,
}direct_m;

typedef struct st_command
{
	unsigned int	pos;	//passenger's position.
	unsigned int	dir;	//up or down.
	unsigned int	dst;	//passenger's destination.
}command_t;

//realtime command.
typedef struct st_rt_command
{
	unsigned int	tms;	//time stamp.
	command_t		cmd;	//passenger's command.
}rt_command_t;

typedef std::vector<command_t> command_vector_t;

typedef struct st_floor
{
	command_vector_t	request[2];
	pthread_mutex_t		mtx;
}floor_t;

typedef struct st_building
{
	unsigned int	totals;		//total floors.
	unsigned int	request;	//requested passenger's cur floor, tag position.
	pthread_mutex_t	mtx;
	bsem_t *		sem;		//signal to notify elevator.
	floor_t *		p_floors;
}building_t;

//--------------------------------------------------------------------------------
//global variables.
static unsigned int g_clock_time = 0;
static unsigned int g_passenger_time = 0;
static building_t * g_building = NULL;

static int exit_code = 0;
static int * p_exit_code = NULL;
//--------------------------------------------------------------------------------
//function declares.
building_t * create_building(const char * cfgfile);
void destroy_building(building_t * build);

elevator_t * create_elevator(const char * cfgfile);
void destroy_elevator(elevator_t * elvt);

pthread_t running_elevator(elevator_t * elvt);

int read_command_file(const char * file);
bool is_invalid_cmd(rt_command_t * rt_cmd);
void send_request(command_t * cmd);

//--------------------------------------------------------------------------------
int main(int argc, char * argv[])
{
	//check parameters.
	if(argc < 3)
	{
		printf("usage: %s -f cmd.txt\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	//@TODO: should parse parameter strictly.

	//init elevator and building from config file.
	unsigned int finished = 1;
	g_building = create_building("./elevator.xml");
	elevator_t * elvt = create_elevator("./elevator.xml");
	
	//create and running elevator schedule thread.
	pthread_t pid_elvt = running_elevator(elvt);

	//read commands from file, parse it, send to elevator at proper time.
	sleep(1);
	read_command_file(argv[2]);

	//waitting for elevator finish all tasks.
	while(finished)
	{
		finished = g_building->request | elvt->dest;
		USLEEP(1);
		g_clock_time++;
	}
	//stop elevator accepting request.
	elvt->trigger = 0;
	pthread_join(pid_elvt, (void**)&p_exit_code);

	//release and exit program.
	destroy_elevator(elvt);
	destroy_building(g_building);
	g_building = NULL;
	
	return 0;
}

int read_command_file(const char * file)
{
	assert(NULL != file);

	FILE * fp = NULL;
	char * line = NULL;
	size_t len = 0;
	ssize_t sz = 0;
	rt_command_t rt_cmd;

	//get intput commands from file.
	fp = fopen(file, "r");
	if(NULL == fp)
	{
		printf("open file: %s error!\n", file);
		exit(EXIT_FAILURE);
	}

	while(-1 != (sz = getline(&line, &len, fp)))
	{
		memset(&rt_cmd, 0, sizeof(rt_command_t));
		sscanf(line, "(%d,%d,%d,%d)", &rt_cmd.tms, 
				&rt_cmd.cmd.pos, &rt_cmd.cmd.dir, &rt_cmd.cmd.dst);
		//check command validity.
		if(is_invalid_cmd(&rt_cmd))
		{
			continue;
		}
		//put command on position floor on proper time, send a signal.
		SLEEP(rt_cmd.tms, g_clock_time);
		send_request(&rt_cmd.cmd);
	}
	fclose(fp);
	return 0;
}

building_t * create_building(const char * cfgfile)
{
	assert(NULL != cfgfile);

	unsigned int i;
	xmlDocPtr doc = NULL;
	xmlNodePtr curNode = NULL;
	xmlNodePtr itemNode = NULL;
	building_t * build = new building_t;

	doc = xmlReadFile(cfgfile, "UTF-8", XML_PARSE_RECOVER);
    if(!doc)
    {
		fprintf(stderr, "read configure file failed!\n");
        exit(EXIT_FAILURE);
    }
    curNode = xmlDocGetRootElement(doc);
    if(!curNode)
    {
        fprintf(stderr, "Empty configure file!\n");
        xmlFreeDoc(doc);
        exit(EXIT_FAILURE);
    }
    if(xmlStrcmp(curNode->name, BAD_CAST "ElevatorConf"))
    {
        fprintf(stderr, "Root node error!\n");
        xmlFreeDoc(doc);
        exit(EXIT_FAILURE);
    }
    itemNode = curNode->xmlChildrenNode;
    while (itemNode)
    {
        if (itemNode->type != XML_ELEMENT_NODE)
        {
            itemNode = itemNode->next;
            continue;
        }
		//get configure value.
        GET_UINT_FROM_XML("building_floors", &build->totals, itemNode);
        GET_UINT_FROM_XML("passenger_time", &g_passenger_time, itemNode);
        itemNode = itemNode->next;
	}
	xmlFreeDoc(doc);
	
	build->request = 0;
	pthread_mutex_init(&build->mtx, NULL);
	build->sem = bsem_create(0);
	build->p_floors = new floor_t[build->totals + 1];
	for(i = 0; i < build->totals + 1; i++)
	{
		pthread_mutex_init(&build->p_floors[i].mtx, NULL);
	}
	return build;
}

elevator_t * create_elevator(const char * cfgfile)
{
	assert(NULL != cfgfile);

	xmlDocPtr doc = NULL;
	xmlNodePtr curNode = NULL;
	xmlNodePtr itemNode = NULL;
	elevator_t * elvt = new elevator_t;

	doc = xmlReadFile(cfgfile, "UTF-8", XML_PARSE_RECOVER);
    if(!doc)
    {
		fprintf(stderr, "Read configure file failed!\n");
        exit(EXIT_FAILURE);
    }
    curNode = xmlDocGetRootElement(doc);
    if(!curNode)
    {
        fprintf(stderr, "Empty configure file!\n");
        xmlFreeDoc(doc);
        exit(EXIT_FAILURE);
    }
    if(xmlStrcmp(curNode->name, BAD_CAST "ElevatorConf"))
    {
        fprintf(stderr, "Root node error!\n");
        xmlFreeDoc(doc);
        exit(EXIT_FAILURE);
    }
    itemNode = curNode->xmlChildrenNode;
    while (itemNode)
    {
        if (itemNode->type != XML_ELEMENT_NODE)
        {
            itemNode = itemNode->next;
            continue;
        }
        GET_UINT_FROM_XML("init_floor", &elvt->cur_pos, itemNode);
        GET_INT_FROM_XML("open_close_time", &elvt->c_o_time, itemNode);
        itemNode = itemNode->next;
	}
	xmlFreeDoc(doc);

	elvt->cur_status = STOPPED;
	elvt->trigger = 1;
	elvt->dest = 0;
	return elvt;
}

void destroy_elevator(elevator_t * elvt)
{
	if(NULL != elvt)
	{
		delete elvt;
	}
}

void destroy_building(building_t * build)
{
	unsigned int i;
	if(NULL != build)
	{
		pthread_mutex_destroy(&build->mtx);
		bsem_destroy(build->sem);
		for(i = 0; i < build->totals; i++)
		{
			pthread_mutex_destroy(&build->p_floors[i].mtx);
		}
		delete [] build->p_floors;
		delete build;
	}
}

//@TODO: function to be tested.
static unsigned int fls(unsigned int x)
{
	int r = 32;
	if (!x){
		return 0;
	}
	if (!(x & 0xffff0000u))
	{
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u))
	{
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u))
	{
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u))
	{
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u))
	{
		x <<= 1;
		r -= 1;
	}
	return (r-1);
}

static void moving_up(int status, elevator_t * elvt)
{
	printf("[%u]: [moving up, elevator at %u floor.]\n", g_clock_time, elvt->cur_pos++);
	elvt->cur_status = MOVING_UP;
	USLEEP(1);
}

static void moving_down(int status, elevator_t * elvt)
{
	printf("[%u]: [moving down, elevator at %u floor.]\n", g_clock_time, elvt->cur_pos--);
	elvt->cur_status = MOVING_DOWN;
	USLEEP(1);
}

static void open_door(elevator_t * elvt)
{
	printf("[%u]: [open door at %u floor.]\n", g_clock_time, elvt->cur_pos);
	USLEEP(elvt->c_o_time);
}

static void passenger_in_out(elevator_t * elvt)
{
	unsigned int i;
	command_t cmd;

	building_t * build = g_building;
	unsigned int mask = ~(1 << elvt->cur_pos);
	printf("[%u]: [passenger in or out.]\n", g_clock_time);
	//passenger get out if destination here.
	elvt->dest &= mask;

	//passenger get in if position here.
	pthread_mutex_lock(&build->p_floors[elvt->cur_pos].mtx);
	for(i = 0;i < build->p_floors[elvt->cur_pos].request[elvt->cur_status].size();i++)
	{
		cmd = build->p_floors[elvt->cur_pos].request[elvt->cur_status].at(i);
		elvt->dest |= (1 << cmd.dst);
	}
	build->p_floors[elvt->cur_pos].request[elvt->cur_status].clear();
	pthread_mutex_unlock(&build->p_floors[elvt->cur_pos].mtx);
	pthread_mutex_lock(&build->mtx);
	build->request &= mask;
	pthread_mutex_unlock(&build->mtx);

	USLEEP(g_passenger_time);
}

static void close_door(elevator_t * elvt)
{
	printf("[%u]: [close door.]\n", g_clock_time);
	USLEEP(elvt->c_o_time);
}

static void stop(elevator_t * elvt)
{
	printf("[%u]: [elevator stopped.]\n", g_clock_time);
	elvt->cur_status = STOPPED;
}

//@function: FD_LOOK scheduling algorithm.
static void * scheduling(void * arg)
{
	building_t * build = g_building;
	elevator_t * elvt = (elevator_t *)(arg);
	unsigned int floor = 0;

	printf("[%u]: [init: elevator at %u floor.]\n", g_clock_time, elvt->cur_pos);

	while(1 == elvt->trigger || elvt->dest != 0)
	{
		if(STOPPED == elvt->cur_status)
		{
			//wait for building's signal.
			bsem_wait(build->sem);
			pthread_mutex_lock(&build->mtx);
			floor = fls(build->request);
			pthread_mutex_unlock(&build->mtx);
			//printf("%u floor request elevator now...\n", floor);
			if(floor > elvt->cur_pos)
			{
				moving_up(0, elvt);
			}
			else if(floor < elvt->cur_pos)
			{
				moving_down(0, elvt);
			}
			else
			{
				open_door(elvt);
				passenger_in_out(elvt);
				close_door(elvt);
				(elvt->dest > (unsigned int)(1 << elvt->cur_pos)) ? (moving_up(0, elvt)) : (moving_down(0, elvt));
			}
		}
		else if(MOVING_UP == elvt->cur_status)
		{
			if(elvt->cur_pos < fls(build->request) || elvt->cur_pos < fls(elvt->dest))
			{
				moving_up(1, elvt);
			}
			else
			{
				elvt->cur_status = MOVING_DOWN;
			}
		}
		else
		{
			if((((1 << elvt->cur_pos)-1) & (build->request)) != 0 || (((1 << elvt->cur_pos) - 1) & (elvt->dest)) != 0)
			{
				moving_down(1, elvt);
			}
			else
			{
				elvt->cur_status = MOVING_UP;
			}
		}
		if((!build->p_floors[elvt->cur_pos].request[elvt->cur_status].empty()) || (elvt->dest & (1 << elvt->cur_pos)))
		{
			open_door(elvt);
			passenger_in_out(elvt);
			close_door(elvt);
		}
		if( 0 == elvt->dest && build->request == 0)
		{
			stop(elvt);
		}
	}
	pthread_exit(&exit_code);
	return NULL;
}

pthread_t running_elevator(elevator_t * elvt)
{
	assert(NULL != elvt);
	pthread_t pid;
	pthread_create(&pid, NULL, scheduling, (void*)elvt);
	return pid;
}

bool is_invalid_cmd(rt_command_t * rt_cmd)
{
	assert(NULL != rt_cmd);

	//@Notes: 1.timestamp invalid;
	//@Notes: 2.direction invalid;
	//@Notes: 3.destination invalid;
	//@Notes: 4.current position equals destination, invalid;
	//@Notes: 5.top floor, and direction up.
	//@Notes: 6.ground floor, and direction down.
	if((rt_cmd->tms < g_clock_time)
			|| (rt_cmd->cmd.pos > g_building->totals)
			|| ((rt_cmd->cmd.dir != DIRECT_DOWN) && (rt_cmd->cmd.dir != DIRECT_UP))
			|| (rt_cmd->cmd.dst > g_building->totals)
			|| (rt_cmd->cmd.pos == rt_cmd->cmd.dst)
			|| (rt_cmd->cmd.pos == g_building->totals && rt_cmd->cmd.dir == DIRECT_UP)
			|| (rt_cmd->cmd.pos == 0 && rt_cmd->cmd.dir == DIRECT_DOWN))
	{
		return true;
	}
	return false;
}

//@function: push command into proper floor, then send signal.
void send_request(command_t * cmd)
{
	building_t * build = g_building;
	pthread_mutex_lock(&build->p_floors[cmd->pos].mtx);
	build->p_floors[cmd->pos].request[cmd->dir].push_back(*cmd);
	pthread_mutex_lock(&build->mtx);
	build->request |= (1 << cmd->pos);
	pthread_mutex_unlock(&build->mtx);
	pthread_mutex_unlock(&build->p_floors[cmd->pos].mtx);
	bsem_post(build->sem);
	printf("<%u>: command: (%u,%u,%u) has been sent!\n", g_clock_time, cmd->pos, cmd->dir, cmd->dst);
}

