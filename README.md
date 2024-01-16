# Dumitru Bianca-Andreea, 332CA, tema3

## Organizarea structurilor de date

* fiecare client are o structura de tip _struct client_ care contine: rank-ul clientului, numarul de file-uri pe care le are de la inceput (*nr_files*), un counter care retine numarul de file-uri care au fost descarcate (*downloaded_files*), un vectori de structuri de tipul *struct file* carecontine file-urile pe care le are nodul de la bun inceput (*files*), numarul de file-uri care trebuiesc descarcate cat si un vector cu numele lor (*nr_needed_files* si *needed_files*) si un *files_list* unde se retin lista de file-uri primita de la tracker.

* trackerul are *database_tracker*, unde fiecare file din vectorul files, care este de tipul file_tracker, contine: numele file-ului, numarul total de clienti care au segmente din file-ul respectiv (*num_clients*), numarul total de segmente (*num_total_chunks*), status-ul file-ului si un vector de clienti ce stocheaza informatii despre fiecare client ce are segmenete din file-ul respectiv. 

## Implementarea protocolului

### In perioada de init:

* se initializeaza fiecare client folosind functia __init_client()__, si se trimit pachete trackerului pentru a se inregistra file-urile detinute de fiecare client.
* pentru a se realiza sincronizarea clientilor, acestia trebuie sa astepte un mesaj de broadcast de la tracker, care este trimis dupa ce toti clientii au trimis pachetele de INIT.

### In perioada de descarcare:

* se initializeaza files_list-ul fiecarui client, prin functiile: __request_list_of_files(rank client, tag)__ si __init_list_of_files_from_tracker(rank)__
* index_client retine indexul clientului la care a ramas sa se descarce segmentele, iar index_chunk este indexul segmentului de la care se continua descarcarea.
* dupa ce a fost primita lista  cu clientii de la care se va realiza descarcarea, se adauga in vectorul *files* al clientului file-ul ce urmeaza sa fie descarcat. (__add_new_file_to_client()__)
* dupa primele 10 segmente descarcate (chunk_counter == 0) se iese din for-uri si se reia descarcarea, prima oara se verifica daca sunt file-uri care trebuiesc descarcate, dupa se trimite un pachet tracker-ului pentru a-si actualiza database-ul; in urma modificarii a fisierelor din tracker, clientul primeste files_list-ul actualizat si se reia descarcarea.
* in tracker, update-ul se produce prin functiile __update_database()__ si __update_swarn_file()__, unde pentru fiecare file se actualizeaza segmentele detinute de fiecare client.

### Finalizarea descarcarilor:

* la fiecare descarcare a unui file este trimis dupa un pachet de update pentru a informa tracker-ul.
* la fiecare update tracker-ul verifica daca numarul de file-uri descarcate ale clientului (__packet->downloaded_files__) este la fel cu numarul de file-uri pe care trebuie sa le descarce (__packet->nr_needed_files__), in cazul in care acesta este egal inseamna ca clientul a terminat de descarcat si isi poate inchide thread-ul de download. De asemenea, in acest caz se incrementeaza __database->num_finished_clients__ ce retine numarul de clienti care si-au terminat descarcarea.
* thread-ul download este inchis in client, atunci cand toate fisierele din lista de file-uri primite de la tracker au statusul DOWNLOAD.
* cand numarul de clienti care si-au terminat descarcarea este egal cu numarul total de clienti, tracker-ul trimite un pachet de tip SHUTDOWN catre toti clientii pentru a se inchide threadul de upload.

Nu s-a realizat implementarea eficienta, prin schimbul intre clienti.