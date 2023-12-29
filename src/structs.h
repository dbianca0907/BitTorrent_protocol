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
    INIT
};

enum client_type {
    SEED,
    PEER,
    LEECHER
};

struct request {
    int rank;
    char needed_segments[MAX_CHUNKS][HASH_SIZE];
};

struct file {
    char name[MAX_FILENAME];
    int nr_chunks;
    char chunks[MAX_CHUNKS][HASH_SIZE];
    int total_needed_chunks;
    struct request *requests;
    enum status status;
};

struct client{
    int rank;
    int nr_files;
    struct file *files; // part of files I already have
    int nr_needed_files;
    char needed_files[MAX_FILES][MAX_FILENAME];
};

struct packet_for_tracker {
    enum packet_type type;
    int rank;
    int nr_files;
    struct file *files;
};

struct client_tracker{
    int rank;
    char segments[MAX_CHUNKS][HASH_SIZE];
    enum client_type type;
    enum status status;
};

struct file_tracker {
    char *name;
    struct client_tracker *clients;
};

struct tracker_database {
    int nr_files;
    struct file_tracker *files;
};
