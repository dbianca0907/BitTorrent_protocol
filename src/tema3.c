#include <mpi.h>
#include <pthread.h>
#include "structs.h"

bool mpi_state = false;
bool init_database = true;
struct client *client;
struct database_tracker *database;

// Funcție pentru a trimite o structură file
void sendFile(struct file *f, int dest) {
    MPI_Send(f, sizeof(struct file), MPI_BYTE, dest, 0, MPI_COMM_WORLD);

    // Trimiterea matricei chunks
    MPI_Send(f->chunks, f->nr_chunks * HASH_SIZE, MPI_CHAR, dest, 0, MPI_COMM_WORLD);
}

// Funcție pentru a primi o structură file
void receiveFile(struct file *f, int source) {
    MPI_Recv(f, sizeof(struct file), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Primirea matricei chunks
    MPI_Recv(f->chunks, f->nr_chunks * HASH_SIZE, MPI_CHAR, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void sendClient(struct client_tracker *c, int dest) {
    MPI_Send(c, sizeof(struct client_tracker), MPI_BYTE, dest, 0, MPI_COMM_WORLD);

    // Trimiterea matricei chunks
    MPI_Send(c->chunks, c->nr_chunks * HASH_SIZE, MPI_CHAR, dest, 0, MPI_COMM_WORLD);
}

void receiveClient(struct client_tracker *c, int source) {
    MPI_Recv(c, sizeof(struct client_tracker), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Primirea matricei chunks
    MPI_Recv(c->chunks, c->nr_chunks * HASH_SIZE, MPI_CHAR, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void send_list_of_clients(struct packet_for_tracker *packet) {
    //Search in the database for the files and send the list to the client
    for (int i = 0; i < packet->nr_needed_files; i++) {
        for (int j = 0; j < database->nr_files; j++) {
            printf("Se compara: %s cu %s\n", packet->needed_files[i], database->files[j].name);
            if (strcmp(packet->needed_files[i], database->files[j].name) == 0) {
                MPI_Send(&database->files[j], sizeof(struct file_tracker), MPI_BYTE, packet->rank, 0, MPI_COMM_WORLD);
                printf("TRIMITE PACHETUL PENTRU: %d cu sizeul: %d\n", packet->rank, sizeof(struct file_tracker));  
                //printf("Am trimis fisierul %s la clientul %d\n", database->files[j].name, packet->rank);
                for (int k = 0; k < database->files[j].num_clients; k++) {
                   // printf("SE TRIMITE CLIENTUL %d\n", database->files[j].clients[k].rank);
                    sendClient(&database->files[j].clients[k], packet->rank);
                    //printf("Am trimis clientul %d la clientul %d\n", database->files[j].clients[k].rank, packet->rank);
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

void print_database() {
    printf("DATABASE:\n");
    for (int i = 0; i < database->nr_files; i++) {
        printf("File %d: %s\n", i, database->files[i].name);
        printf("Nr clients: %d\n", database->files[i].num_clients);
        for (int j = 0; j < database->files[i].num_clients; j++) {
            printf("Clientul %d are %d chunks\n", database->files[i].clients[j].rank, database->files[i].clients[j].nr_chunks);
        }
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

    //Alloc memory for client and for the packet to the tracker
    client = (struct client*) malloc(sizeof(struct client));
    struct packet_for_tracker *packet = (struct packet_for_tracker*) malloc(sizeof(struct packet_for_tracker));
    client->rank = rank;
    fscanf(fisier, "%d", &client->nr_files);
    client->files = (struct file*) malloc(client->nr_files * sizeof(struct file));

    packet->nr_files = client->nr_files;
    packet->files = (struct file*) malloc(client->nr_files * sizeof(struct file));

    for (int i = 0; i < client->nr_files; i++) {
        fscanf(fisier, "%s", client->files[i].name);
        strcpy(packet->files[i].name, client->files[i].name);
        fscanf(fisier, "%d", &client->files[i].nr_chunks);
        packet->files[i].nr_chunks = client->files[i].nr_chunks;
        for (int j = 0; j < client->files[i].nr_chunks; j++) {
            fscanf(fisier, "%s", client->files[i].chunks[j]);
            strcpy(packet->files[i].chunks[j], client->files[i].chunks[j]);
        }
    }
    fscanf(fisier, "%d", &client->nr_needed_files);
    for (int i = 0; i < client->nr_needed_files; i++) {
        fscanf(fisier, "%s", client->needed_files[i]);
    }
    // Se aloca memorie pentru lista de fisiere primita de la tracker
    client->files_list = (struct file_tracker*) malloc(client->nr_needed_files * sizeof(struct file_tracker));

    packet->type = INIT;
    packet->rank = client->rank;

    MPI_Send(packet, sizeof(struct packet_for_tracker), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    for (int j = 0; j < packet->nr_files; j++) {
        sendFile(&packet->files[j], 0);
    }
    
    fclose(fisier);
}

void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;

    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;


    return NULL;
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
            printf("Added new file %s with client %d\n", database->files[index].name, database->files[index].clients[0].rank);
        }
    }
    init_database = false;
}

void tracker(int numtasks, int rank) {
    int num_clients = numtasks - 1;
    for (int i = 1; i < num_clients; i++) {
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
    // Se așteapta mesaje de la clienti
    printf("\n");
    print_database();
    printf("\n");
    while (1) {
        struct packet_for_tracker packet;
        MPI_Recv(&packet, sizeof(struct packet_for_tracker), MPI_BYTE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (packet.type == REQUEST) {
            MPI_Recv(&packet.needed_files, packet.nr_needed_files * MAX_FILENAME, MPI_CHAR, packet.rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("Received request from rank %d for %d files\n", packet.rank, packet.nr_needed_files);
            send_list_of_clients(&packet);
        }
    }
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
    //Se primesc file-urile de la tracker
    for (int i = 0; i < client->nr_needed_files; i++) {
        
        MPI_Recv(&client->files_list[i], sizeof(struct file_tracker), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("NODUL %d AM PRIMIT file %s from tracker\n",rank, client->needed_files[i]);
        for (int j = 0; j < client->nr_needed_files; j++) {
            client->files_list[i].clients = (struct client_tracker *)malloc(client->files_list[i].num_clients * sizeof(struct client_tracker));
            receiveClient(&client->files_list[i].clients[j], 0);
            printf("NODUL %d Receive client: %d\n",rank, client->files_list[i].clients[j].rank);
        }
    }
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
