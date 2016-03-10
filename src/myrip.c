/*
 * Daniel Farley - dfarley@ucsc.edu
 * CE 156 - Programing Assignment 5
 * Usage: ./myrip <node.config> <neightbor.config> <local_port>
 */

#include "mytimer.h"

#define UPDATE_INTERVAL 10+(rand()%5)
#define DEAD_ROUTE 40
#define MAX_DISTANCE 16

typedef struct {
    uint32_t distance;
    uint32_t next_hop;
    struct sockaddr_in destaddr;
    uint32_t destination;
    time_t last_updated;
    int neighbor;
} node__t;

typedef struct {
    uint8_t command;      //2 = response
    uint8_t version;      //1 = RIP v1
    uint16_t num_entries; 
    uint8_t entries;      
} packet__t;

typedef struct {
    uint8_t family;    //2 = IP
    uint16_t blank1;   //00
    uint32_t addr;     
    uint32_t blank2;   //0000
    uint32_t blank3;   //0000
    uint32_t distance; 
} entry__t;

node__t **topo = NULL;
node__t *this = NULL;
int local_port = 0;
int incfd = 0;

void parse_node_config(char *nodefp);
void parse_neighbor_config(char *neighborfp);
node__t *get_node(char nick);
int sizeof_topo();
void print_node(node__t *node);
void print_topo();
void free_topo();
packet__t *new_packet(int num_entries);
int sizeof_packet(packet__t *pkt);
int is_valid_route(node__t *node);
void create_route_packet(int signo);
void send_routes(packet__t *p_routes);

int main(int argc, char **argv)
{
    int n, len;
    struct sockaddr_in incaddr;
    packet__t *p_recv = new_packet(sizeof_topo());
    
    srand(time(NULL));
    
    if (argc != 4) {
        printf("Usage: %s <node.config> <neightbor.config> <local_port>\n\n", argv[0]);
        exit(1);
    }
    
    local_port = strtoul(argv[3], NULL, 10);
    parse_node_config(argv[1]);
    parse_neighbor_config(argv[2]);
    
    print_topo();
    
    incfd = Socket(AF_INET, SOCK_DGRAM, 0);
    Bind(incfd, (SA *) &(this->destaddr), sizeof(this->destaddr));
    
    
    //Set sigalarm action
    struct sigaction sact = {
        .sa_handler = create_route_packet,
        .sa_flags = SA_RESTART,
    };
    if (sigaction(SIGALRM, &sact, NULL) == -1) {
        err_sys("sigaction error: %s", strerror(errno));
    }
    
    for (;;) {
        len = sizeof(incaddr);
        alarm(UPDATE_INTERVAL);
        n = recvfrom(incfd,
                     p_recv,  //header -  pkt->entries   +        N       *     entries
                     sizeof(packet__t) - sizeof(uint8_t) + (sizeof_topo() * sizeof(entry__t)),
                     0,
                     (SA *) &incaddr,
                     &len);
        if (n < 0) {
            printf("recvfrom() error: %s\n", strerror(errno));
            //error, ignore it?
        } else if (n == 0) {
            printf("recvfrom() error: n == 0\n");
            //"peer has performed an orderly shutdown"
            //What do?
        } else {
            //If the packet isn't from a neighbor then we don't care
            
            printf("got packet with %d entries from %s:%u\n", 
                    p_recv->num_entries, 
                    inet_ntoa(incaddr.sin_addr),
                    ntohs(incaddr.sin_port)
                   );
            
            
        }
        free(p_recv);
        p_recv = new_packet(sizeof_topo());
    }
    
    free_topo();
}

void parse_node_config(char *nodefp)
{
    int num_alloced = 4;
    FILE *fp = fopen(nodefp, "r");
    
    if (!fp) {
        printf("  parse_node_config(): fopen(%s) ERROR.\n\n", nodefp);
        exit(2);
    }
    
    if ((topo = calloc(num_alloced + 1, sizeof(node__t*))) == NULL) {
        err_sys("  parse_node_config(): ERROR allocating memory!\n\n");
    } 
    
    for (int i = 0; 1; i++) {
        char buffer[100],
             ipaddr[20];
        uint32_t nick;
        int port;
        
        if (i >= num_alloced) {//We need more space!
            num_alloced *= 2;
            //Get us more space
            if ((topo = realloc(topo, (sizeof(node__t*) * (num_alloced + 1)))) == NULL) {
                err_sys("parse_node_config(): ERROR allocating memory!\n\n");
            }
            
            //Why isn't there a Recalloc?
            int j;
            for (j = num_alloced/2; j < num_alloced + 1; j++) {
                topo[j] = NULL;
            }
        }
        
        //Initialize space for next node
        if ((topo[i] = calloc(1, sizeof(node__t))) == NULL) {
            err_sys("  parse_node_config(): ERROR allocating memory!\n\n");
        }
        topo[i]->distance = MAX_DISTANCE;
        topo[i]->last_updated = time(NULL);
        bzero(&(topo[i]->destaddr), sizeof(topo[i]->destaddr));
        topo[i]->destaddr.sin_family = AF_INET;
        
        //Get next line in file
        if ((fgets(buffer, 100, fp)) == NULL) {
            //if it's an empty string, we're at EOF
            free(topo[i]);
            topo[i] = NULL;
            break;
        }
        
        if (sscanf(buffer, "%u %s %d", &nick, ipaddr, &port) != 3) {
            printf("  sscanf() failed\n");
            free(topo[i]);
            topo[i] = NULL;
            break;
        }
        
        //Check for unique destinations
        if (get_node(nick) != NULL) {
            err_sys("  parse_node_config(): Duplicate node destinations!\n\n");
        } else {
            topo[i]->destination = nick;
        }
        
        
        //printf("local_port=%d, port=%d\n", local_port, port);
        if (port == local_port) {
            this = topo[i];
            this->distance = 0;
            this->next_hop = nick;
        }
        
        topo[i]->destaddr.sin_port = htons(port);
        if (inet_pton(AF_INET, ipaddr, &(topo[i]->destaddr.sin_addr)) <= 0)
        {
            printf("parse_node_config():  inet_pton(%s) ERROR\n\n", ipaddr);
            free(topo[i]);
            topo[i] = NULL;
            break;
        }
        
    }
    fclose(fp);
    //print_topo();
}

void parse_neighbor_config(char *neighborfp)
{
    char buffer[100];
    uint32_t from, to;
    int dist;
    FILE *fp = fopen(neighborfp, "r");
    
    if (!fp) {
        printf("  parse_neighbor_config(): fopen(%s) ERROR.\n\n", neighborfp);
        exit(3);
    }
    
    while ((fgets(buffer, 100, fp)) != NULL) {
        //printf("parse_neighbor_config(): got %s", buffer);
        if (sscanf(buffer, "%u %u %d", &from, &to, &dist) != 3) {
            printf("  parse_neighbor_config(): sscanf(%s) ERROR.\n\n", buffer);
        }
        
        //Is it better to quit or invalidate the line?
        if (dist >= MAX_DISTANCE) {
            printf("  parse_neighbor_config(): dist=%d >= MAX=%d.  Skipping connection.\n\n", dist, MAX_DISTANCE);
            continue;
        }
        
        if (from == this->destination) {
            get_node(to)->distance = dist;
            get_node(to)->next_hop = to;
            get_node(to)->neighbor = 1;
        } else if (to == this->destination) {
            get_node(from)->distance = dist;
            get_node(from)->next_hop = from;
            get_node(from)->neighbor = 1;
        }
    }
    fclose(fp);
    //print_neighbors();
}

node__t *get_node(char nick)
{
    for (int i = 0; topo[i]; i++) {
        if (topo[i]->destination == nick) {
            return topo[i];
        }
    }
    return NULL;
}

int sizeof_topo()
{
    if (!topo) return 0;
    for (int i = 0; topo[i]; i++) {
		if (!topo[i+1]) {
			return i+1;
		}
	}
    return 0;
}

void print_node(node__t *node)
{
    if (!node) return;
    
    
    int max_distance_width = (int)floor(log10((double)abs(MAX_DISTANCE))) + 1; //from https://stackoverflow.com/a/1068870
    
    //FIXME: "undefined reference to log10 and floor" Why doesn't this work...?  
    //int label_width = (int)floor(log10((double)abs(sizeof_topo()))) + 1;
    int label_width = 1;
    
    printf("%p  %*u%c | %*d@%*u    %d     %s:%u",
        node,  //%p
        label_width,  //%*u
        node->destination,  //%*u
        ((node == this)?  //%c
           ('*')
           :((node->neighbor == 0)?
              (' ')
              :('-')
            )
        ),
        max_distance_width,  //%*d
        node->distance,  //%*d
        label_width, //(int)floor(log10(abs((float)sizeof_topo()))) + 1,  //%*u
        (node->next_hop == 0)?(0):(node->next_hop),  //%*u
        (time(NULL) - node->last_updated),  //%d
        inet_ntoa(node->destaddr.sin_addr),  //%s
        ntohs(node->destaddr.sin_port)  //%u
    );
}

void print_topo()
{
    if (!topo) return;
    
    printf("------------------Topography--------------------\n");
    printf("Node            | Dist     Time   IP\n");
    printf("------------------------------------------------\n");
    
    for (int i = 0; topo[i]; i++) {
        print_node(topo[i]);
        printf("\n");
    }
    printf("------------------------------------------------\n\n");
}

void free_topo()
{
    for (int i = 0; topo[i]; i++) {
        free(topo[i]);
    }
    free(topo);
}

packet__t *new_packet(int num_entries)
{
    packet__t *p_new = calloc(1, sizeof(packet__t) 
                                      - sizeof(uint8_t) 
                                      + (num_entries * sizeof(entry__t)));
                                      
    if (!p_new) {
        err_sys("  new_blank_packet(): ERROR allocating memory!\n\n");
    }
    p_new->command = 2;
    p_new->version = 1;
    p_new->num_entries = num_entries;
    
    entry__t *entries = (entry__t*) &(p_new->entries);
    for (int i = 0; i < p_new->num_entries; i++) {
        entries[i].family = 2;
    }
    
    return p_new;
}

int sizeof_packet(packet__t *pkt)
{
    return (sizeof(packet__t) 
            - sizeof(uint8_t) 
            + (pkt->num_entries * sizeof(entry__t))
           );
}

int is_valid_route(node__t *node)
{
    return (
            (node->next_hop != 0) 
            && (time(NULL) - node->last_updated <= DEAD_ROUTE)
            && (node != this)
           );
}

void create_route_packet(int signo)
{
    printf("create_route_packet() started!\n");
    
    int num_routes = 0;
    packet__t *p_routes;
    entry__t *p_entries;
    
    //how big is our packet?
    for (int i = 0; topo[i]; i++) {
        if (is_valid_route(topo[i])) {
            num_routes++;
        }
    }
    
    //create packet
    p_routes = new_packet(num_routes);
    entry__t *entries = (entry__t*) &(p_routes->entries);
    
    //fill in packet entries 
    for (int i = 0; topo[i]; i++) {
        if (is_valid_route(topo[i])) {
            entries->addr = topo[i]->destination;
            entries->distance = topo[i]->distance;
            entries++;
        }
    }
    
    printf("  create_route_packet(): created packet with %d entries:\n", p_routes->num_entries);
    entry__t *tmp = (entry__t*) &(p_routes->entries);
    
    for (int i = 0; i < p_routes->num_entries; i++) {
        printf("  create_route_packet(): entry %d - %d@%c\n", i, tmp[i].distance, (char) tmp[i].addr);
    }
    
    //send packet to neighbors
    send_routes(p_routes);
    free(p_routes);
    
    //reset alarm
    alarm(0);
    alarm(UPDATE_INTERVAL);
}

void send_routes(packet__t *p_routes)
{
    for (int i = 0; topo[i]; i++) {
        if (topo[i]->neighbor) {
            printf("  send_routes(): sending to %s:%u\n",
                    inet_ntoa(topo[i]->destaddr.sin_addr),
                    ntohs(topo[i]->destaddr.sin_port)
                   );
            
            Sendto(incfd,
                   p_routes,
                   sizeof_packet(p_routes),
                   0,
                   (SA *) &(topo[i]->destaddr),
                   sizeof(topo[i]->destaddr)
                  );
        }
    }
}
