/*
 * Daniel Farley - dfarley@ucsc.edu
 * Usage: ./myrip <node.config> <neightbor.config> <local_port>
 */

#include "mytimer.h"

#define UPDATE_INTERVAL 10  //also includes 0-4 seconds of randomness
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
int sockfd = 0;
mytimer_t tmr_send_routes = TIMER_INIT;
mytimer_t tmr_check_dead_routes = TIMER_INIT;

void parse_node_config(char *nodefp);
void parse_neighbor_config(char *neighborfp);
node__t *get_node(uint32_t nick);
int sizeof_topo();
void print_node(node__t *node);
void print_topo();
void free_topo();
packet__t *new_packet(int num_entries);
int sizeof_packet(packet__t *pkt);
void create_route_packet(time_t now);
void send_routes(packet__t *p_routes);
void check_route_validity(time_t now);
node__t *is_neighbor(struct sockaddr_in addr);
void update_routes(packet__t *p_recv, node__t *sender);

int main(int argc, char **argv)
{
    int n, len;
    struct sockaddr_in incaddr;
    packet__t *p_recv = new_packet(sizeof_topo());
    fd_set rset;
    struct timeval tv;
    
    srand(time(NULL));
    
    if (argc != 4) {
        printf("Usage: %s <node.config> <neightbor.config> <local_port>\n\n", argv[0]);
        exit(1);
    }
    
    local_port = strtoul(argv[3], NULL, 10);
    parse_node_config(argv[1]);
    parse_neighbor_config(argv[2]);
    
    print_topo();
    
    sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
    this->destaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    Bind(sockfd, (SA *) &(this->destaddr), sizeof(this->destaddr));
    
    //printf("starting timers: %u\n", time(NULL));
    timer_start(&tmr_send_routes, UPDATE_INTERVAL+(rand()%5), create_route_packet);
    timer_start(&tmr_check_dead_routes, DEAD_ROUTE, check_route_validity);
    
    for (;;) {
        
        len = sizeof(incaddr);
        
        tv_init(&tv);
        tv_timer(&tv, &tmr_send_routes);
        tv_timer(&tv, &tmr_check_dead_routes);
        
        FD_ZERO(&rset);
        FD_SET(sockfd, &rset);
        
        //printf("entering select: sec=%ld, usec=%ld\n", (long)tv.tv_sec, (long)tv.tv_usec);
        int n;
        if ((n = select(sockfd+1, &rset, NULL, NULL, &tv)) < 0) {
            err_quit("select() < 0, strerror(errno) = %s\n", strerror(errno));
        }
        
        //check for packet arrival
        if (FD_ISSET(sockfd, &rset)) {
            n = recvfrom(sockfd,
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
                node__t *sender;
                if ((sender = is_neighbor(incaddr)) != NULL) {
                    //TODO: do things with the packet
                    printf("got packet with %d entries from %s:%u\n", 
                        p_recv->num_entries, 
                        inet_ntoa(incaddr.sin_addr),
                        ntohs(incaddr.sin_port)
                    );
                    update_routes(p_recv, sender);
                } else {
                    printf("got packet from a non-neighbor, ignoring.\n");
                }
                
            }
            free(p_recv);
            p_recv = new_packet(sizeof_topo());
        }
        
        //printf("  ...checking timers\n");
        timer_check(&tmr_send_routes);
        timer_check(&tmr_check_dead_routes);
        
        
        print_topo();
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
            //get_node(to)->next_hop = to;
            get_node(to)->neighbor = 1;
        } else if (to == this->destination) {
            get_node(from)->distance = dist;
            //get_node(from)->next_hop = from;
            get_node(from)->neighbor = 1;
        }
    }
    fclose(fp);
    //print_neighbors();
}

node__t *get_node(uint32_t nick)
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
    int label_width = (int)floor(log10((double)abs(sizeof_topo()))) + 1;
    int time_width = (int)floor(log10((double)abs(DEAD_ROUTE))) + 1;
    
    printf("%p  %*u%c | %*d@%*u    %*u     %s:%u",
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
        time_width,
        (time(NULL) - node->last_updated),  //%*u
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
    printf("------------------------------------------------\n\n\n\n\n");
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

void create_route_packet(time_t now)
{
    printf("create_route_packet() started: %u\n", time(NULL));
    
    int num_routes = 0;
    packet__t *p_routes;
    entry__t *p_entries;
    
    //how big is our packet?
    for (int i = 0; topo[i]; i++) {
        if (topo[i]->next_hop != 0) {
            num_routes++;
        }
    }
    
    //create packet
    p_routes = new_packet(num_routes);
    entry__t *entries = (entry__t*) &(p_routes->entries);
    
    //fill in packet entries 
    for (int i = 0; topo[i]; i++) {
        if (topo[i]->next_hop != 0) {
            entries->addr = topo[i]->destination;
            entries->distance = topo[i]->distance;
            entries++;
        }
    }
    
    printf("  create_route_packet(): created packet with %d entries:\n", p_routes->num_entries);
    entry__t *tmp = (entry__t*) &(p_routes->entries);
    
    for (int i = 0; i < p_routes->num_entries; i++) {
        printf("  create_route_packet(): entry %d - %d@%u\n", i, tmp[i].distance, (char) tmp[i].addr);
    }
    
    //send packet to neighbors
    if (p_routes->num_entries > 0) {
        send_routes(p_routes);
    }
    free(p_routes);
    
    //reset timer
    timer_start(&tmr_send_routes, UPDATE_INTERVAL+(rand()%5), create_route_packet);
}

void send_routes(packet__t *p_routes)
{
    for (int i = 0; topo[i]; i++) {
        if (topo[i]->neighbor) {
            /*
            printf("  send_routes(): sending to %s:%u\n",
                    inet_ntoa(topo[i]->destaddr.sin_addr),
                    ntohs(topo[i]->destaddr.sin_port)
                   );
            */
            Sendto(sockfd,
                   p_routes,
                   sizeof_packet(p_routes),
                   0,
                   (SA *) &(topo[i]->destaddr),
                   sizeof(topo[i]->destaddr)
                  );
        }
    }
}

void check_route_validity(time_t now)
{
    printf("check_route_validity() started: %u\n", time(NULL));
    
    int next_death = DEAD_ROUTE;
    
    for (int i = 0; topo[i]; i++) {
        if (topo[i] == this) {
            topo[i]->last_updated = time(NULL);
        } else if (time(NULL) - topo[i]->last_updated > DEAD_ROUTE) {
            topo[i]->next_hop = 0;
            topo[i]->last_updated = time(NULL);
        }
    }
    
    for (int i = 0; topo[i]; i++) {
        if ((DEAD_ROUTE - (time(NULL) - topo[i]->last_updated)) < next_death) {
            next_death = (DEAD_ROUTE - (time(NULL) - topo[i]->last_updated));
        }
    }
    
    //TODO: check_dead_routes should figure out when the next one will be invalid, not a const
    timer_start(&tmr_check_dead_routes, next_death + 1, check_route_validity);
}

node__t *is_neighbor(struct sockaddr_in addr)
{
    for (int i = 0; topo[i]; i++) {
        if ((topo[i]->destaddr.sin_addr.s_addr == addr.sin_addr.s_addr) 
                && (topo[i]->destaddr.sin_port == addr.sin_port)) {
            return topo[i];
        }
    }
    return NULL;
}

void update_routes(packet__t *p_recv, node__t *sender)
{
    entry__t *entries = (entry__t*) &(p_recv->entries);
    for (int i = 0; i < p_recv->num_entries; i++) {
        node__t *node = get_node(entries[i].addr);
        
        printf("  entries[%d]: old_dist=%d, new_dist=%d via %d\n", i, node->distance, (entries[i].distance + sender->distance), sender->destination);
        
        if ((entries[i].distance + sender->distance) <= node->distance) {
            node->distance = (entries[i].distance + sender->distance);
            node->distance = (node->distance > MAX_DISTANCE)?(MAX_DISTANCE):(node->distance);
            
            node->next_hop = sender->destination;
        
            node->last_updated = time(NULL);
        } 
    }
}
