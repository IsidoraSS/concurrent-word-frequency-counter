#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>

#define HM_SIZE 10000
#define MAX_FILES 50
#define WORD_LENGTH 64


pthread_mutex_t slotMutexes[HM_SIZE]; //thread key for every slot in hash map (when new element is added)
pthread_t threads[MAX_FILES];

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

static unsigned long hash_key(const char* key) { //hash function (found on the internet)
    unsigned long hash = FNV_OFFSET;
    for (const char* p = key; *p; p++) {
        hash ^= (unsigned long)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash % HM_SIZE;
}

typedef struct hm_object //element in hash_map structure
{
	char* key; //word
	int value; //frequency
    struct hm_object* next; //next element if there is collision
} hm_object;



//hashmap code from the internet, changed so it fits the needs
typedef struct hm{
    hm_object** elements;
} hm;

hm_object *hm_object_create(const char *key){
    hm_object *element=malloc(sizeof(hm_object));
    element->key=malloc(strlen(key)+1);

    strcpy(element->key,key);
    element->value=1;

    element->next=NULL;
    return element;
}

hm *hm_make(void){
    hm *hashmap=malloc(sizeof(hm));

    hashmap->elements=malloc(sizeof(hm_object)*HM_SIZE);

    for(int i=0;i<HM_SIZE;i++){
        hashmap->elements[i]=NULL; 
        pthread_mutex_init(&slotMutexes[i], NULL);
    }

    return hashmap;
}

void hm_increment(hm *hashmap, const char *key){
    long index=hash_key(key); 

    pthread_mutex_lock(&slotMutexes[index]); //we lock the entire slot in case a new element is added
    hm_object *element=hashmap->elements[index];

    if(element==NULL){ //if it's a new element we add it to the map
        hashmap->elements[index]=hm_object_create(key);
        pthread_mutex_unlock(&slotMutexes[index]);
        return;
    }

    hm_object *previous;
    while(element!=NULL){ //if it's the one we are looking for or if it's in a collision list
        if(strcmp(element->key,key)==0){
            element->value++;
            pthread_mutex_unlock(&slotMutexes[index]);
            return;
        }
        previous=element;
        element=previous->next;
    }

    previous->next=hm_object_create(key); //if we need to add it to the collision list
    pthread_mutex_unlock(&slotMutexes[index]);
}

int *hm_get(hm *hashmap, const char *key) {
    long index=hash_key(key);

    pthread_mutex_lock(&slotMutexes[index]);
    hm_object *element = hashmap->elements[index];

    if (element == NULL) {
        pthread_mutex_unlock(&slotMutexes[index]);
        return NULL;
    }

    while (element != NULL) {
        if (strcmp(element->key, key) == 0) {
            pthread_mutex_unlock(&slotMutexes[index]);
            return &(element->value);
        }

        element = element->next;
    }

    pthread_mutex_unlock(&slotMutexes[index]);
    return NULL;
}

char *lastNonNullWord(int length, char reci[][length]) { //function returns the last found word
    char *lastNonNull = NULL;
    for (int j = length - 1; j >= 0; j--) {
        if (reci[j][0] != '\0') {
            lastNonNull = reci[j];
            break;
        }
    }
    int eol = strcspn(lastNonNull, "\n");
    lastNonNull[eol] = '\0';
    return lastNonNull;
}

int isFileRead(char *filePath, char files[MAX_FILES][256], int nOfScannedFiles){ //function checks if the file is in the array
    for(int i=0;i<nOfScannedFiles;i++){
        if(strcmp(files[i],filePath)==0){
            return 1;
        }
    }
    return 0;
}

char* toLower(char *s) { 
  for(char *p=s; *p; p++) *p=tolower(*p);
  return s;
}

int isAlphaStr(const char* str) {
    while (*str != '\0') {
        if (!isalpha(*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

void truncStr(char* word) {
    word[WORD_LENGTH] = '\0';
}

hm *hashmap;

void* scanner(void *args) //thread function
{
    char *filePath= (char *) args;
    int lastPosition = 0;
    time_t lastModification = 0;

    while(1){
        FILE *file=fopen(filePath,"r");

        struct stat metaData;
        fstat(fileno(file), &metaData);
        time_t currentModification = metaData.st_mtime; //gets file's metadata


        if(difftime(lastModification, currentModification)<0){ //checks modification time
            fseek(file, lastPosition, SEEK_SET); //starts from the last position

            char word[WORD_LENGTH];
            while (fscanf(file, "%s", word) == 1){
                if(isAlphaStr(word)==0) continue; //letters only!
                if (strlen(word) > WORD_LENGTH) truncStr(word);//word length!
                //if it's a stop word
                hm_increment(hashmap, toLower(word));
            }
            lastPosition=ftell(file); //we set new last position
        }

        fclose(file);
        lastModification=currentModification;
        sleep(5);
    }
}

int main(int argc, char *argv[])
{
    printf("Current working directory: %s\n", getcwd(NULL, 0));
    hashmap=hm_make();
    char files[MAX_FILES][256];
    int nOfScannedFiles=0;

    if(argc>1){ //we have a file of stop words

    }

    while(1){
        char input[256]={0};
        char words[70][70]={0};
        fgets(input, sizeof(input), stdin);

        char *token;
        int i=0;
        token = strtok(input, " ");
        while( token != NULL ) {
            strcpy(words[i],token);
            i++;
            token = strtok(NULL, " ");
        }

        if(strcmp(words[0],"_count_") == 0){
            int eol = strcspn(words[1], "\n");
            words[1][eol] = '\0'; 
            if(isFileRead(words[1], files, nOfScannedFiles)){ //checks if the file is in the array
                printf("Fajl je vec obradjen\n"); //if it is, notify
            }
            else{
                FILE *f=fopen(words[1],"r");
                if(f==NULL){
                    printf("Fajl ne postoji\n");
                    continue;
                }
                fclose(f);

                strcpy(files[nOfScannedFiles], words[1]); //if it's not in the array we add it
                pthread_create(&threads[nOfScannedFiles],NULL,scanner,files[nOfScannedFiles]); //and we create a new thread for it
                nOfScannedFiles++;
            }
        }
        else if(strcmp(words[0],"_stop_") == 0){
            return 0;
        }
        else{
            char *word=lastNonNullWord(70, words);
            int *result = hm_get(hashmap, word);
            if (result != NULL) {
                printf("Value for key '%s': %d\n", word, *result);
            } else {
                printf("Value for key '%s': 0\n", word);
            }
        }
    }

    return 0;
}
