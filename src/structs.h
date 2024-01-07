#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

enum status {
    COMPLETE,
    INCOMPLETE
};

enum packet_type {
    REQUEST,
    RESPONSE,
    INIT,
    WAIT
};

enum client_type {
    SEED,
    PEER,
    LEECHER
};

struct request {
    int rank;
    char needed_segments[MAX_CHUNKS][HASH_SIZE + 2];
};

struct file {
    char name[MAX_FILENAME];
    int nr_chunks;
    char chunks[MAX_CHUNKS][HASH_SIZE + 2];
    int total_needed_chunks;
    struct request *requests;
    enum status status;
};

struct client{
    int rank;
    int nr_files;
    struct file *files; // part of files I already have
    struct file *files_to_download; // part of files I need to download
    int nr_needed_files;
    char needed_files[MAX_FILES][MAX_FILENAME];
    struct file_tracker *files_list; // list received from the tracker
};

struct packet_for_peer {
    enum packet_type type;
    int rank_dest;
    char message[2];
    char chunk_hash[HASH_SIZE + 2];
    char file_name[MAX_FILENAME];
};

struct packet_for_tracker {
    enum packet_type type;
    int rank;
    int nr_files;
    struct file *files;
    int nr_needed_files;
    char needed_files[MAX_FILES][MAX_FILENAME];
};

struct client_tracker{
    int rank;
    int nr_chunks;
    char chunks[MAX_CHUNKS][HASH_SIZE + 2];
    enum client_type type;
};

struct file_tracker {
    char name[MAX_FILENAME];
    int num_clients;
    int num_total_chunks;
    enum status status;
    struct client_tracker *clients;
};

struct database_tracker {
    int nr_files;
    struct file_tracker *files;
};
