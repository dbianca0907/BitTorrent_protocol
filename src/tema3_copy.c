#include <mpi.h>
#include <pthread.h>
#include "structs.h"

bool mpi_state = false;
bool init_database = true;
bool init_list_of_clients = true;
bool mutex_state = false;
bool announce_download_complete = false;
bool close_client = false;
struct client *client;
struct database_tracker *database;

//TO DO: de sters total_needed_files
//TO DO: de sters files_downloaded din database

void print_client() {
    printf("Rank: %d\n", client->rank);
    printf("Nr files: %d\n", client->nr_files);
    for (int i = 0; i < client->nr_files; i++) {
        printf("File name: %s\n", client->files[i].name);
        printf("Nr chunks: %d\n", client->files[i].nr_chunks);
    }
    printf("Nr needed files: %d\n", client->nr_needed_files);
    for (int i = 0; i < client->nr_needed_files; i++) {
        printf("Needed file: %s\n", client->needed_files[i]);
        for (int m = 0; m < client->nr_files; m++) {
            if (strcmp(client->needed_files[i], client->files[m].name) == 0) {
                for (int j = 0; j < client->files[m].nr_chunks; j++) {
                    printf("%s\n", client->files[m].chunks[j]);
                }
            }
        }
    }
    printf("\n");
}

// Funcție pentru a trimite o structură file
void sendFile(struct file *f, int dest, int tag) {
    MPI_Send(f, sizeof(struct file), MPI_BYTE, dest, tag, MPI_COMM_WORLD);

    // Trimiterea matricei chunks
    MPI_Send(f->chunks, f->nr_chunks * (HASH_SIZE + 2), MPI_CHAR, dest, tag, MPI_COMM_WORLD);
    
}

// Funcție pentru a primi o structură file
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
                    // if (packet->rank == 2) {
                    //     printf("Am trimis clientul %d\n", database->files[j].clients[k].rank);
                    // }
                    sendClient(&database->files[j].clients[k], packet->rank, tag);
                }
                break;
            }
        }
    }
    // if (packet->rank == 2) {
    //     printf("Am trimis lista de clienti\n");
    // }
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
    packet.rank_dest = dest;
    strcpy(packet.chunk_hash, chunk_hash);
    strcpy(packet.file_name, file_name);
    MPI_Send(&packet, sizeof(struct packet_for_peer), MPI_BYTE, dest, 1, MPI_COMM_WORLD);
}

void print_database() {
    printf("DATABASE:\n");
    for (int i = 0; i < database->nr_files; i++) {
        printf("File %d: %s\n", i, database->files[i].name);
        printf("Nr clients: %d\n", database->files[i].num_clients);
        printf("Nr total chunks chunks: %d\n", database->files[i].num_total_chunks);
        for (int j = 0; j < database->files[i].num_clients; j++) {
            printf("Clientul %d are %d chunks\n", database->files[i].clients[j].rank, database->files[i].clients[j].nr_chunks);
        }
    }
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

    if(packet->rank == 2) {
        printf("SE TRIMITE PACHETUL CU DOWWNLOADED FIULES:%d\n", packet->downloaded_files);
    }
    
    MPI_Send(packet, sizeof(struct packet_for_tracker), MPI_BYTE, 0, tag, MPI_COMM_WORLD);
    for (int j = 0; j < packet->nr_files; j++) {
        sendFile(&packet->files[j], 0, tag);
    }
    if (type == UPDATE) {
        MPI_Send(client->needed_files, client->nr_needed_files * MAX_FILENAME, MPI_CHAR, 0, tag, MPI_COMM_WORLD);
    }
}

void init_client(int rank) {
    char path[100];
   // sprintf(path, "../checker/tests/test%d/in%d.txt", rank, rank);
    // FILE *fisier = fopen(path, "r");
    sprintf(path, "../checker/tests/test1/in%d.txt", rank);
    FILE *fisier = fopen(path, "r");
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
    // Se aloca memorie pentru lista de fisiere primita de la tracker
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
        for (int j = 0; j < client->files_list->num_clients; j++) {
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
    client->files[new_file_index].total_needed_chunks = client->files_list[i].num_total_chunks;
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
                *index_chunk = k; // incepe dupa intrerupere la index_chunk + 1
                (*chunk_counter)--;
                // if (rank == 2) {
                //     printf("CHUNK COUNTER: %d\n", *chunk_counter);
                //     printf("S A AJUNS LA CHUNKUL: %d -------------> SUNT %d segmente\n", k,  client->files[new_file_index].nr_chunks);
                //     printf("S a adaugat chunk-ul: %s\n", client->files[new_file_index].chunks[k]);
                // }
            } else {
                // Se trimite un mesaj de eroare
                printf("Eroare la downloadarea chunk-ului %s de la clientul %d\n", cl->chunks[k], cl->rank);
            }
        }
    }
    // if (rank == 2) {
    //     printf("ESTE LA CLIENTUL: %d\n", *index_client);
    //     printf("A RAMAS LA CHUNK-UL: %d\n", *index_chunk);
    // }
    client->nr_files++;
}

void update_list_of_files_from_tracker(int rank) {
     //Se primeste lista de file-uri de la tracker
     //Free files_list
    for (int i = 0; i < client->nr_needed_files; i++) {
        free(client->files_list[i].clients);
    }
    for (int i = 0; i < client->nr_needed_files; i++) {
        enum status status = client->files_list[i].status; // se pastreaza statusul pt fiecare file
        int old_num_clients = client->files_list[i].num_clients;
        MPI_Recv(&client->files_list[i], sizeof(struct file_tracker), MPI_BYTE, 0, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        client->files_list[i].status = status;
        client->files_list[i].clients = (struct client_tracker *)malloc(client->files_list[i].num_clients * sizeof(struct client_tracker));
        for (int j = 0; j < client->files_list[i].num_clients; j++) {
            receiveClient(&client->files_list[i].clients[j], 0, 4);
        }
        // if (rank == 2) {
        //     printf("Nodul %d a primit fisierul %s de la tracker\n", rank, client->files_list[i].name);
        //     printf("Numarul total de chunkuri este: %d\n", client->files_list[i].num_total_chunks);
        // }
        client->files_list[i].status = status;
    }
}

void download_complete_files() {
    for (int i = 0; i < client->nr_needed_files; i++) {
        if (client->rank == 2) {
            printf("STATUS FILES:%d\n", client->files_list[i].status);
        }
        if (client->files_list[i].status == COMPLETE) {
            for (int j = 0; j < client->nr_files; j++) {
                if (client->rank == 2) {
                    printf("Se compara %s cu %s", client->files_list[i].name, client->files[j].name);
                }
                if (strcmp(client->files_list[i].name, client->files[j].name) == 0) {
                    char name_output_file[100];
                    char path[100];
                    sprintf(name_output_file, "client%d_%s", client->rank, client->files[j].name);
                    sprintf(path, "../checker/tests/test1/%s", name_output_file);
                    FILE *output_file = fopen(path, "w");
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
    int tag;
    int index_client = 0;
    int index_chunk = 0;
    bool change_client = false;
    int chunk_counter = 10;
    int cnt = 0;

    while (!close_client) {
        if (init_list_of_clients) {
            // Se cere lista de fisiere de la tracker
            request_list_of_files(rank, 0);
            init_list_of_files_from_tracker(rank);
            char message[3];
            MPI_Bcast(message, 3, MPI_CHAR, 0, MPI_COMM_WORLD);
            init_list_of_clients = false;
        } else {
            // if (cnt == 8) {
            //     break;
            // }
            // // Se cere lista de fisiere de la tracker
            //daca file ul la care se face downloadul s a terminat, se notifica checkerul
            download_complete_files();
            send_packet_to_tracker(UPDATE, 3);
            if (client->rank == 2) {
                printf("DOWNLOAD FILES SI NEEDED SUNT %d %d\n", client->downloaded_files, client->nr_needed_files);
            }
            if (client->downloaded_files != client->needed_files) {
                update_list_of_files_from_tracker(rank);
            } else {
                if (client->rank == 2) {
                    printf("Intra in break\n");
                }
                break;
            }
            chunk_counter = 10; // de facut 10
        }
        if (client->rank == 2) {
                    printf("Ramane in looop aici?\n");
                }
        bool all_files_downloaded = false;
        //Se parcurge lista de needed_files si se cauta cate segmente mai am de downloadat
        for (int i = 0; i < client->nr_needed_files && chunk_counter > 0; i++) {
            if (client->files_list[i].status == COMPLETE || client->files_list[i].status == DOWNLOADED) { // de scos complete
                if (client->rank == 2) {
                    printf("INTRA AICI\n");
                }
                if (i == client->nr_needed_files - 1) {
                    all_files_downloaded = true;
                }
                continue;
            }
            bool found = false;
            // Se itereaza lista de fisiere pe care o are nodul
            for (int j = 0; j < client->nr_files && chunk_counter > 0; j++) {
                if (strcmp(client->needed_files[i], client->files[j].name) == 0) {
                    found = true;
                    //Se itereaza prin clientii din lista incepand de la cel gasit anterior
                    int m, k;
                    for (m = index_client; m < client->files_list[i].num_clients && chunk_counter > 0; m++) {
                        if (client->files_list[i].clients[m].rank == rank) {
                            // Daca s-a ajuns la clientul curent, se trece la urmatorul
                            continue;
                        }
                        // Se itereaza prin segmenetele clientului de pe indexul gasit anterior
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
                                if (client->rank == 2) {
                                    printf("A ajuns la nr chunkuri:%d\n", client->files[j].nr_chunks);
                                }
                            }
                            } else {
                                // Se trimite un mesaj de eroare
                                printf("Eroare la downloadarea chunk-ului %s de la clientul %d\n", cl->chunks[k], cl->rank);
                            }
                        }
                    }
                    if (rank == 2) {
                        printf("SE VERIFICA %d cu %d\n", client->files[j].nr_chunks, client->files_list[i].num_total_chunks);
                        printf("NUME: %s si %s\n", client->files[j].name, client->files_list[i].name);
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
        if (all_files_downloaded) {
            if (client->rank == 2) {
                printf("Intra aici\n");
            }
            break;
        }
    }
    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;
    while (!close_client) {
        struct packet_for_peer packet;
        MPI_Status status;
        packet.rank_dest = 0;
        MPI_Recv(&packet, sizeof(struct packet_for_peer), MPI_BYTE, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
        struct packet_for_peer response_packet;
        response_packet.type = ACK;
        MPI_Send(&response_packet, sizeof(struct packet_for_peer), MPI_BYTE, status.MPI_SOURCE, 2, MPI_COMM_WORLD);  
    }  
    if (client->rank == 2) {
    printf("A terminat uploadul\n");
    }
    return NULL;
}

// Funcție de parsare pentru pachete de tipul INIT
void parseInitPacket(struct packet_for_tracker *packet) {
    int index;
    bool file_was_added = false;
    if (init_database) {
        database = (struct database_tracker *)malloc(sizeof(struct database_tracker));
        database->nr_files = packet->nr_files;
        database->files = (struct file_tracker *)malloc(database->nr_files * sizeof(struct file_tracker));
        database->num_finished_clients = 0;
    }
    for (int i = 0; i < packet->nr_files; i++) {
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
                database->files[k].num_total_chunks += packet->files[i].nr_chunks;
                // printf("Added client %d to existing file %s\n", database->files[k].clients[index].rank, packet->files[i].name);
                // Trecere la următorul fișier
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
            // printf("Added new file %s with client %d\n", database->files[index].name, database->files[index].clients[0].rank);
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
                        // if (rank == 2) {
                        //     printf("Clientul are acelasi nr de chunkuri\n");
                        // }
                        return;
                    }
                    // if (rank == 2)
                    //     printf("Se inlocuiest old_chunks care este: %d cu nr_chunks care este: %d\n", database->files->clients[j].nr_chunks, nr_chunks);
                    int old_nr_chunks = database->files->clients[j].nr_chunks;
                    for (int k = old_nr_chunks; k < nr_chunks; k++) {
                        strcpy(database->files->clients[j].chunks[k], chunks[k]);
                    }
                    database->files[i].clients[j].nr_chunks = nr_chunks;
                    client_exists = true;
                    break;
                }
            }
            if (!client_exists) {
                // add new client in swarn and its chunks
                // if (rank == 2) {
                //     printf("Adding new client %d to file %s\n", rank, file_name);
                // }
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

// DE VERIFICAT FUNCTIA ASTA
int update_database(struct packet_for_tracker *packet) {
    // if (packet->rank == 2) {
    //     printf("Needed files si downloaded files sunt: %d si %d\n", packet->nr_needed_files, packet->downloaded_files);
    // }
    for (int i = 0; i < packet->nr_needed_files; i++) {
        for (int j = 0; j < packet->nr_files; j++) {        
            if (strcmp(packet->needed_files[i], packet->files[j].name) == 0) {
                // se verifica daca clientul este in swarn-ul fisierului, daca nu se adauga
                // daca este se actualizeaza numarul de chunkuri si chunkurile
                // if (packet->rank == 2) {
                //     printf("Inainte de update_swarn sunt: %d chunkuri\n", packet->files[j].nr_chunks);
                // }
                update_swarn_file(packet->rank, packet->files[j].name, packet->files[j].nr_chunks, packet->files[j].chunks);
            }
        }
    }
    if (packet->nr_needed_files - packet->downloaded_files == 0) {
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
        // Procesează structura primită de la nodul cu rankul i
        // printf("Received packet from rank %d\n", i);
        if (packet.type == INIT) {
            // printf("Rank: %d, nr_files: %d\n", packet.rank, packet.nr_files);
            packet.files = (struct file *)malloc(packet.nr_files * sizeof(struct file));
            for (int j = 0; j < packet.nr_files; j++) {
                // Procesează structura file
                receiveFile(&packet.files[j], i, 0);
            }
            parseInitPacket(&packet);
        }
    }
    // printf("Am primit toate pachetele de la clienti\n"); // --> se trimite mesaj bdcast ca se poate incepe descarcarea
    // printf("\n");
    // Se trimite mesaj broadcast cilentilor ca se poate incepe descarcarea
    char message[3] = "OK";
    MPI_Bcast(message, 3, MPI_CHAR, 0, MPI_COMM_WORLD);
    // printf("Am trimis mesajul de broadcast: %s\n", message);
    printf("\n");
    print_database();
    printf("\n");

    // Se trimit listele de clienti care au nevoie de un anumit fisier
    for (int i = 1; i < numtasks; i++) {
        struct packet_for_tracker packet;
        MPI_Recv(&packet, sizeof(struct packet_for_tracker), MPI_BYTE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (packet.type == REQUEST) {
            MPI_Recv(&packet.needed_files, packet.nr_needed_files * MAX_FILENAME, MPI_CHAR, packet.rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            send_list_of_clients(&packet, 0);
        }
    }
    MPI_Bcast(message, 3, MPI_CHAR, 0, MPI_COMM_WORLD); // anunta celelalte noduri ca s-au trimis listele
    printf("\n");
    // tracker-ul primeste update uri de la clienti o data la 10 segmente descarcate
    while (1) {
        struct packet_for_tracker packet;
        if (database->num_finished_clients == num_clients) {
            break;
        }
        MPI_Recv(&packet, sizeof(struct packet_for_tracker), MPI_BYTE,  MPI_ANY_SOURCE, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        packet.files = (struct file *)malloc(packet.nr_files * sizeof(struct file));
        if (packet.type == UPDATE) {
            // if (packet.rank == 2) {
            //     printf("Received update from rank %d\n", packet.rank);
            // }
            for (int j = 0; j < packet.nr_files; j++) {
                // Procesează structura file
                receiveFile(&packet.files[j], packet.rank, 3);
            }
            MPI_Recv(&packet.needed_files, packet.nr_needed_files * MAX_FILENAME, MPI_CHAR, packet.rank, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (packet.rank == 2) {
                printf("\n");
                print_database();
                printf("\n");
            }
            int res = update_database(&packet);
            if (res != -1) {
                send_list_of_clients(&packet, 4);
            }
            // in functie de needed_files se trimite files_list-ul actualizat
        }
    }
    printf("A terminat trackerul--> trimite mesaj\n");
    
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;
    if (!mpi_state) {
        init_client(rank);
        mpi_state = true;
    }
    // Se așteaptă mesajul de la tracker
    char message[3];
    MPI_Bcast(message, 3, MPI_CHAR, 0, MPI_COMM_WORLD);
    // printf("Received message from tracker: %s\n", message);

    // // Se cere lista de fisiere de la tracker
    // request_list_of_files(rank);
    // //Se primeste lista de file-uri de la tracker
    // for (int i = 0; i < client->nr_needed_files; i++) {
    //     MPI_Recv(&client->files_list[i], sizeof(struct file_tracker), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    //     printf("Nodul %d a primit fisierul %s de la tracker\n", rank, client->files_list[i].name);
    //     printf("Numarul total de chunkuri este: %d\n", client->files_list[i].num_total_chunks);
    //     // cand se primeste lista de file-uri toate sunt incomplete
    //     client->files_list[i].status = INCOMPLETE;
    //     printf("FIle-ul este: %d\n", client->files_list[i].status);
    //     for (int j = 0; j < client->files_list->num_clients; j++) {
    //         client->files_list[i].clients = (struct client_tracker *)malloc(client->files_list[i].num_clients * sizeof(struct client_tracker));
    //         receiveClient(&client->files_list[i].clients[j], 0, 0);
    //     }
    // }
    printf("A trimis mesaj de ok pentru restule proceselor\n");
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
    MPI_Finalize();
}
