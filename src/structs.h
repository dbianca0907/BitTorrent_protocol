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
    INCOMPLETE,
    DOWNLOADED,
};

enum packet_type {
    REQUEST,
    ACK,
    INIT,
    UPDATE,
    SHUTDOWN,
};

struct file {
    char name[MAX_FILENAME];
    int nr_chunks;
    char chunks[MAX_CHUNKS][HASH_SIZE + 2];
    enum status status;
};

struct client{
    int rank;
    int nr_files;
    int downloaded_files;
    struct file *files; // part of files I already have
    int nr_needed_files;
    char needed_files[MAX_FILES][MAX_FILENAME];
    struct file_tracker *files_list; // list received from the tracker
};

struct packet_for_peer {
    enum packet_type type;
    char chunk_hash[HASH_SIZE + 2];
    char file_name[MAX_FILENAME];
};

struct packet_for_tracker {
    enum packet_type type;
    int rank;
    int nr_files;
    int downloaded_files;
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
    int files_downloaded_by_clients[8];
    int num_finished_clients;
};
