#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

// =======================
// Input helpers
// =======================
static void input_read_line(char* buf, size_t sz) {
    if (!fgets(buf, (int)sz, stdin)) {
        buf[0] = '\0';
        return;
    }
    buf[strcspn(buf, "\n")] = '\0';
}
static int input_read_int(int* out) {
    char tmp[64];
    input_read_line(tmp, sizeof(tmp));
    return sscanf(tmp, "%d", out) == 1;
}
static int input_read_double(double* out) {
    char tmp[64];
    input_read_line(tmp, sizeof(tmp));
    return sscanf(tmp, "%lf", out) == 1;
}

// =======================
// Forward declarations for AVL functions & types
// =======================
typedef struct AVL_Node AVL_Node;
static AVL_Node* avl_insert(AVL_Node* node, int key, void* data);
static AVL_Node* avl_search(AVL_Node* root, int key);
static AVL_Node* avl_delete(AVL_Node* root, int key);
static void avl_inorder(AVL_Node* root, void (*cb)(void*));
static int avl_count_nodes(AVL_Node* root);

// =======================
// AVL Node struct & implementation
// =======================
struct AVL_Node {
    int key;
    void* data;
    int height;
    AVL_Node* left;
    AVL_Node* right;
};

static int avl_height(AVL_Node* n) { return n ? n->height : 0; }
static int avl_max(int a, int b) { return a > b ? a : b; }

static AVL_Node* avl_right_rotate(AVL_Node* y) {
    AVL_Node* x = y->left;
    AVL_Node* T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = 1 + avl_max(avl_height(y->left), avl_height(y->right));
    x->height = 1 + avl_max(avl_height(x->left), avl_height(x->right));
    return x;
}

static AVL_Node* avl_left_rotate(AVL_Node* x) {
    AVL_Node* y = x->right;
    AVL_Node* T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = 1 + avl_max(avl_height(x->left), avl_height(x->right));
    y->height = 1 + avl_max(avl_height(y->left), avl_height(y->right));
    return y;
}

static int avl_get_balance(AVL_Node* n) { return n ? avl_height(n->left) - avl_height(n->right) : 0; }

static AVL_Node* avl_new_node(int key, void* data) {
    AVL_Node* n = (AVL_Node*)malloc(sizeof(AVL_Node));
    if (!n) return NULL;
    n->key = key;
    n->data = data;
    n->height = 1;
    n->left = n->right = NULL;
    return n;
}

static AVL_Node* avl_insert(AVL_Node* node, int key, void* data) {
    if (!node) return avl_new_node(key, data);
    if (key < node->key) node->left = avl_insert(node->left, key, data);
    else if (key > node->key) node->right = avl_insert(node->right, key, data);
    else { node->data = data; return node; }
    node->height = 1 + avl_max(avl_height(node->left), avl_height(node->right));
    int balance = avl_get_balance(node);
    if (balance > 1 && key < node->left->key) return avl_right_rotate(node);
    if (balance < -1 && key > node->right->key) return avl_left_rotate(node);
    if (balance > 1 && key > node->left->key) { node->left = avl_left_rotate(node->left); return avl_right_rotate(node); }
    if (balance < -1 && key < node->right->key) { node->right = avl_right_rotate(node->right); return avl_left_rotate(node); }
    return node;
}

static AVL_Node* avl_search(AVL_Node* root, int key) {
    if (!root) return NULL;
    if (root->key == key) return root;
    if (key < root->key) return avl_search(root->left, key);
    return avl_search(root->right, key);
}

static AVL_Node* avl_min_value_node(AVL_Node* node) {
    AVL_Node* current = node;
    while (current && current->left) current = current->left;
    return current;
}

static AVL_Node* avl_delete(AVL_Node* root, int key) {
    if (!root) return root;
    if (key < root->key) root->left = avl_delete(root->left, key);
    else if (key > root->key) root->right = avl_delete(root->right, key);
    else {
        if (!root->left || !root->right) {
            AVL_Node* temp = root->left ? root->left : root->right;
            if (!temp) { temp = root; root = NULL; }
            else *root = *temp;
            free(temp);
        } else {
            AVL_Node* temp = avl_min_value_node(root->right);
            root->key = temp->key;
            root->data = temp->data;
            root->right = avl_delete(root->right, temp->key);
        }
    }
    if (!root) return root;
    root->height = 1 + avl_max(avl_height(root->left), avl_height(root->right));
    int balance = avl_get_balance(root);
    if (balance > 1 && avl_get_balance(root->left) >= 0) return avl_right_rotate(root);
    if (balance > 1 && avl_get_balance(root->left) < 0) { root->left = avl_left_rotate(root->left); return avl_right_rotate(root); }
    if (balance < -1 && avl_get_balance(root->right) <= 0) return avl_left_rotate(root);
    if (balance < -1 && avl_get_balance(root->right) > 0) { root->right = avl_right_rotate(root->right); return avl_left_rotate(root); }
    return root;
}

static void avl_inorder(AVL_Node* root, void (*cb)(void*)) {
    if (!root) return;
    avl_inorder(root->left, cb);
    if (cb && root->data) cb(root->data);
    avl_inorder(root->right, cb);
}

static int avl_count_nodes(AVL_Node* root) {
    if (!root) return 0;
    return 1 + avl_count_nodes(root->left) + avl_count_nodes(root->right);
}

// =======================
// AVLChain & HashMap_Chain
// =======================
typedef struct AVLChain {
    int owner_id;
    AVL_Node* tree;
    struct AVLChain* next;
} AVLChain;

typedef struct HashMap_Chain {
    int size;
    int mode; // 0 = top-level (AVL per bucket), 1 = chained (AVLChain per bucket)
    void** buckets; // when mode=0 -> AVL_Node*; when mode=1 -> AVLChain*
} HashMap_Chain;

static HashMap_Chain* create_HashMap_Chain(int size, int mode) {
    HashMap_Chain* hm = (HashMap_Chain*)malloc(sizeof(HashMap_Chain));
    if (!hm) return NULL;
    hm->size = size;
    hm->mode = mode;
    hm->buckets = calloc(size, sizeof(void*));
    return hm;
}
static int hm_hash(int key, int size) { return abs(key) % size; }

/* Top-level insert/get for mode=0 */
static void hms_insert_top(HashMap_Chain* map, int key, void* data) {
    if (!map || map->mode != 0) return;
    int idx = hm_hash(key, map->size);
    AVL_Node* root = (AVL_Node*)map->buckets[idx];
    root = avl_insert(root, key, data);
    map->buckets[idx] = root;
}
static void* hms_get_top(HashMap_Chain* map, int key) {
    if (!map || map->mode != 0) return NULL;
    int idx = hm_hash(key, map->size);
    AVL_Node* root = (AVL_Node*)map->buckets[idx];
    AVL_Node* n = avl_search(root, key);
    return n ? n->data : NULL;
}
static int hms_count_all(HashMap_Chain* map) {
    if (!map || map->mode != 0) return 0;
    int total = 0;
    for (int i = 0; i < map->size; ++i) total += avl_count_nodes((AVL_Node*)map->buckets[i]);
    return total;
}
static void hms_traverse_all(HashMap_Chain* map, void (*cb)(void*)) {
    if (!map || map->mode != 0) return;
    for (int i = 0; i < map->size; ++i) avl_inorder((AVL_Node*)map->buckets[i], cb);
}

/* Chain helpers for mode=1 */
static AVLChain* avlchain_find(AVLChain* head, int owner_id) {
    AVLChain* cur = head;
    while (cur) { if (cur->owner_id == owner_id) return cur; cur = cur->next; }
    return NULL;
}
static AVLChain* avlchain_create_prepend(HashMap_Chain* map, int idx, int owner_id) {
    AVLChain* node = (AVLChain*)malloc(sizeof(AVLChain));
    if (!node) return NULL;
    node->owner_id = owner_id;
    node->tree = NULL;
    node->next = (AVLChain*)map->buckets[idx];
    map->buckets[idx] = node;
    return node;
}
static void hmchain_insert_record(HashMap_Chain* map, int key, void* data, int owner_id) {
    if (!map || map->mode != 1) return;
    int idx = hm_hash(key, map->size);
    AVLChain* head = (AVLChain*)map->buckets[idx];
    AVLChain* chain = avlchain_find(head, owner_id);
    if (!chain) chain = avlchain_create_prepend(map, idx, owner_id);
    chain->tree = avl_insert(chain->tree, key, data);
}
static void* hmchain_get_record(HashMap_Chain* map, int key, int owner_id) {
    if (!map || map->mode != 1) return NULL;
    int idx = hm_hash(key, map->size);
    AVLChain* head = (AVLChain*)map->buckets[idx];
    AVLChain* chain = avlchain_find(head, owner_id);
    if (!chain) return NULL;
    AVL_Node* n = avl_search(chain->tree, key);
    return n ? n->data : NULL;
}
static void hmchain_delete_record(HashMap_Chain* map, int key, int owner_id) {
    if (!map || map->mode != 1) return;
    int idx = hm_hash(key, map->size);
    AVLChain* head = (AVLChain*)map->buckets[idx];
    AVLChain* chain = avlchain_find(head, owner_id);
    if (!chain) return;
    chain->tree = avl_delete(chain->tree, key);
}
static void hmchain_traverse_owner(HashMap_Chain* map, int owner_id, void (*cb)(void*)) {
    if (!map || map->mode != 1 || !cb) return;
    for (int i = 0; i < map->size; ++i) {
        AVLChain* cur = (AVLChain*)map->buckets[i];
        while (cur) {
            if (cur->owner_id == owner_id) avl_inorder(cur->tree, cb);
            cur = cur->next;
        }
    }
}
static int hmchain_count_owner(HashMap_Chain* map, int owner_id) {
    if (!map || map->mode != 1) return 0;
    int total = 0;
    for (int i = 0; i < map->size; ++i) {
        AVLChain* cur = (AVLChain*)map->buckets[i];
        while (cur) { if (cur->owner_id == owner_id) total += avl_count_nodes(cur->tree); cur = cur->next; }
    }
    return total;
}

// =======================
// Domain structs
// =======================
typedef struct Member {
    int member_id;
    char name[100];
    char role[50];
    char contact[20];
    char department_name[50];
} Member;

typedef struct Feedback {
    int feedback_id;
    int member_id;
    int rating;
    char comment[256];
} Feedback;

typedef struct Department {
    int dept_id;
    char name[100];
    HashMap_Chain* member_map; // mode=1
    int member_count;
} Department;

typedef struct Event {
    int event_id;
    char name[100];
    char venue[100];
    char date[20];
    double budget;
    double expenses;
    double profit_loss;
    char description[256];
    HashMap_Chain* member_map;   // mode=1 (owner_id = event_id)
    HashMap_Chain* feedback_map; // mode=1 (owner_id = event_id)
    int member_count;
    int feedback_count;
} Event;

// =======================
// Persistable structs
// =======================
typedef struct PersistDepartment { int dept_id; char name[100]; int member_count; } PersistDepartment;

typedef struct PersistEvent {
    int event_id;
    char name[100];
    char venue[100];
    char date[20];
    double budget;
    double expenses;
    double profit_loss;
    char description[256];
    int member_count;
    int feedback_count;
} PersistEvent;

typedef struct PersistFeedback { int event_id; Feedback fb; } PersistFeedback;

// =======================
// System context
// =======================
typedef struct IT_Club_System {
    HashMap_Chain* department_map; // mode=0
    HashMap_Chain* event_map;      // mode=0
    pthread_mutex_t data_lock;
} IT_Club_System;

static IT_Club_System* system_create(void) {
    IT_Club_System* s = (IT_Club_System*)malloc(sizeof(IT_Club_System));
    if (!s) return NULL;
    s->department_map = create_HashMap_Chain(20, 0);
    s->event_map = create_HashMap_Chain(20, 0);
    pthread_mutex_init(&s->data_lock, NULL);
    return s;
}

// =======================
// Create helpers
// =======================
static Department* department_create(int dept_id, const char* name) {
    Department* d = (Department*)malloc(sizeof(Department));
    if (!d) return NULL;
    d->dept_id = dept_id;
    strncpy(d->name, name, sizeof(d->name)-1);
    d->name[sizeof(d->name)-1] = '\0';
    d->member_map = create_HashMap_Chain(10, 1);
    d->member_count = 0;
    return d;
}

static Event* event_create(int event_id, const char* name, const char* venue, const char* date, double budget, const char* desc) {
    Event* e = (Event*)malloc(sizeof(Event));
    if (!e) return NULL;
    e->event_id = event_id;
    strncpy(e->name, name, sizeof(e->name)-1); e->name[sizeof(e->name)-1] = '\0';
    strncpy(e->venue, venue, sizeof(e->venue)-1); e->venue[sizeof(e->venue)-1] = '\0';
    strncpy(e->date, date, sizeof(e->date)-1); e->date[sizeof(e->date)-1] = '\0';
    e->budget = budget;
    e->expenses = 0.0;
    e->profit_loss = budget;
    strncpy(e->description, desc, sizeof(e->description)-1); e->description[sizeof(e->description)-1] = '\0';
    e->member_map = create_HashMap_Chain(10, 1);
    e->feedback_map = create_HashMap_Chain(10, 1);
    e->member_count = 0;
    e->feedback_count = 0;
    return e;
}

// =======================
// Display callbacks
// =======================
static void display_member_cb(void* data) {
    Member* m = (Member*)data;
    if (!m) return;
    printf("  ID:%d | %s | Role:%s | Contact:%s | Dept:%s\n", m->member_id, m->name, m->role, m->contact, m->department_name);
}
static void display_feedback_cb(void* data) {
    Feedback* f = (Feedback*)data;
    if (!f) return;
    printf("  Feedback ID:%d | Member ID:%d | Rating:%d\n    %s\n", f->feedback_id, f->member_id, f->rating, f->comment);
}
static void display_department_cb(void* data) {
    Department* d = (Department*)data;
    if (!d) return;
    printf("\n=== Department ID:%d | %s ===\nMembers: %d\n", d->dept_id, d->name, d->member_count);
}
static void display_event_cb(void* data) {
    Event* e = (Event*)data;
    if (!e) return;
    printf("\n=== Event ID:%d | %s ===\nVenue:%s | Date:%s\nBudget:%.2f | Expenses:%.2f | Profit/Loss:%.2f\nMembers:%d | Feedback:%d\n",
           e->event_id, e->name, e->venue, e->date, e->budget, e->expenses, e->profit_loss, e->member_count, e->feedback_count);
}

// =======================
// Persistence helpers
// =======================
static FILE* persist_fp_global = NULL;
static FILE* persist_feedback_fp = NULL;

static void persist_write_member_cb(void* data) {
    if (!persist_fp_global || !data) return;
    Member* m = (Member*)data;
    fwrite(m, sizeof(Member), 1, persist_fp_global);
}
static void persist_write_feedback_avl(AVL_Node* root, int event_id) {
    if (!root || !persist_feedback_fp) return;
    persist_write_feedback_avl(root->left, event_id);
    Feedback* f = (Feedback*)root->data;
    if (f) {
        PersistFeedback pf;
        pf.event_id = event_id;
        pf.fb = *f;
        fwrite(&pf, sizeof(PersistFeedback), 1, persist_feedback_fp);
    }
    persist_write_feedback_avl(root->right, event_id);
}

static void persist_save_departments(IT_Club_System* sys) {
    FILE* fp = fopen("departments.dat", "wb");
    if (!fp) { printf("[ERR] cannot open departments.dat\n"); return; }
    int count = hms_count_all(sys->department_map);
    fwrite(&count, sizeof(int), 1, fp);
    persist_fp_global = fp;
    for (int i = 0; i < sys->department_map->size; ++i) {
        AVL_Node* root = (AVL_Node*)sys->department_map->buckets[i];
        if (!root) continue;
        void write_dept(void* data) {
            Department* d = (Department*)data;
            if (!d) return;
            PersistDepartment pd;
            pd.dept_id = d->dept_id;
            strncpy(pd.name, d->name, sizeof(pd.name)-1); pd.name[sizeof(pd.name)-1] = '\0';
            pd.member_count = d->member_count;
            fwrite(&pd, sizeof(PersistDepartment), 1, persist_fp_global);
            for (int b = 0; b < d->member_map->size; ++b) {
                AVLChain* cur = (AVLChain*)d->member_map->buckets[b];
                while (cur) {
                    if (cur->owner_id == d->dept_id) avl_inorder(cur->tree, persist_write_member_cb);
                    cur = cur->next;
                }
            }
        }
        avl_inorder(root, write_dept);
    }
    persist_fp_global = NULL;
    fclose(fp);
}

static void persist_save_events_and_feedback(IT_Club_System* sys) {
    FILE* efp = fopen("events.dat", "wb");
    if (!efp) { printf("[ERR] cannot open events.dat\n"); return; }
    int event_count = hms_count_all(sys->event_map);
    fwrite(&event_count, sizeof(int), 1, efp);
    persist_fp_global = efp;
    for (int i = 0; i < sys->event_map->size; ++i) {
        AVL_Node* root = (AVL_Node*)sys->event_map->buckets[i];
        if (!root) continue;
        void write_event(void* data) {
            Event* e = (Event*)data;
            if (!e) return;
            PersistEvent pe;
            pe.event_id = e->event_id;
            strncpy(pe.name, e->name, sizeof(pe.name)-1); pe.name[sizeof(pe.name)-1] = '\0';
            strncpy(pe.venue, e->venue, sizeof(pe.venue)-1); pe.venue[sizeof(pe.venue)-1] = '\0';
            strncpy(pe.date, e->date, sizeof(pe.date)-1); pe.date[sizeof(pe.date)-1] = '\0';
            pe.budget = e->budget;
            pe.expenses = e->expenses;
            pe.profit_loss = e->profit_loss;
            strncpy(pe.description, e->description, sizeof(pe.description)-1); pe.description[sizeof(pe.description)-1] = '\0';
            pe.member_count = e->member_count;
            pe.feedback_count = e->feedback_count;
            fwrite(&pe, sizeof(PersistEvent), 1, persist_fp_global);
            for (int b = 0; b < e->member_map->size; ++b) {
                AVLChain* cur = (AVLChain*)e->member_map->buckets[b];
                while (cur) {
                    if (cur->owner_id == e->event_id) avl_inorder(cur->tree, persist_write_member_cb);
                    cur = cur->next;
                }
            }
        }
        avl_inorder(root, write_event);
    }
    persist_fp_global = NULL;
    fclose(efp);

    FILE* ffp = fopen("feedback.dat", "wb");
    if (!ffp) { printf("[ERR] cannot open feedback.dat\n"); return; }
    int total_feedback = 0;
    for (int i = 0; i < sys->event_map->size; ++i) {
        AVL_Node* root = (AVL_Node*)sys->event_map->buckets[i];
        if (!root) continue;
        void count_fb(void* data) {
            Event* e = (Event*)data;
            if (!e) return;
            total_feedback += e->feedback_count;
        }
        avl_inorder(root, count_fb);
    }
    fwrite(&total_feedback, sizeof(int), 1, ffp);
    persist_feedback_fp = ffp;
    for (int i = 0; i < sys->event_map->size; ++i) {
        AVL_Node* root = (AVL_Node*)sys->event_map->buckets[i];
        if (!root) continue;
        void write_fbs(void* data) {
            Event* e = (Event*)data;
            if (!e) return;
            for (int b = 0; b < e->feedback_map->size; ++b) {
                AVLChain* cur = (AVLChain*)e->feedback_map->buckets[b];
                while (cur) {
                    if (cur->owner_id == e->event_id) persist_write_feedback_avl(cur->tree, e->event_id);
                    cur = cur->next;
                }
            }
        }
        avl_inorder(root, write_fbs);
    }
    persist_feedback_fp = NULL;
    fclose(ffp);
}

static void persist_save_all(IT_Club_System* sys) {
    persist_save_departments(sys);
    persist_save_events_and_feedback(sys);
    printf("[SAVE] Data saved to departments.dat, events.dat, feedback.dat\n");
}

static void persist_load_departments(IT_Club_System* sys) {
    FILE* fp = fopen("departments.dat", "rb");
    if (!fp) return;
    int cnt = 0;
    if (fread(&cnt, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    for (int i = 0; i < cnt; ++i) {
        PersistDepartment pd;
        if (fread(&pd, sizeof(PersistDepartment), 1, fp) != 1) break;
        Department* d = department_create(pd.dept_id, pd.name);
        for (int j = 0; j < pd.member_count; ++j) {
            Member* m = (Member*)malloc(sizeof(Member));
            if (fread(m, sizeof(Member), 1, fp) != 1) { free(m); break; }
            hmchain_insert_record(d->member_map, m->member_id, m, d->dept_id);
            d->member_count++;
        }
        hms_insert_top(sys->department_map, d->dept_id, d);
    }
    fclose(fp);
}

static void persist_load_events(IT_Club_System* sys) {
    FILE* fp = fopen("events.dat", "rb");
    if (!fp) return;
    int cnt = 0;
    if (fread(&cnt, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    for (int i = 0; i < cnt; ++i) {
        PersistEvent pe;
        if (fread(&pe, sizeof(PersistEvent), 1, fp) != 1) break;
        Event* e = event_create(pe.event_id, pe.name, pe.venue, pe.date, pe.budget, pe.description);
        e->expenses = pe.expenses;
        e->profit_loss = pe.profit_loss;
        for (int j = 0; j < pe.member_count; ++j) {
            Member* m = (Member*)malloc(sizeof(Member));
            if (fread(m, sizeof(Member), 1, fp) != 1) { free(m); break; }
            hmchain_insert_record(e->member_map, m->member_id, m, e->event_id);
            e->member_count++;
        }
        hms_insert_top(sys->event_map, e->event_id, e);
    }
    fclose(fp);
}

static void persist_load_feedbacks(IT_Club_System* sys) {
    FILE* fp = fopen("feedback.dat", "rb");
    if (!fp) return;
    int total = 0;
    if (fread(&total, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    for (int i = 0; i < total; ++i) {
        PersistFeedback pf;
        if (fread(&pf, sizeof(PersistFeedback), 1, fp) != 1) break;
        Event* e = (Event*)hms_get_top(sys->event_map, pf.event_id);
        if (!e) continue;
        Feedback* f = (Feedback*)malloc(sizeof(Feedback));
        *f = pf.fb;
        hmchain_insert_record(e->feedback_map, f->feedback_id, f, e->event_id);
        e->feedback_count++;
    }
    fclose(fp);
}

static void persist_load_all(IT_Club_System* sys) {
    persist_load_departments(sys);
    persist_load_events(sys);
    persist_load_feedbacks(sys);
    printf("[LOAD] Completed. Departments: %d, Events: %d\n", hms_count_all(sys->department_map), hms_count_all(sys->event_map));
}

// =======================
// New encryption using system sha256sum
// =======================

// generate_secure_key: use head -c 64 /dev/urandom | sha256sum
static int generate_secure_key_hex(char out_hex64[65]) {
    FILE* fp = popen("head -c 64 /dev/urandom | sha256sum", "r");
    if (!fp) {
        printf("[ERR] Failed to execute sha256sum command.\n");
        return -1;
    }
    char buffer[128];
    if (!fgets(buffer, sizeof(buffer), fp)) { pclose(fp); return -1; }
    pclose(fp);
    // extract first 64 hex chars
    int found = 0;
    for (int i = 0; i < (int)strlen(buffer) && found < 64; ++i) {
        if (isxdigit((unsigned char)buffer[i])) { out_hex64[found++] = buffer[i]; }
    }
    if (found != 64) return -1;
    out_hex64[64] = '\0';
    return 0;
}

// xor file using hex key string (64 hex chars)
static int xor_file_with_hex_key(const char* inpath, const char* outpath, const char* hexkey) {
    if (!inpath || !outpath || !hexkey) return -1;
    size_t hexlen = strlen(hexkey);
    if (hexlen != 64) return -1;

    FILE* fi = fopen(inpath, "rb");
    if (!fi) return -1;
    FILE* fo = fopen(outpath, "wb");
    if (!fo) { fclose(fi); return -1; }

    unsigned char buf[4096];
    size_t n;
    size_t idx = 0;
    while ((n = fread(buf, 1, sizeof(buf), fi)) > 0) {
        for (size_t i = 0; i < n; ++i) {
            // derive a byte from hexkey by taking two hex characters based on idx
            int pos = (int)(idx % 32); // 32 bytes
            char ha = hexkey[pos*2];
            char hb = hexkey[pos*2 + 1];
            unsigned char va = (ha >= '0' && ha <= '9') ? ha - '0' : (ha >= 'A' && ha <= 'F') ? ha - 'A' + 10 : (ha >= 'a' && ha <= 'f') ? ha - 'a' + 10 : 0;
            unsigned char vb = (hb >= '0' && hb <= '9') ? hb - '0' : (hb >= 'A' && hb <= 'F') ? hb - 'A' + 10 : (hb >= 'a' && hb <= 'f') ? hb - 'a' + 10 : 0;
            unsigned char keybyte = (va << 4) | vb;
            buf[i] ^= keybyte;
            idx++;
        }
        if (fwrite(buf, 1, n, fo) != n) { fclose(fi); fclose(fo); return -1; }
    }

    fclose(fi); fclose(fo);
    return 0;
}

static void encrypt_all_files_generate_key(void) {
    char hex[65];
    if (generate_secure_key_hex(hex) != 0) { printf("[ERR] Failed to generate secure key.\n"); return; }
    printf("=== SECURE KEY (SAVE THIS - required for decryption) ===\n%s\n====================================================\n", hex);

    if (xor_file_with_hex_key("departments.dat", "departments.enc", hex) == 0) printf("[OK] departments.dat -> departments.enc\n"); else printf("[ERR] Failed to encrypt departments.dat\n");
    if (xor_file_with_hex_key("events.dat", "events.enc", hex) == 0) printf("[OK] events.dat -> events.enc\n"); else printf("[ERR] Failed to encrypt events.dat\n");
    if (xor_file_with_hex_key("feedback.dat", "feedback.enc", hex) == 0) printf("[OK] feedback.dat -> feedback.enc\n"); else printf("[ERR] Failed to encrypt feedback.dat\n");
}

static void decrypt_all_files_with_prompt(void) {
    char hex[128];
    printf("Enter 64-character hex key (paste):\n");
    input_read_line(hex, sizeof(hex));
    if (strlen(hex) != 64) { printf("[ERR] Key must be 64 hex chars.\n"); return; }

    if (xor_file_with_hex_key("departments.enc", "departments.dat", hex) == 0) printf("[OK] departments.enc -> departments.dat\n"); else printf("[ERR] Failed to decrypt departments.enc\n");
    if (xor_file_with_hex_key("events.enc", "events.dat", hex) == 0) printf("[OK] events.enc -> events.dat\n"); else printf("[ERR] Failed to decrypt events.enc\n");
    if (xor_file_with_hex_key("feedback.enc", "feedback.dat", hex) == 0) printf("[OK] feedback.enc -> feedback.dat\n"); else printf("[ERR] Failed to decrypt feedback.enc\n");
}

// =======================
// Auto-save thread
// =======================
static void* autosave_thread(void* arg) {
    IT_Club_System* sys = (IT_Club_System*)arg;
    for (;;) {
        sleep(300); // 5 minutes
        pthread_mutex_lock(&sys->data_lock);
        persist_save_all(sys);
        pthread_mutex_unlock(&sys->data_lock);
    }
    return NULL;
}

// =======================
// Simple recursive data-only heap for Events
// =======================
typedef struct Heap {
    Event** data;
    int size;
    int capacity;
} Heap;
static Heap* heap_create(int cap) {
    Heap* h = (Heap*)malloc(sizeof(Heap));
    h->data = malloc(sizeof(Event*) * cap);
    h->size = 0; h->capacity = cap;
    return h;
}
static void heap_swap(Heap* h, int a, int b) { Event* tmp = h->data[a]; h->data[a] = h->data[b]; h->data[b] = tmp; }
static double heap_score(Event* e) { return e->profit_loss; }
static void heapify_up_recursive(Heap* h, int idx) {
    if (idx == 0) return;
    int parent = (idx - 1) / 2;
    if (heap_score(h->data[idx]) > heap_score(h->data[parent])) { heap_swap(h, idx, parent); heapify_up_recursive(h, parent); }
}
static void heapify_down_recursive(Heap* h, int idx) {
    int left = 2*idx + 1, right = 2*idx + 2;
    if (left >= h->size) return;
    int largest = idx;
    if (heap_score(h->data[left]) > heap_score(h->data[largest])) largest = left;
    if (right < h->size && heap_score(h->data[right]) > heap_score(h->data[largest])) largest = right;
    if (largest != idx) { heap_swap(h, idx, largest); heapify_down_recursive(h, largest); }
}
static void heap_insert(Heap* h, Event* e) { if (h->size >= h->capacity) return; int idx = h->size++; h->data[idx] = e; heapify_up_recursive(h, idx); }
static Event* heap_extract_max(Heap* h) { if (h->size == 0) return NULL; Event* max_event = h->data[0]; h->data[0] = h->data[h->size - 1]; h->size--; heapify_down_recursive(h, 0); return max_event; }

// =======================
// ParticipationRecord heap (for most active members)
// =======================
typedef struct ParticipationRecord { Member* member; int count; } ParticipationRecord;
typedef struct PHeap { ParticipationRecord** data; int size; int capacity; } PHeap;
static PHeap* pheap_create(int cap) { PHeap* h = (PHeap*)malloc(sizeof(PHeap)); h->data = malloc(sizeof(ParticipationRecord*) * cap); h->size = 0; h->capacity = cap; return h; }
static void pheap_swap(PHeap* h, int a, int b) { ParticipationRecord* tmp = h->data[a]; h->data[a] = h->data[b]; h->data[b] = tmp; }
static int pheap_score(ParticipationRecord* r) { return r->count; }
static void pheap_up(PHeap* h, int idx) { if (idx == 0) return; int p = (idx - 1) / 2; if (pheap_score(h->data[idx]) > pheap_score(h->data[p])) { pheap_swap(h, idx, p); pheap_up(h, p); } }
static void pheap_down(PHeap* h, int idx) { int l = 2*idx + 1, r = 2*idx + 2; if (l >= h->size) return; int largest = idx; if (pheap_score(h->data[l]) > pheap_score(h->data[largest])) largest = l; if (r < h->size && pheap_score(h->data[r]) > pheap_score(h->data[largest])) largest = r; if (largest != idx) { pheap_swap(h, idx, largest); pheap_down(h, largest); } }
static void pheap_insert(PHeap* h, ParticipationRecord* r) { if (h->size >= h->capacity) return; int idx = h->size++; h->data[idx] = r; pheap_up(h, idx); }
static ParticipationRecord* pheap_extract_max(PHeap* h) { if (h->size == 0) return NULL; ParticipationRecord* r = h->data[0]; h->data[0] = h->data[h->size - 1]; h->size--; pheap_down(h, 0); return r; }

// =======================
// Reporting Top-K Events
// =======================
static Heap* reporting_heap = NULL;
static void reporting_heap_cb(void* data) { if (!reporting_heap || !data) return; Event* e = (Event*)data; heap_insert(reporting_heap, e); }
static void reporting_top_k(IT_Club_System* sys, int k) {
    Heap* h = heap_create(1024);
    reporting_heap = h;
    for (int i = 0; i < sys->event_map->size; ++i) avl_inorder((AVL_Node*)sys->event_map->buckets[i], reporting_heap_cb);
    reporting_heap = NULL;
    printf("\n=== Top %d Events by Profit/Loss ===\n", k);
    for (int i = 0; i < k && h->size > 0; ++i) {
        Event* e = heap_extract_max(h);
        printf("%d. %s (ID:%d) — Profit: %.2f\n", i+1, e->name, e->event_id, e->profit_loss);
    }
    free(h->data);
    free(h);
}

// =======================
// Most Active Member Feature
// =======================
static void participation_insert_seed(Member* m, HashMap_Chain* part_map, int owner_id) {
    ParticipationRecord* r = (ParticipationRecord*)malloc(sizeof(ParticipationRecord));
    r->member = m; r->count = 0;
    hmchain_insert_record(part_map, m->member_id, r, owner_id);
}
static HashMap_Chain* g_part_map_tmp = NULL;
static int g_part_owner_tmp = 0;
static void seed_participation_cb(void* data) {
    Member* m = (Member*)data;
    if (!m || !g_part_map_tmp) return;
    participation_insert_seed(m, g_part_map_tmp, g_part_owner_tmp);
}
static Department* g_active_dept = NULL;
static HashMap_Chain* g_part_map_count = NULL;
static void count_participation_member_cb(void* data) {
    Member* m = (Member*)data;
    if (!m || !g_active_dept || !g_part_map_count) return;
    if (strcmp(m->department_name, g_active_dept->name) == 0) {
        ParticipationRecord* r = (ParticipationRecord*)hmchain_get_record(g_part_map_count, m->member_id, g_active_dept->dept_id);
        if (r) r->count++;
    }
}
static PHeap* g_ph_for_push = NULL;
static void push_participation_cb(void* data) {
    ParticipationRecord* r = (ParticipationRecord*)data;
    if (!r || !g_ph_for_push) return;
    pheap_insert(g_ph_for_push, r);
}

static void display_most_active_members(IT_Club_System* sys) {
    printf("Enter Department ID to analyze: ");
    int did;
    if (!input_read_int(&did)) return;
    Department* d = (Department*)hms_get_top(sys->department_map, did);
    if (!d) { printf("Department not found.\n"); return; }

    HashMap_Chain* part_map = create_HashMap_Chain(64, 1);
    g_part_map_tmp = part_map; g_part_owner_tmp = d->dept_id;
    for (int i = 0; i < d->member_map->size; ++i) {
        AVLChain* cur = (AVLChain*)d->member_map->buckets[i];
        while (cur) {
            if (cur->owner_id == d->dept_id) avl_inorder(cur->tree, seed_participation_cb);
            cur = cur->next;
        }
    }
    g_part_map_tmp = NULL;

    g_active_dept = d; g_part_map_count = part_map;
    for (int i = 0; i < sys->event_map->size; ++i) {
        AVL_Node* root = (AVL_Node*)sys->event_map->buckets[i];
        if (!root) continue;
        void per_event_cb(void* edata) {
            Event* e = (Event*)edata;
            if (!e) return;
            for (int b = 0; b < e->member_map->size; ++b) {
                AVLChain* cur = (AVLChain*)e->member_map->buckets[b];
                while (cur) {
                    if (cur->owner_id == e->event_id) avl_inorder(cur->tree, count_participation_member_cb);
                    cur = cur->next;
                }
            }
        }
        avl_inorder(root, per_event_cb);
    }
    g_active_dept = NULL; g_part_map_count = NULL;

    PHeap* ph = pheap_create(256);
    g_ph_for_push = ph;
    for (int i = 0; i < part_map->size; ++i) {
        AVLChain* cur = (AVLChain*)part_map->buckets[i];
        while (cur) {
            if (cur->owner_id == d->dept_id) avl_inorder(cur->tree, push_participation_cb);
            cur = cur->next;
        }
    }
    g_ph_for_push = NULL;

    int k = 3;
    printf("\nTop %d active members in Department '%s' (by event participation):\n", k, d->name);
    for (int i = 0; i < k; ++i) {
        ParticipationRecord* rec = pheap_extract_max(ph);
        if (!rec) break;
        Member* m = rec->member;
        printf("%d. %s (Member ID:%d) — Events Participated: %d\n", i+1, m->name, m->member_id, rec->count);
    }

    // Note: we don't free ph.data elements here (they point to ParticipationRecords stored in the chain)
    // and members themselves are owned by Departments/Events; if you want to free everything on exit, add cleanup.
}

// =======================
// Interactive functions
// =======================
static void interactive_create_department(IT_Club_System* sys) {
    printf("Enter Department ID (int): ");
    int id;
    if (!input_read_int(&id)) { printf("Invalid.\n"); return; }
    if (hms_get_top(sys->department_map, id)) { printf("Department ID exists.\n"); return; }
    char name[100];
    printf("Enter Department Name: ");
    input_read_line(name, sizeof(name));
    Department* d = department_create(id, name);
    hms_insert_top(sys->department_map, id, d);
    printf("Department '%s' added (ID:%d)\n", name, id);
}
static void interactive_create_event(IT_Club_System* sys) {
    printf("Enter Event ID (int): ");
    int id;
    if (!input_read_int(&id)) { printf("Invalid.\n"); return; }
    if (hms_get_top(sys->event_map, id)) { printf("Event ID exists.\n"); return; }
    char name[100], venue[100], date[20], desc[256];
    double budget = 0.0, expenses = 0.0;
    printf("Enter Event Name: "); input_read_line(name, sizeof(name));
    printf("Enter Venue: "); input_read_line(venue, sizeof(venue));
    printf("Enter Date (DD/MM/YYYY): "); input_read_line(date, sizeof(date));
    printf("Enter Budget: "); input_read_double(&budget);
    printf("Enter Expenses: "); input_read_double(&expenses);
    printf("Enter Description: "); input_read_line(desc, sizeof(desc));
    Event* e = event_create(id, name, venue, date, budget, desc);
    e->expenses = expenses;
    e->profit_loss = budget - expenses;
    hms_insert_top(sys->event_map, id, e);
    printf("Event '%s' created (ID:%d) Profit/Loss: %.2f\n", name, id, e->profit_loss);
}
static void interactive_add_member_department(IT_Club_System* sys) {
    printf("Enter Department ID: ");
    int did; if (!input_read_int(&did)) return;
    Department* d = (Department*)hms_get_top(sys->department_map, did);
    if (!d) { printf("Department not found.\n"); return; }
    Member tmp;
    printf("Enter Member ID: "); if (!input_read_int(&tmp.member_id)) return;
    if (hmchain_get_record(d->member_map, tmp.member_id, d->dept_id)) { printf("Member ID exists in dept.\n"); return; }
    printf("Enter Member Name: "); input_read_line(tmp.name, sizeof(tmp.name));
    printf("Enter Role: "); input_read_line(tmp.role, sizeof(tmp.role));
    printf("Enter Contact: "); input_read_line(tmp.contact, sizeof(tmp.contact));
    strncpy(tmp.department_name, d->name, sizeof(tmp.department_name)-1); tmp.department_name[sizeof(tmp.department_name)-1] = '\0';
    Member* m = (Member*)malloc(sizeof(Member)); *m = tmp;
    hmchain_insert_record(d->member_map, m->member_id, m, d->dept_id);
    d->member_count++;
    printf("Member '%s' (ID:%d) added to Dept '%s'\n", m->name, m->member_id, d->name);
}
static void interactive_add_member_event(IT_Club_System* sys) {
    printf("Enter Event ID: ");
    int eid; if (!input_read_int(&eid)) return;
    Event* e = (Event*)hms_get_top(sys->event_map, eid);
    if (!e) { printf("Event not found.\n"); return; }
    Member tmp;
    printf("Enter Member ID: "); if (!input_read_int(&tmp.member_id)) return;
    if (hmchain_get_record(e->member_map, tmp.member_id, e->event_id)) { printf("Member ID exists for this event.\n"); return; }
    printf("Enter Member Name: "); input_read_line(tmp.name, sizeof(tmp.name));
    printf("Enter Role: "); input_read_line(tmp.role, sizeof(tmp.role));
    printf("Enter Contact: "); input_read_line(tmp.contact, sizeof(tmp.contact));
    printf("Enter Department Name: "); input_read_line(tmp.department_name, sizeof(tmp.department_name));
    Member* m = (Member*)malloc(sizeof(Member)); *m = tmp;
    hmchain_insert_record(e->member_map, m->member_id, m, e->event_id);
    e->member_count++;
    printf("Member '%s' (ID:%d) added to Event '%s'\n", m->name, m->member_id, e->name);
}
static void interactive_remove_member_event(IT_Club_System* sys) {
    printf("Enter Event ID: ");
    int eid; if (!input_read_int(&eid)) return;
    Event* e = (Event*)hms_get_top(sys->event_map, eid);
    if (!e) { printf("Event not found.\n"); return; }
    printf("Enter Member ID to remove: ");
    int mid; if (!input_read_int(&mid)) return;
    hmchain_delete_record(e->member_map, mid, e->event_id);
    if (e->member_count > 0) e->member_count--;
    printf("Attempted removal of member %d from event %d\n", mid, eid);
}
static void interactive_add_feedback_event(IT_Club_System* sys) {
    printf("Enter Event ID: ");
    int eid; if (!input_read_int(&eid)) return;
    Event* e = (Event*)hms_get_top(sys->event_map, eid);
    if (!e) { printf("Event not found.\n"); return; }
    Feedback tmp;
    printf("Enter Feedback ID: "); if (!input_read_int(&tmp.feedback_id)) return;
    if (hmchain_get_record(e->feedback_map, tmp.feedback_id, e->event_id)) { printf("Feedback ID exists.\n"); return; }
    printf("Enter Member ID (author): "); input_read_int(&tmp.member_id);
    printf("Enter Rating (1-5): "); input_read_int(&tmp.rating);
    printf("Enter Comment: "); input_read_line(tmp.comment, sizeof(tmp.comment));
    Feedback* f = (Feedback*)malloc(sizeof(Feedback)); *f = tmp;
    hmchain_insert_record(e->feedback_map, f->feedback_id, f, e->event_id);
    e->feedback_count++;
    printf("Feedback %d added to event %s\n", f->feedback_id, e->name);
}
static void interactive_edit_event_financials(IT_Club_System* sys) {
    printf("Enter Event ID to edit: ");
    int eid; if (!input_read_int(&eid)) return;
    Event* e = (Event*)hms_get_top(sys->event_map, eid);
    if (!e) { printf("Event not found.\n"); return; }
    printf("Current Budget: %.2f | Expenses: %.2f | Profit/Loss: %.2f\n", e->budget, e->expenses, e->profit_loss);
    double nb, ne;
    printf("Enter new Budget (press Enter to keep): ");
    if (!input_read_double(&nb)) nb = e->budget;
    printf("Enter new Expenses (press Enter to keep): ");
    if (!input_read_double(&ne)) ne = e->expenses;
    e->budget = nb; e->expenses = ne; e->profit_loss = nb - ne;
    printf("Updated: Profit/Loss = %.2f\n", e->profit_loss);
}
static void interactive_display_all_departments(IT_Club_System* sys) {
    if (hms_count_all(sys->department_map) == 0) { printf("No departments.\n"); return; }
    hms_traverse_all(sys->department_map, display_department_cb);
}
static void interactive_display_members_department(IT_Club_System* sys) {
    printf("Enter Department ID: ");
    int did; if (!input_read_int(&did)) return;
    Department* d = (Department*)hms_get_top(sys->department_map, did);
    if (!d) { printf("Department not found.\n"); return; }
    if (d->member_count == 0) { printf("No members.\n"); return; }
    printf("Members in Department '%s':\n", d->name);
    hmchain_traverse_owner(d->member_map, d->dept_id, display_member_cb);
}
static void interactive_display_all_events(IT_Club_System* sys) {
    if (hms_count_all(sys->event_map) == 0) { printf("No events.\n"); return; }
    hms_traverse_all(sys->event_map, display_event_cb);
}
static void interactive_display_members_event(IT_Club_System* sys) {
    printf("Enter Event ID: ");
    int eid; if (!input_read_int(&eid)) return;
    Event* e = (Event*)hms_get_top(sys->event_map, eid);
    if (!e) { printf("Event not found.\n"); return; }
    if (e->member_count == 0) { printf("No members.\n"); return; }
    printf("Members in Event '%s':\n", e->name);
    hmchain_traverse_owner(e->member_map, e->event_id, display_member_cb);
}
static void interactive_display_feedback_event(IT_Club_System* sys) {
    printf("Enter Event ID: ");
    int eid; if (!input_read_int(&eid)) return;
    Event* e = (Event*)hms_get_top(sys->event_map, eid);
    if (!e) { printf("Event not found.\n"); return; }
    if (e->feedback_count == 0) { printf("No feedback.\n"); return; }
    printf("Feedback for Event '%s':\n", e->name);
    hmchain_traverse_owner(e->feedback_map, e->event_id, display_feedback_cb);
}

// =======================
// Menu & main
// =======================
static void display_menu(void) {
    printf("\n========== IT Club Management System ==========\n");
    printf("1.  Create Event (user-provided ID)\n");
    printf("2.  Create Department (user-provided ID)\n");
    printf("3.  Add Member to Department\n");
    printf("4.  Add Member to Event\n");
    printf("5.  Remove Member from Event\n");
    printf("6.  Add Feedback to Event\n");
    printf("7.  Find Event by ID\n");
    printf("8.  Display All Events\n");
    printf("9.  Display All Departments\n");
    printf("10. Display Members in Department\n");
    printf("11. Display Members in Event\n");
    printf("12. Display Feedback for Event\n");
    printf("13. Top 3 Profitable Events\n");
    printf("14. Export & Encrypt All (.enc files)\n");
    printf("15. Import & Decrypt All (.enc files)\n");
    printf("16. Save & Exit\n");
    printf("17. Edit Event Budget/Expenses\n");
    printf("18. Most Active Members in Department (Top 3)\n");
    printf("===============================================\n");
    printf("Enter choice: ");
}

int main(void) {
    IT_Club_System* sys = system_create();
    if (!sys) { fprintf(stderr, "Failed to initialize system.\n"); return 1; }

    printf("[INIT] Loading data...\n");
    persist_load_all(sys);

    pthread_t tid;
    if (pthread_create(&tid, NULL, autosave_thread, sys) == 0) pthread_detach(tid);
    else printf("[WARN] autosave thread failed\n");

    for (;;) {
        display_menu();
        int choice;
        if (!input_read_int(&choice)) { printf("Invalid input.\n"); continue; }
        pthread_mutex_lock(&sys->data_lock);
        switch (choice) {
            case 1: interactive_create_event(sys); break;
            case 2: interactive_create_department(sys); break;
            case 3: interactive_add_member_department(sys); break;
            case 4: interactive_add_member_event(sys); break;
            case 5: interactive_remove_member_event(sys); break;
            case 6: interactive_add_feedback_event(sys); break;
            case 7: {
                printf("Enter Event ID: ");
                int id; if (input_read_int(&id)) {
                    Event* e = (Event*)hms_get_top(sys->event_map, id);
                    if (!e) printf("Event not found.\n"); else display_event_cb(e);
                }
                break;
            }
            case 8: interactive_display_all_events(sys); break;
            case 9: interactive_display_all_departments(sys); break;
            case 10: interactive_display_members_department(sys); break;
            case 11: interactive_display_members_event(sys); break;
            case 12: interactive_display_feedback_event(sys); break;
            case 13: reporting_top_k(sys, 3); break;
            case 14: { persist_save_all(sys); encrypt_all_files_generate_key(); } break;
            case 15: { decrypt_all_files_with_prompt(); persist_load_all(sys); } break;
            case 16: persist_save_all(sys); pthread_mutex_unlock(&sys->data_lock); printf("Saved. Exiting.\n"); exit(0); break;
            case 17: interactive_edit_event_financials(sys); break;
            case 18: display_most_active_members(sys); break;
            default: printf("Invalid choice.\n"); break;
        }
        pthread_mutex_unlock(&sys->data_lock);
    }
    return 0;
}
