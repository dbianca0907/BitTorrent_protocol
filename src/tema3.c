#include <mpi.h>
#include <pthread.h>
#include "structs.h"

bool client_init = false;
bool init_database = true;
bool init_list_of_clients = true;
struct client *client;
struct database_tracker *database;

void sendFile(struct file *f, int dest, int tag) {
    MPI_Send(f, sizeof(struct file), MPI_BYTE, dest, tag, MPI_COMM_WORLD);

    // Trimiterea matricei chunks
    MPI_Send(f->chunks, f->nr_chunks * (HASH_SIZE + 2), MPI_CHAR, dest, tag, MPI_COMM_WORLD);
    
}

void receiveFile(struct file *f, int source, int tag) {
    MPI_Recv(f, sizeof(struct file), MPI_BYTE, source, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Primirea matricei chunks
    MPI_Recv(f->chunks, f->nr_chunks * (HASH_SIZE + 2), MPI_CHAR, source, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void sendClient(struct client_tracker *c, int dest, int tag) {
    MPI_Send(c, sizeof(struct client_tracker), MPI_BYTE, dest, tag, MPI_COMM_WORLD);

    // Trimiterea matricei chunks
    MPI_Send(c->chunks, c->nr_chunks * (HASH_SIZE + 2), MPI_CHAR, dest, tag, MPI_COMM_WORLD);
}

void receiveClient(struct client_tracker *c, int source, int tag) {
    MPI_Recv(c, sizeof(struct client_tracker), MPI_BYTE, source, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Primirea matricei chunks
    MPI_Recv(c->chunks, c->nr_chunks * (HASH_SIZE + 2), MPI_CHAR, source, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void send_list_of_clients(struct packet_for_tracker *packet, int tag) {
    //Search in the database for the files and send the list to the client
    for (int i = 0; i < packet->nr_needed_files; i++) {
        for (int j = 0; j < database->nr_files; j++) {
            if (strcmp(packet->needed_files[i], database->files[j].name) == 0) {
                MPI_Send(&database->files[j], sizeof(struct file_tracker), MPI_BYTE, packet->rank, tag, MPI_COMM_WORLD);
                for (int k = 0; k < database->files[j].num_clients; k++) {
                    sendClient(&database->files[j].clients[k], packet->rank, tag);
                }
                break;
            }
        }
    }
}

void request_list_of_files(int rank, int tag) {
    struct packet_for_tracker packet;
    packet.type = REQUEST;
    packet.rank = rank;
    packet.nr_needed_files = client->nr_needed_files;
    for (int i = 0; i < client->nr_needed_files; i++) {
        strcpy(packet.needed_files[i], client->needed_files[i]);
    }
    MPI_Send(&packet, sizeof(struct packet_for_tracker), MPI_BYTE, 0, tag, MPI_COMM_WORLD);
    MPI_Send(client->needed_files, client->nr_needed_files * MAX_FILENAME, MPI_CHAR, 0, tag, MPI_COMM_WORLD);
}

void request_chunk(int rank, int dest, char *chunk_hash, char *file_name) {
    struct packet_for_peer packet;
    packet.type = REQUEST;
    strcpy(packet.chunk_hash, chunk_hash);
    strcpy(packet.file_name, file_name);
    MPI_Send(&packet, sizeof(struct packet_for_peer), MPI_BYTE, dest, 1, MPI_COMM_WORLD);
}

void send_packet_to_tracker(enum packet_type type, int tag) {
    struct packet_for_tracker *packet = (struct packet_for_tracker*) malloc(sizeof(struct packet_for_tracker));
    packet->nr_files = client->nr_files;
    packet->files = (struct file*) malloc(client->nr_files * sizeof(struct file));

    for (int i = 0; i < client->nr_files; i++) {
        strcpy(packet->files[i].name, client->files[i].name);
        packet->files[i].nr_chunks = client->files[i].nr_chunks;
        for (int j = 0; j < client->files[i].nr_chunks; j++) {
            strcpy(packet->files[i].chunks[j], client->files[i].chunks[j]);
        }
    }
    packet->type = type;
    packet->rank = client->rank;
    packet->nr_needed_files = client->nr_needed_files;
    packet->downloaded_files = client->downloaded_files;
    
    MPI_Send(packet, sizeof(struct packet_for_tracker), MPI_BYTE, 0, tag, MPI_COMM_WORLD);
    for (int j = 0; j < packet->nr_files; j++) {
        sendFile(&packet->files[j], 0, tag);
    }
    if (type == UPDATE) {
        MPI_Send(client->needed_files, client->nr_needed_files * MAX_FILENAME, MPI_CHAR, 0, tag, MPI_COMM_WORLD);
    }
}

void init_client(int rank) {
    char file[10];
   
    sprintf(file, "in%d.txt", rank);
    FILE *fisier = fopen(file, "r");
    if (fisier == NULL) {
        fprintf(stderr, "Nu s-a putut deschide fisierul.\n");
        exit(-1);
    }

    client = (struct client*) malloc(sizeof(struct client));
    client->rank = rank;
    client->downloaded_files = 0;
    fscanf(fisier, "%d", &client->nr_files);
    client->files = (struct file*) malloc(client->nr_files * sizeof(struct file));

    for (int i = 0; i < client->nr_files; i++) {
        fscanf(fisier, "%s", client->files[i].name);
        fscanf(fisier, "%d", &client->files[i].nr_chunks);
        client->files[i].status = COMPLETE;
        for (int j = 0; j < client->files[i].nr_chunks; j++) {
            fscanf(fisier, "%32s", client->files[i].chunks[j]);
            client->files[i].chunks[j][HASH_SIZE + 1] = '\0';
        }
    }

    fscanf(fisier, "%d", &client->nr_needed_files);
    for (int i = 0; i < client->nr_needed_files; i++) {
        fscanf(fisier, "%s", client->needed_files[i]);
    }
    client->files_list = (struct file_tracker*) malloc(client->nr_needed_files * sizeof(struct file_tracker));
    send_packet_to_tracker(INIT, 0);
    fclose(fisier);
}

void init_list_of_files_from_tracker(int rank) {
    //Se primeste lista de file-uri de la tracker
    for (int i = 0; i < client->nr_needed_files; i++) {
        MPI_Recv(&client->files_list[i], sizeof(struct file_tracker), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        client->files_list[i].status = INCOMPLETE;
        client->files_list[i].clients = (struct client_tracker *)malloc(client->files_list[i].num_clients * sizeof(struct client_tracker));
        for (int j = 0; j < client->files_list[i].num_clients; j++) {
            receiveClient(&client->files_list[i].clients[j], 0, 0);
        }
    }
}

void add_new_file_to_client(int rank, int i, int *chunk_counter, int *index_chunk, int *index_client) {
    // Se realoca files
    client->files = (struct file*) realloc(client->files, (client->nr_files + 1) * sizeof(struct file));
    int new_file_index = client->nr_files;
    client->files[new_file_index].nr_chunks = 0;
    strcpy(client->files[new_file_index].name, client->files_list[i].name);
    for (int m = 0; m < client->files_list[i].num_clients && *chunk_counter > 0; m++) {
        *index_client = m;
        struct client_tracker *cl = &client->files_list[i].clients[m];
        for (int k = 0; k < cl->nr_chunks && *chunk_counter > 0; k++) {
            // Se trimite request pentru fiecare chunk
            request_chunk(rank, cl->rank, cl->chunks[k], client->files_list[i].name);
            
            // Se primeste cate un chunk
            struct packet_for_peer packet;
            MPI_Recv(&packet, sizeof(struct packet_for_peer), MPI_BYTE, cl->rank, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (packet.type == ACK) {
                strcpy(client->files[new_file_index].chunks[k], cl->chunks[k]);
                client->files[new_file_index].nr_chunks++;
                *index_chunk = k;
                (*chunk_counter)--;
            } else {
                printf("Eroare la downloadarea chunk-ului %s de la clientul %d\n", cl->chunks[k], cl->rank);
            }
        }
    }
    client->nr_files++;
}

void update_list_of_files_from_tracker(int rank) {
    if (client->nr_needed_files == client->downloaded_files) {
        return;
    }
    for (int i = 0; i < client->nr_needed_files; i++) {
        free(client->files_list[i].clients);
    }
    for (int i = 0; i < client->nr_needed_files; i++) {
        enum status status = client->files_list[i].status;
        MPI_Recv(&client->files_list[i], sizeof(struct file_tracker), MPI_BYTE, 0, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        client->files_list[i].status = status;
        client->files_list[i].clients = (struct client_tracker *)malloc(client->files_list[i].num_clients * sizeof(struct client_tracker));
        for (int j = 0; j < client->files_list[i].num_clients; j++) {
            receiveClient(&client->files_list[i].clients[j], 0, 4);
        }
        client->files_list[i].status = status;
    }
}

void download_complete_files() {
    for (int i = 0; i < client->nr_needed_files; i++) {
        if (client->files_list[i].status == COMPLETE) {
            for (int j = 0; j < client->nr_files; j++) {
                if (strcmp(client->files_list[i].name, client->files[j].name) == 0) {
                    char name_output_file[100];
                    sprintf(name_output_file, "client%d_%s", client->rank, client->files[j].name);
                    FILE *output_file = fopen(name_output_file, "w");
                    if (output_file == NULL) {
                        perror("Eroare la deschiderea fișierului de ieșire");
                        return;
                    }
                    for (int k = 0; k < client->files[j].nr_chunks; k++) {
                        fprintf(output_file, "%s", client->files[j].chunks[k]);
                        if (k != client->files[j].nr_chunks - 1) {
                            fprintf(output_file, "\n");
                        }
                    }
                    client->files_list[i].status = DOWNLOADED;
                    client->files[j].status = DOWNLOADED;
                    client->downloaded_files++;
                    fclose(output_file);
                }
            }
        }
    }
}

void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;
    int index_client = 0;
    int index_chunk = 0;
    int chunk_counter = 10;

    while (1) {
        if (init_list_of_clients) {
            // Se cere lista de fisiere de la tracker
            request_list_of_files(rank, 0);
            init_list_of_files_from_tracker(rank);
            init_list_of_clients = false;
        } else {
            download_complete_files();
            send_packet_to_tracker(UPDATE, 3);
            update_list_of_files_from_tracker(rank);
            chunk_counter = 10;
        }
        bool all_files_downloaded = false;
        //Se parcurge lista de needed_files si se cauta cate segmente mai am de descarcat
        for (int i = 0; i < client->nr_needed_files && chunk_counter > 0; i++) {
            if (client->files_list[i].status == DOWNLOADED) {
                if (i == client->nr_needed_files - 1) {
                    all_files_downloaded = true;
                }
                continue;
            }
            bool found = false;
            // Se itereaza lista de fisiere pe care o are clientul
            for (int j = 0; j < client->nr_files && chunk_counter > 0; j++) {
                if (strcmp(client->needed_files[i], client->files[j].name) == 0) {
                    found = true;
                    //Se itereaza prin files_list-ul primit de la tracker incepand de la index_client
                    int m, k;
                    for (m = index_client; m < client->files_list[i].num_clients && chunk_counter > 0; m++) {
                        if (client->files_list[i].clients[m].rank == rank) {
                            // Daca s-a ajuns la clientul curent, se trece la urmatorul
                            continue;
                        }
                        // Se itereaza prin segmenetele clientului de pe index_client
                        struct client_tracker *cl = &client->files_list[i].clients[m];
                        for (k = index_chunk + 1; k < cl->nr_chunks && chunk_counter > 0; k++) {
                            // Se trimite request pentru fiecare chunk
                            request_chunk(rank, cl->rank, cl->chunks[k], client->files_list[i].name);
                            
                            // Se primeste cate un chunk
                            struct packet_for_peer packet;
                            MPI_Recv(&packet, sizeof(struct packet_for_peer), MPI_BYTE, cl->rank, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                            if (packet.type == ACK) {
                            // Se adauga segmentul
                            strcpy(client->files[j].chunks[client->files[j].nr_chunks], client->files_list[i].clients[m].chunks[k]);
                            client->files[j].nr_chunks++;
                            chunk_counter--;
                            if (chunk_counter == 0) {
                                index_client = m;
                                index_chunk = k;
                            }
                            }
                        }
                    }
                    if (client->files[j].nr_chunks == client->files_list[i].num_total_chunks
                            && client->files_list[i].status != DOWNLOADED) {
                                client->files_list[i].status = COMPLETE;
                    }
                }
            }
            if (client->nr_files == 0 || !found) {
                add_new_file_to_client(rank, i, &chunk_counter, &index_chunk, &index_client);
            }
        }
        if (all_files_downloaded || client->nr_needed_files == 0) {
            break;
        }
    }
    return NULL;
}

void *upload_thread_func(void *arg)
{
    while (1) {
        struct packet_for_peer packet;
        MPI_Status status;
        MPI_Recv(&packet, sizeof(struct packet_for_peer), MPI_BYTE, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
        if (status.MPI_SOURCE == TRACKER_RANK && packet.type == SHUTDOWN) {
            break;
        }
        struct packet_for_peer response_packet;
        response_packet.type = ACK;
        MPI_Send(&response_packet, sizeof(struct packet_for_peer), MPI_BYTE, status.MPI_SOURCE, 2, MPI_COMM_WORLD);  
    }  
    return NULL;
}

void parseInitPacket(struct packet_for_tracker *packet) {
    int index;
    if (init_database) {
        database = (struct database_tracker *)malloc(sizeof(struct database_tracker));
        database->nr_files = packet->nr_files;
        database->files = (struct file_tracker *)malloc(database->nr_files * sizeof(struct file_tracker));
        database->num_finished_clients = 0;
    }
    if (packet->nr_needed_files == 0) {
        database->num_finished_clients++;
    }
    for (int i = 0; i < packet->nr_files; i++) {
        bool file_was_added = false;
        for (int k = 0; k < database->nr_files; k++) {
            // Se verifica dacă fișierul există deja în baza de date
            if (!init_database && strcmp(database->files[k].name, packet->files[i].name) == 0) {
                // Realocare și inițializare a matricei de client_tracker pentru noul client
                database->files[k].clients = (struct client_tracker *)realloc(database->files[k].clients, (database->files->num_clients + 1) * sizeof(struct client_tracker));
                database->files[k].num_clients++;
                index = database->files[k].num_clients - 1;
                // Copiere informații despre chunks și alte atribute la fileul gasit;
                database->files[k].clients[index].rank = packet->rank;
                database->files[k].clients[index].nr_chunks = packet->files[i].nr_chunks;
                for (int j = 0; j < packet->files[i].nr_chunks; j++) {
                    strcpy(database->files[k].clients[index].chunks[j], packet->files[i].chunks[j]);
                }
                file_was_added = true;
                break;
            } 
        }
        if (!file_was_added) {
            // Se adauga un fisier nou in baza de date
            index = i; // pentru cazul de initializare a bazei de date
            if (!init_database) {
                //daca nu este la initializarea matricei se realoca si se adauga un nou fisier
                database->files = (struct file_tracker *)realloc(database->files, (database->nr_files + 1) * sizeof(struct file_tracker));
                database->nr_files++;
                index = database->nr_files - 1;
            }
            strcpy(database->files[index].name, packet->files[i].name);
            database->files[index].num_clients = 1;
            database->files[index].clients = (struct client_tracker *)malloc(sizeof(struct client_tracker));
            database->files[index].clients[0].rank = packet->rank;
            database->files[index].clients[0].nr_chunks = packet->files[i].nr_chunks;
            for (int j = 0; j < packet->files[i].nr_chunks; j++) {
                strcpy(database->files[index].clients[0].chunks[j], packet->files[i].chunks[j]);
            }
            database->files[index].num_total_chunks = packet->files[i].nr_chunks;
        }
    }
    init_database = false;
}

void update_swarn_file(int rank, char file_name[MAX_FILENAME], int nr_chunks, char chunks[MAX_CHUNKS][HASH_SIZE + 2]) {
    for (int i = 0; i < database->nr_files; i++) {
        if (strcmp(database->files[i].name, file_name) == 0) {
            bool client_exists = false;
            for (int j = 0; j < database->files[i].num_clients; j++) {
                if (database->files[i].clients[j].rank == rank) {
                    // se updateaza swarnul doar daca clientul are un numarul diferit de chunks
                    if (database->files[i].num_total_chunks == nr_chunks) {
                        return;
                    }
                    int old_nr_chunks = database->files[i].clients[j].nr_chunks;
                    for (int k = old_nr_chunks; k < nr_chunks; k++) {
                        strcpy(database->files[i].clients[j].chunks[k], chunks[k]);
                    }
                    database->files[i].clients[j].nr_chunks = nr_chunks;
                    client_exists = true;
                    break;
                }
            }
            if (!client_exists) {
                database->files[i].clients = (struct client_tracker *)realloc(database->files[i].clients, (database->files[i].num_clients + 1) * sizeof(struct client_tracker));
                int new_index = database->files[i].num_clients;
                database->files[i].clients[new_index].rank = rank;
                database->files[i].clients[new_index].nr_chunks = nr_chunks;
                for (int m = 0; m < nr_chunks; m++) {
                    strcpy(database->files[i].clients[new_index].chunks[m], chunks[m]);
                }
                database->files[i].num_clients++;
            }
        }
    }
}

int update_database(struct packet_for_tracker *packet) {
    for (int i = 0; i < packet->nr_needed_files; i++) {
        for (int j = 0; j < packet->nr_files; j++) {        
            if (strcmp(packet->needed_files[i], packet->files[j].name) == 0) {
                update_swarn_file(packet->rank, packet->files[j].name, packet->files[j].nr_chunks, packet->files[j].chunks);
            }
        }
    }
    if (packet->nr_needed_files != 0 && packet->nr_needed_files - packet->downloaded_files == 0) {
        database->num_finished_clients++;
        return -1;
    }
    return 0;
}

void tracker(int numtasks, int rank) {
    int num_clients = numtasks - 1;
    for (int i = 1; i < numtasks; i++) {
        struct packet_for_tracker packet;
        MPI_Recv(&packet, sizeof(struct packet_for_tracker), MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (packet.type == INIT) {
            packet.files = (struct file *)malloc(packet.nr_files * sizeof(struct file));
            for (int j = 0; j < packet.nr_files; j++) {
                receiveFile(&packet.files[j], i, 0);
            }
            parseInitPacket(&packet);
        }
    }
    
    // Se trimite mesaj broadcast cilentilor ca sa poata incepe descarcarea
    char message[3] = "OK";
    MPI_Bcast(message, 3, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Se trimit listele de clienti
    for (int i = 1; i < numtasks; i++) {
        struct packet_for_tracker packet;
        MPI_Recv(&packet, sizeof(struct packet_for_tracker), MPI_BYTE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (packet.type == REQUEST) {
            MPI_Recv(&packet.needed_files, packet.nr_needed_files * MAX_FILENAME, MPI_CHAR, packet.rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            send_list_of_clients(&packet, 0);
        }
    }
    // tracker-ul primeste update uri de la clienti o data la 10 segmente descarcate
    while (1) {
        struct packet_for_tracker packet;
        if (database->num_finished_clients == num_clients) {
            break;
        }
        MPI_Recv(&packet, sizeof(struct packet_for_tracker), MPI_BYTE,  MPI_ANY_SOURCE, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        packet.files = (struct file *)malloc(packet.nr_files * sizeof(struct file));
        if (packet.type == UPDATE) {
            for (int j = 0; j < packet.nr_files; j++) {
                receiveFile(&packet.files[j], packet.rank, 3);
            }
            MPI_Recv(&packet.needed_files, packet.nr_needed_files * MAX_FILENAME, MPI_CHAR, packet.rank, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            int res = update_database(&packet);
            if (res != -1) {
                send_list_of_clients(&packet, 4);
            }
        }
    }
    for (int i = 1; i < numtasks; i++) {
        struct packet_for_peer packet;
        packet.type = SHUTDOWN;
        MPI_Send(&packet, sizeof(struct packet_for_peer), MPI_BYTE, i, 1, MPI_COMM_WORLD);
    }
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;
    if (!client_init) {
        init_client(rank);
        client_init = true;
    }
    // Se așteaptă mesajul de la tracker
    char message[3];
    MPI_Bcast(message, 3, MPI_CHAR, 0, MPI_COMM_WORLD);

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}

void free_memory(int rank) {
    // Eliberarea memoriei pentru structura database
    if (rank == TRACKER_RANK && database != NULL) {
        for (int i = 0; i < database->nr_files; i++) {
            free(database->files[i].clients);
        }
        free(database->files);
        free(database);
    } else {
        // Eliberarea memoriei pentru structura client
        if (client != NULL) {
            free(client->files_list);
            free(client->files);
            free(client);
        }
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }
    free_memory(rank);
    MPI_Finalize();
}
