#include <mpi.h>
#include <pthread.h>
#include "structs.h"

bool mpi_state = false;
struct client *client;

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

    //Alloc memory for client
    client = (struct client*) malloc(sizeof(struct client));
    client->rank = rank;
    // MPI_Send(&client->rank, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD); // send the rank
    fscanf(fisier, "%d", &client->nr_files);
    // MPI_Send(&client->nr_files, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD); // send the nr_files
    client->files = (struct file*) malloc(client->nr_files * sizeof(struct file));
    for (int i = 0; i < client->nr_files; i++) {
        fscanf(fisier, "%s", client->files[i].name);
        // MPI_Send(client->files[i].name, sizeof(client->files[i].name), MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD); // send the file name
        fscanf(fisier, "%d", &client->files[i].nr_chunks);
        // MPI_Send(&client->files[i].nr_chunks, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
        for (int j = 0; j < client->files[i].nr_chunks; j++) {
            fscanf(fisier, "%s", client->files[i].chunks[j]);
            // send the chunks
            // MPI_Send(client->files[i].chunks[j], sizeof(client->files[i].chunks[j]), MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
        }
    }
    fscanf(fisier, "%d", &client->nr_needed_files);
    for (int i = 0; i < client->nr_needed_files; i++) {
        fscanf(fisier, "%s", client->needed_files[i]);
    }

    struct packet_for_tracker *packet = (struct packet_for_tracker*) malloc(sizeof(struct packet_for_tracker));
    packet->type = INIT;
    packet->nr_files = client->nr_files;
    packet->files = client->files;
    packet->rank = client->rank;
    //MPI_Send(packet, sizeof(struct packet_for_tracker), MPI_BYTE, TRACKER_RANK, 0, MPI_COMM_WORLD);
    // Trimite întregul pachet, inclusiv numele și chunk-urile
    //Send nr files and nr_chunks
    MPI_Send(&packet->type, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
    MPI_Send(&packet->rank, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
    MPI_Send(&packet->nr_files, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
    for (int i = 0; i < client->nr_files; i++) {
        MPI_Send(client->files[i].name, strlen(client->files[i].name) + 1, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
        printf("Am trimis numele fisierului %s\n", client->files[i].name);
        MPI_Send(&client->files[i].nr_chunks, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
        for (int j = 0; j < client->files[i].nr_chunks; j++) {
            MPI_Send(client->files[i].chunks[j], strlen(client->files[i].chunks[j]) + 1, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
        }
    }

    fclose(fisier);
}

// MAke a function that prints the client struct
void print_client() {
    printf("Rank: %d\n", client->rank);
    printf("Nr files: %d\n", client->nr_files);
    for (int i = 0; i < client->nr_files; i++) {
        printf("File name: %s\n", client->files[i].name);
        printf("Nr chunks: %d\n", client->files[i].nr_chunks);
        // for (int j = 0; j < client->files[i].nr_chunks; j++) {
        //     printf("Chunk: %s\n", client->files[i].chunks[j]);
        // }
    }
    printf("Nr needed files: %d\n", client->nr_needed_files);
    for (int i = 0; i < client->nr_needed_files; i++) {
        printf("Needed file: %s\n", client->needed_files[i]);
    }
    printf("\n");
    printf("\n");
}

//Make a function to print the packet_for_tracker struct
void print_packet_for_tracker(struct packet_for_tracker *packet) {
    printf("Nr files: %d\n", packet->nr_files);
    for (int i = 0; i < packet->nr_files; i++) {
        printf("File name: %s\n", packet->files[i].name);
        printf("Nr chunks: %d\n", packet->files[i].nr_chunks);
        // for (int j = 0; j < packet->files[i].nr_chunks; j++) {
        //     printf("Chunk: %s\n", packet->files[i].chunks[j]);
        // }
    }
    printf("Rank: %d\n", packet->rank);
    printf("\n");
    printf("\n");
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

void tracker(int numtasks, int rank) {
    struct packet_for_tracker *packet = (struct packet_for_tracker*) malloc(sizeof(struct packet_for_tracker));
    MPI_Status status;

    int num_clients = numtasks - 1;
    int cnt = 0;
    while (true) {
        //MPI_Recv(packet, sizeof(struct packet_for_tracker), MPI_BYTE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        // Receive the type of the packet
        MPI_Recv(&packet->type, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        // Receive the rank of the client
        MPI_Recv(&packet->rank, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        // Receive the number of files
        MPI_Recv(&packet->nr_files, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        packet->files = (struct file*) malloc(packet->nr_files * sizeof(struct file));
        for (int i = 0; i < packet->nr_files; i++) {
            printf("INCEP SA PRIMESC FILE URI\n");
            MPI_Recv(packet->files[i].name, 6, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            
            printf("Nume fisier: %s\n", packet->files[i].name);
            MPI_Recv(&packet->files[i].nr_chunks, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            printf("Nr chunks: %d\n", packet->files[i].nr_chunks);
            for (int j = 0; j < packet->files[i].nr_chunks; j++) {
                //packet->files[i].chunks[j] = (char*) malloc(MAX_CHUNK_SIZE); // MAX_CHUNK_SIZE ar trebui să fie suficient pentru dimensiunea maximă a unui chunk
                MPI_Recv(packet->files[i].chunks[j], HASH_SIZE, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            }
        }
        if (packet->nr_files > 0) {
            printf("NUME: %s\n", packet->files[0].name);
        }
        if (packet->type == INIT) {
            printf("Am primit un pachet de la clientul cu rank-ul %d\n", packet->rank);
            cnt++;
        }
        if (cnt == num_clients) {
            break;
        }
    }
    printf("Am primit toate pachetele de la clienti\n");
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;
    if (!mpi_state) {
        init_client(rank);
        print_client();
        mpi_state = true;
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
