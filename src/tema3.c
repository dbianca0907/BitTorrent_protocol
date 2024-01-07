#include <mpi.h>
#include <pthread.h>
#include "structs.h"

bool mpi_state = false;
bool init_database = true;
struct client *client;
struct database_tracker *database;

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
    }
    printf("\n");
}

// Funcție pentru a trimite o structură file
void sendFile(struct file *f, int dest) {
    MPI_Send(f, sizeof(struct file), MPI_BYTE, dest, 0, MPI_COMM_WORLD);

    // Trimiterea matricei chunks
    MPI_Send(f->chunks, f->nr_chunks * (HASH_SIZE + 2), MPI_CHAR, dest, 0, MPI_COMM_WORLD);
    
}

// Funcție pentru a primi o structură file
void receiveFile(struct file *f, int source) {
    MPI_Recv(f, sizeof(struct file), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Primirea matricei chunks
    MPI_Recv(f->chunks, f->nr_chunks * (HASH_SIZE + 2), MPI_CHAR, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void sendClient(struct client_tracker *c, int dest) {
    MPI_Send(c, sizeof(struct client_tracker), MPI_BYTE, dest, 0, MPI_COMM_WORLD);

    // Trimiterea matricei chunks
    MPI_Send(c->chunks, c->nr_chunks * (HASH_SIZE + 2), MPI_CHAR, dest, 0, MPI_COMM_WORLD);
}

void receiveClient(struct client_tracker *c, int source) {
    MPI_Recv(c, sizeof(struct client_tracker), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Primirea matricei chunks
    MPI_Recv(c->chunks, c->nr_chunks * (HASH_SIZE + 2), MPI_CHAR, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void send_list_of_clients(struct packet_for_tracker *packet) {
    //Search in the database for the files and send the list to the client
    for (int i = 0; i < packet->nr_needed_files; i++) {
        for (int j = 0; j < database->nr_files; j++) {
            if (strcmp(packet->needed_files[i], database->files[j].name) == 0) {
                MPI_Send(&database->files[j], sizeof(struct file_tracker), MPI_BYTE, packet->rank, 0, MPI_COMM_WORLD);
                for (int k = 0; k < database->files[j].num_clients; k++) {
                    sendClient(&database->files[j].clients[k], packet->rank);
                }
                break;
            }
        }
    }

}

void request_list_of_files(int rank) {
    struct packet_for_tracker packet;
    packet.type = REQUEST;
    packet.rank = rank;
    packet.nr_needed_files = client->nr_needed_files;
    for (int i = 0; i < client->nr_needed_files; i++) {
        strcpy(packet.needed_files[i], client->needed_files[i]);
        printf("Nodul %d are nevoie de fisierul %s\n", rank, packet.needed_files[i]);
    }
    MPI_Send(&packet, sizeof(struct packet_for_tracker), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    MPI_Send(client->needed_files, client->nr_needed_files * MAX_FILENAME, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
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
        printf("Nr chunks: %d\n", database->files[i].num_total_chunks);
        for (int j = 0; j < database->files[i].num_clients; j++) {
            printf("Clientul %d are %d chunks\n", database->files[i].clients[j].rank, database->files[i].clients[j].nr_chunks);
        }
    }
}

void send_packet_to_tracker() {
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
    packet->type = INIT;
    packet->rank = client->rank;

    MPI_Send(packet, sizeof(struct packet_for_tracker), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    for (int j = 0; j < packet->nr_files; j++) {
        sendFile(&packet->files[j], 0);
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
    fscanf(fisier, "%d", &client->nr_files);
    client->files = (struct file*) malloc(client->nr_files * sizeof(struct file));

    for (int i = 0; i < client->nr_files; i++) {
        fscanf(fisier, "%s", client->files[i].name);
        fscanf(fisier, "%d", &client->files[i].nr_chunks);
        for (int j = 0; j < client->files[i].nr_chunks; j++) {
            fscanf(fisier, "%32s", client->files[i].chunks[j]);
            client->files[i].chunks[j][HASH_SIZE + 1] = '\0';
        }
        printf("\n");
    }

    fscanf(fisier, "%d", &client->nr_needed_files);
    for (int i = 0; i < client->nr_needed_files; i++) {
        fscanf(fisier, "%s", client->needed_files[i]);
    }
    // Se aloca memorie pentru lista de fisiere primita de la tracker
    client->files_list = (struct file_tracker*) malloc(client->nr_needed_files * sizeof(struct file_tracker));
    send_packet_to_tracker();
    fclose(fisier);
}

void add_new_file_to_client(int rank, int i) {
    // Se realoca files
    client->files = (struct file*) realloc(client->files, (client->nr_files + 1) * sizeof(struct file));
    int new_file_index = client->nr_files;
    client->files[new_file_index].nr_chunks = 0;
    for (int m = 0; m < client->files_list[i].num_clients; m++) {
        //printf("Este la iteratia: %d\n", m);
        struct client_tracker *cl = &client->files_list[i].clients[m];
        for (int k = 0; k < cl->nr_chunks; k++) {
            // Se trimite request pentru fiecare chunk
            request_chunk(rank, cl->rank, cl->chunks[k], client->files_list[i].name);
            
            // Se primeste cate un chunk
            struct packet_for_peer packet;
            enum packet_type type = WAIT;
            MPI_Recv(&packet, sizeof(struct packet_for_peer), MPI_BYTE, cl->rank, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            strcpy(client->files[new_file_index].chunks[k], cl->chunks[k]);
            client->files[new_file_index].nr_chunks++;
            if (rank == 1) {
                printf("S A AJUNS LA CHUNKUL: %d -------------> SUNT %d segmente\n", k,  client->files[new_file_index].nr_chunks);
                printf("S a adaugat chunk-ul: %s\n", client->files[new_file_index].chunks[k]);
            }
        }
    }
    client->nr_files++;
    printf("\n");
}

void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;
     //Se parcurge lista de needed_files si se cauta cate segmente mai am de downloadat
    for (int i = 0; i < client->nr_needed_files; i++) {
        if (client->files_list[i].status == COMPLETE) {
            // 
            continue;
        }
        // Se itereaza lista de fisiere pe care o are nodul
        for (int j = 0; j < client->nr_files; j++) {
            if (strcmp(client->needed_files[i], client->files[j].name) == 0) {
                int diff = client->files_list[i].num_total_chunks - client->files[j].nr_chunks;
                if (diff == 0) {
                    //file -ul s-a downloadat complet
                    client->files_list[i].status = COMPLETE;
                    break;
                }
                // Se aloca memorie pentru cate un chunk din file-ul care trebuie downloadat
                // Mai intai se itereaza prin clientii disponibili se cauta indexul de la care ar trebui inceputa downloadarea
                int aux = client->files[j].nr_chunks;
                int index = 0;
                for (int k = 0; k < client->files_list[i].num_clients; k++) {
                    aux -= client->files_list[i].clients[k].nr_chunks;
                    if (aux == 0) {
                        index = k + 1;
                        break;
                    } else if (aux < 0) {
                        index = k;
                        aux = client->files_list[i].clients[k].nr_chunks + aux; // segmentul de la care se incepe
                        break;
                    }
                }
                //Se itereaza prin clientii din lista incepand de la cel gasit anterior
                for (int m = index; m < client->files_list[i].num_clients; m++) {
                    // Se itereaza prin segmenetele clientului de pe indexul gasit anterior
                    struct client_tracker *cl = &client->files_list[i].clients[m];
                    for (int k = aux; k < cl->nr_chunks; k++) {
                        // Se trimite request pentru fiecare chunk
                        request_chunk(rank, cl->rank, cl->chunks[k], client->files_list[i].name);
                        // Se primeste cate un chunk
                        struct packet_for_peer packet;
                        MPI_Recv(&packet, sizeof(struct packet_for_peer), MPI_BYTE, cl->rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        if (strcmp(packet.message, "OK") == 0) {
                        // Se adauga segmentul
                        strcpy(client->files[j].chunks[client->files[j].nr_chunks], packet.chunk_hash);
                        client->files[j].nr_chunks++;
                        } else {
                            // Se trimite un mesaj de eroare
                            printf("Eroare la downloadarea chunk-ului %s de la clientul %d\n", cl->chunks[k], cl->rank);
                        }
                    }
                }
            } else {
                add_new_file_to_client(rank, i);
                break;
            }
        }
        if (client->nr_files == 0) {
            add_new_file_to_client(rank, i);
        }
        if (rank == 1) {
            printf("Nr total de file-uri este: %d pentru nodul: %d\n", client->nr_files, rank);
        }
    }
    //return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;
    while (1) {
        struct packet_for_peer packet;
        MPI_Status status;
        enum packet_type type = WAIT;
        packet.rank_dest = 0;
        MPI_Recv(&packet, sizeof(struct packet_for_peer), MPI_BYTE, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
        type = packet.type;
        struct packet_for_peer response_packet;
        response_packet.type = RESPONSE;
        MPI_Send(&response_packet, sizeof(struct packet_for_peer), MPI_BYTE, status.MPI_SOURCE, 2, MPI_COMM_WORLD);  
    }  
    //return NULL;
}

// Funcție de parsare pentru pachete de tipul INIT
void parseInitPacket(struct packet_for_tracker *packet) {
    int index;
    bool file_was_added = false;
    if (init_database) {
        database = (struct database_tracker *)malloc(sizeof(struct database_tracker));
        printf("Am intrat in if\n");
        database->nr_files = packet->nr_files;
        database->files = (struct file_tracker *)malloc(database->nr_files * sizeof(struct file_tracker));
        printf("Size ul database-ului este: %d\n", database->nr_files);
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
                printf("Added client %d to existing file %s\n", database->files[k].clients[index].rank, packet->files[i].name);
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
            printf("Added new file %s with client %d\n", database->files[index].name, database->files[index].clients[0].rank);
        }
    }
    init_database = false;
}

void tracker(int numtasks, int rank) {
    int num_clients = numtasks - 1;
    for (int i = 1; i < numtasks; i++) {
        struct packet_for_tracker packet;
        MPI_Recv(&packet, sizeof(struct packet_for_tracker), MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // Procesează structura primită de la nodul cu rankul i
        printf("Received packet from rank %d\n", i);
        if (packet.type == INIT) {
            printf("Rank: %d, nr_files: %d\n", packet.rank, packet.nr_files);
            packet.files = (struct file *)malloc(packet.nr_files * sizeof(struct file));
            for (int j = 0; j < packet.nr_files; j++) {
                // Procesează structura file
                receiveFile(&packet.files[j], i);
            }
            parseInitPacket(&packet);
        }
    }
    printf("Am primit toate pachetele de la clienti\n"); // --> se trimite mesaj bdcast ca se poate incepe descarcarea
    printf("\n");
    // Se trimite mesaj broadcast cilentilor ca se poate incepe descarcarea
    char message[3] = "OK";
    MPI_Bcast(message, 3, MPI_CHAR, 0, MPI_COMM_WORLD);
    printf("Am trimis mesajul de broadcast: %s\n", message);
    printf("\n");
    print_database();
    printf("\n");

    // Se trimit listele de clienti care au nevoie de un anumit fisier
    for (int i = 1; i < numtasks; i++) {
        struct packet_for_tracker packet;
        MPI_Recv(&packet, sizeof(struct packet_for_tracker), MPI_BYTE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (packet.type == REQUEST) {
            MPI_Recv(&packet.needed_files, packet.nr_needed_files * MAX_FILENAME, MPI_CHAR, packet.rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("Received request from rank %d for %d files\n", packet.rank, packet.nr_needed_files);
            send_list_of_clients(&packet);
        }
    }
    MPI_Bcast(message, 3, MPI_CHAR, 0, MPI_COMM_WORLD); // anunta celelalte noduri ca s-au trimis listele

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
    printf("Received message from tracker: %s\n", message);

    // Se cere lista de fisiere de la tracker
    request_list_of_files(rank);
    //Se primeste lista de file-uri de la tracker
    for (int i = 0; i < client->nr_needed_files; i++) {
        MPI_Recv(&client->files_list[i], sizeof(struct file_tracker), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Nodul %d a primit fisierul %s de la tracker\n", rank, client->files_list[i].name);
        printf("Numarul total de chunkuri este: %d\n", client->files_list[i].num_total_chunks);
        // cand se primeste lista de file-uri toate sunt incomplete
        client->files_list[i].status = INCOMPLETE;
        printf("FIle-ul este: %d\n", client->files_list[i].status);
        for (int j = 0; j < client->files_list->num_clients; j++) {
            client->files_list[i].clients = (struct client_tracker *)malloc(client->files_list[i].num_clients * sizeof(struct client_tracker));
            receiveClient(&client->files_list[i].clients[j], 0);
        }
    }
    MPI_Bcast(message, 3, MPI_CHAR, 0, MPI_COMM_WORLD);
    printf("\n");
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
    print_client();
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
