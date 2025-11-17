#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* ===========================
   Basic AVL Tree Implementation
   =========================== */

/* Generic AVL node used for top-level indices and for per-chain trees */
typedef struct AVL_Node {
    int key;            /* typically ID (dept_id, event_id, member_id, feedback_id) */
    void* data;         /* pointer to stored object (Department*, Event*, Member*, Feedback*) */
    int height;
    struct AVL_Node* left;
    struct AVL_Node* right;
} AVL_Node;

/* Helper: max of two ints */
static int helper_max(int a, int b) { return (a > b) ? a : b; }

/* AVL helpers */
static int avl_height(AVL_Node* n) { return n ? n->height : 0; }
static int avl_balance(AVL_Node* n) { return n ? avl_height(n->left) - avl_height(n->right) : 0; }

/* Right rotate */
static AVL_Node* avl_right_rotate(AVL_Node* y) {
    AVL_Node* x = y->left;
    AVL_Node* T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = helper_max(avl_height(y->left), avl_height(y->right)) + 1;
    x->height = helper_max(avl_height(x->left), avl_height(x->right)) + 1;
    return x;
}

/* Left rotate */
static AVL_Node* avl_left_rotate(AVL_Node* x) {
    AVL_Node* y = x->right;
    AVL_Node* T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = helper_max(avl_height(x->left), avl_height(x->right)) + 1;
    y->height = helper_max(avl_height(y->left), avl_height(y->right)) + 1;
    return y;
}

/* Insert node into AVL tree (no duplicate handling beyond ignoring) */
static AVL_Node* avl_insert(AVL_Node* node, int key, void* data) {
    if (!node) {
        AVL_Node* n = (AVL_Node*)malloc(sizeof(AVL_Node));
        n->key = key;
        n->data = data;
        n->height = 1;
        n->left = n->right = NULL;
        return n;
    }
    if (key < node->key) node->left = avl_insert(node->left, key, data);
    else if (key > node->key) node->right = avl_insert(node->right, key, data);
    else {
        /* Duplicate: update data pointer and return */
        node->data = data;
        return node;
    }
    node->height = 1 + helper_max(avl_height(node->left), avl_height(node->right));
    int balance = avl_balance(node);

    if (balance > 1 && key < node->left->key) return avl_right_rotate(node);
    if (balance < -1 && key > node->right->key) return avl_left_rotate(node);
    if (balance > 1 && key > node->left->key) {
        node->left = avl_left_rotate(node->left);
        return avl_right_rotate(node);
    }
    if (balance < -1 && key < node->right->key) {
        node->right = avl_right_rotate(node->right);
        return avl_left_rotate(node);
    }
    return node;
}

/* Find min node in AVL subtree */
static AVL_Node* avl_min_node(AVL_Node* node) {
    AVL_Node* cur = node;
    while (cur && cur->left) cur = cur->left;
    return cur;
}

/* Delete node by key from AVL tree */
static AVL_Node* avl_delete(AVL_Node* root, int key) {
    if (!root) return root;
    if (key < root->key) root->left = avl_delete(root->left, key);
    else if (key > root->key) root->right = avl_delete(root->right, key);
    else {
        if (!root->left || !root->right) {
            AVL_Node* temp = root->left ? root->left : root->right;
            if (!temp) {
                temp = root;
                root = NULL;
            } else *root = *temp;
            free(temp);
        } else {
            AVL_Node* temp = avl_min_node(root->right);
            root->key = temp->key;
            root->data = temp->data;
            root->right = avl_delete(root->right, temp->key);
        }
    }
    if (!root) return root;
    root->height = 1 + helper_max(avl_height(root->left), avl_height(root->right));
    int balance = avl_balance(root);
    if (balance > 1 && avl_balance(root->left) >= 0) return avl_right_rotate(root);
    if (balance > 1 && avl_balance(root->left) < 0) { root->left = avl_left_rotate(root->left); return avl_right_rotate(root); }
    if (balance < -1 && avl_balance(root->right) <= 0) return avl_left_rotate(root);
    if (balance < -1 && avl_balance(root->right) > 0) { root->right = avl_right_rotate(root->right); return avl_left_rotate(root); }
    return root;
}

/* Search AVL by key */
static AVL_Node* avl_search(AVL_Node* root, int key) {
    if (!root || root->key == key) return root;
    if (key < root->key) return avl_search(root->left, key);
    return avl_search(root->right, key);
}

/* In-order traversal calling callback on each node->data */
static void avl_inorder(AVL_Node* root, void (*cb)(void*)) {
    if (!root) return;
    avl_inorder(root->left, cb);
    if (cb) cb(root->data);
    avl_inorder(root->right, cb);
}

/* Count nodes in AVL */
static int avl_count_nodes(AVL_Node* root) {
    if (!root) return 0;
    return 1 + avl_count_nodes(root->left) + avl_count_nodes(root->right);
}

/* ===========================
   Domain Models & Persistable structs
   =========================== */

/* Member stored in department or event */
typedef struct Member {
    int member_id;
    char name[100];
    char role[50];
    char contact[15];
    char department_name[50]; /* optional textual department reference */
} Member;

/* Feedback stored in event */
typedef struct Feedback {
    int feedback_id;
    int member_id;
    int rating;
    char comment[256];
} Feedback;

/* Forward declare HashMap_Chain to be used in Department/Event */
typedef struct HashMap_Chain HashMap_Chain;

/* Event entity */
typedef struct Event {
    int event_id;
    char name[100];
    char venue[100];
    char date[20];
    double budget;
    double expenses;
    double profit_loss;
    char description[256];
    HashMap_Chain* member_map;
    HashMap_Chain* feedback_map;
    int member_count;
    int feedback_count;
} Event;

/* Department entity */
typedef struct Department {
    int dept_id;
    char name[100];
    HashMap_Chain* member_map;
    int member_count;
} Department;

/* Persistable pointer-free structs (written/read as blocks) */
typedef struct PersistDepartment {
    int dept_id;
    char name[100];
    int member_count;
} PersistDepartment;

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

typedef struct PersistFeedback {
    int event_id;
    Feedback fb;
} PersistFeedback;

/* ===========================
   HashMap_Chain: Owner-aware chaining with AVL trees per chain
   Structure:
     HashMap_Chain -> buckets[] -> AVLChain linked list per bucket
     AVLChain holds owner_type, owner_id, and an AVL tree for records
   =========================== */

typedef enum {
    OWNER_DEPARTMENT = 1,
    OWNER_EVENT_MEMBERS = 2,
    OWNER_EVENT_FEEDBACK = 3
} OwnerType;

/* Chain node: linked list element in a bucket */
typedef struct AVLChain {
    AVL_Node* tree;        /* AVL tree storing the records for this owner */
    int owner_type;
    int owner_id;
    struct AVLChain* next;
} AVLChain;

/* HashMap_Chain container */
struct HashMap_Chain {
    AVLChain** buckets;
    int size;
};

/* Create HashMap_Chain of given bucket count */
static HashMap_Chain* hmchain_create(int size) {
    HashMap_Chain* m = (HashMap_Chain*)malloc(sizeof(HashMap_Chain));
    m->size = size;
    m->buckets = (AVLChain**)calloc(size, sizeof(AVLChain*));
    return m;
}

/* Hash helper for ints */
static int hmchain_hash(int key, int size) {
    if (size == 0) return 0;
    int v = abs(key) % size;
    return v;
}

/* Find chain in a bucket by owner_type+owner_id */
static AVLChain* hmchain_find_chain(AVLChain* head, int owner_type, int owner_id) {
    AVLChain* cur = head;
    while (cur) {
        if (cur->owner_type == owner_type && cur->owner_id == owner_id) return cur;
        cur = cur->next;
    }
    return NULL;
}

/* Create and prepend chain into bucket */
static AVLChain* hmchain_create_chain(HashMap_Chain* map, int idx, int owner_type, int owner_id) {
    AVLChain* c = (AVLChain*)malloc(sizeof(AVLChain));
    c->tree = NULL;
    c->owner_type = owner_type;
    c->owner_id = owner_id;
    c->next = map->buckets[idx];
    map->buckets[idx] = c;
    return c;
}

/* Insert a record (key,data) into map under owner metadata */
static void hmchain_insert(HashMap_Chain* map, int key, void* data, int owner_type, int owner_id) {
    if (!map) return;
    int idx = hmchain_hash(key, map->size);
    AVLChain* chain = hmchain_find_chain(map->buckets[idx], owner_type, owner_id);
    if (!chain) chain = hmchain_create_chain(map, idx, owner_type, owner_id);
    /* If key exists, update; else insert */
    AVL_Node* found = avl_search(chain->tree, key);
    if (found) { found->data = data; return; }
    chain->tree = avl_insert(chain->tree, key, data);
}

/* Get a record by key in a specific owner chain */
static void* hmchain_get(HashMap_Chain* map, int key, int owner_type, int owner_id) {
    if (!map) return NULL;
    int idx = hmchain_hash(key, map->size);
    AVLChain* chain = hmchain_find_chain(map->buckets[idx], owner_type, owner_id);
    if (!chain) return NULL;
    AVL_Node* n = avl_search(chain->tree, key);
    return n ? n->data : NULL;
}

/* Delete a record by key in specific owner chain */
static void hmchain_delete(HashMap_Chain* map, int key, int owner_type, int owner_id) {
    if (!map) return;
    int idx = hmchain_hash(key, map->size);
    AVLChain* chain = hmchain_find_chain(map->buckets[idx], owner_type, owner_id);
    if (!chain) return;
    chain->tree = avl_delete(chain->tree, key);
}

/* Traverse all chains and call callback only for chains matching owner_type+owner_id */
static void hmchain_traverse_owner(HashMap_Chain* map, int owner_type, int owner_id, void (*cb)(void*)) {
    if (!map || !cb) return;
    for (int i = 0; i < map->size; ++i) {
        AVLChain* cur = map->buckets[i];
        while (cur) {
            if (cur->owner_type == owner_type && cur->owner_id == owner_id) {
                avl_inorder(cur->tree, cb);
            }
            cur = cur->next;
        }
    }
}

/* Count nodes for specific owner across all chains */
static int hmchain_count_owner(HashMap_Chain* map, int owner_type, int owner_id) {
    if (!map) return 0;
    int total = 0;
    for (int i = 0; i < map->size; ++i) {
        AVLChain* cur = map->buckets[i];
        while (cur) {
            if (cur->owner_type == owner_type && cur->owner_id == owner_id) {
                total += avl_count_nodes(cur->tree);
            }
            cur = cur->next;
        }
    }
    return total;
}

/* ===========================
   System context
   =========================== */

typedef struct IT_Club_System {
    AVL_Node* department_tree; /* dept_id -> Department* */
    AVL_Node* event_tree;      /* event_id -> Event* */
    pthread_mutex_t data_lock;
} IT_Club_System;

/* Create and initialize system context */
static IT_Club_System* system_create() {
    IT_Club_System* s = (IT_Club_System*)malloc(sizeof(IT_Club_System));
    s->department_tree = NULL;
    s->event_tree = NULL;
    pthread_mutex_init(&s->data_lock, NULL);
    return s;
}

/* ===========================
   Display helpers (extracted)
   =========================== */

/* Print functions for domain objects used as callbacks */
static void display_member(void* data) {
    Member* m = (Member*)data;
    if (!m) return;
    printf("  ID:%d | %s | Role:%s | Contact:%s | Dept:%s\n",
           m->member_id, m->name, m->role, m->contact, m->department_name);
}

static void display_feedback(void* data) {
    Feedback* f = (Feedback*)data;
    if (!f) return;
    printf("  Feedback ID:%d | Member ID:%d | Rating:%d/5\n", f->feedback_id, f->member_id, f->rating);
    printf("    %s\n", f->comment);
}

static void display_event_basic(void* data) {
    Event* e = (Event*)data;
    if (!e) return;
    printf("\n=== Event ID:%d | %s ===\nVenue:%s | Date:%s\nBudget:%.2f | Expenses:%.2f | Profit/Loss:%.2f\nMembers:%d | Feedback:%d\n",
        e->event_id, e->name, e->venue, e->date, e->budget, e->expenses, e->profit_loss, e->member_count, e->feedback_count);
}

static void display_department_basic(void* data) {
    Department* d = (Department*)data;
    if (!d) return;
    printf("\n=== Department ID:%d | %s ===\nMembers:%d\n", d->dept_id, d->name, d->member_count);
}

/* ===========================
   Department module helpers
   =========================== */

/* Allocate department object */
static Department* department_create(int dept_id, const char* name) {
    Department* d = (Department*)malloc(sizeof(Department));
    d->dept_id = dept_id;
    strncpy(d->name, name, sizeof(d->name)-1);
    d->name[sizeof(d->name)-1] = '\0';
    d->member_map = hmchain_create(10);
    d->member_count = 0;
    return d;
}

/* Check if department exists in system */
static int department_exists(IT_Club_System* sys, int dept_id) {
    return avl_search(sys->department_tree, dept_id) != NULL;
}

/* Add department (interactive wrapper uses this helper) */
static void department_add(IT_Club_System* sys, Department* d) {
    sys->department_tree = avl_insert(sys->department_tree, d->dept_id, d);
}

/* Find department by id */
static Department* department_find(IT_Club_System* sys, int dept_id) {
    AVL_Node* node = avl_search(sys->department_tree, dept_id);
    return node ? (Department*)node->data : NULL;
}

/* Add member to department by Department pointer (helper) */
static int department_add_member(Department* d, Member* m) {
    /* Ensure unique member id within this department */
    void* exists = hmchain_get(d->member_map, m->member_id, OWNER_DEPARTMENT, d->dept_id);
    if (exists) return 0; /* not inserted */
    Member* mp = (Member*)malloc(sizeof(Member));
    *mp = *m;
    hmchain_insert(d->member_map, mp->member_id, mp, OWNER_DEPARTMENT, d->dept_id);
    d->member_count++;
    return 1;
}

/* Display members in department (helper) */
static void department_display_members(Department* d) {
    if (!d) { printf("Department not found.\n"); return; }
    if (d->member_count == 0) { printf("No members in department '%s'.\n", d->name); return; }
    printf("\nMembers in Department '%s' (ID:%d):\n", d->name, d->dept_id);
    hmchain_traverse_owner(d->member_map, OWNER_DEPARTMENT, d->dept_id, display_member);
}

/* ===========================
   Event module helpers
   =========================== */

/* Allocate event object */
static Event* event_create(int event_id, const char* name, const char* venue, const char* date, double budget, const char* desc) {
    Event* e = (Event*)malloc(sizeof(Event));
    e->event_id = event_id;
    strncpy(e->name, name, sizeof(e->name)-1); e->name[sizeof(e->name)-1] = '\0';
    strncpy(e->venue, venue, sizeof(e->venue)-1); e->venue[sizeof(e->venue)-1] = '\0';
    strncpy(e->date, date, sizeof(e->date)-1); e->date[sizeof(e->date)-1] = '\0';
    e->budget = budget;
    e->expenses = 0.0;
    e->profit_loss = budget;
    strncpy(e->description, desc, sizeof(e->description)-1); e->description[sizeof(e->description)-1] = '\0';
    e->member_map = hmchain_create(10);
    e->feedback_map = hmchain_create(10);
    e->member_count = 0;
    e->feedback_count = 0;
    return e;
}

/* Check if event exists */
static int event_exists(IT_Club_System* sys, int event_id) {
    return avl_search(sys->event_tree, event_id) != NULL;
}

/* Add event into system */
static void event_add(IT_Club_System* sys, Event* e) {
    sys->event_tree = avl_insert(sys->event_tree, e->event_id, e);
}

/* Find event by id */
static Event* event_find(IT_Club_System* sys, int event_id) {
    AVL_Node* node = avl_search(sys->event_tree, event_id);
    return node ? (Event*)node->data : NULL;
}

/* Add a member to an Event (helper) */
static int event_add_member(Event* e, Member* m) {
    void* exists = hmchain_get(e->member_map, m->member_id, OWNER_EVENT_MEMBERS, e->event_id);
    if (exists) return 0;
    Member* mp = (Member*)malloc(sizeof(Member));
    *mp = *m;
    hmchain_insert(e->member_map, mp->member_id, mp, OWNER_EVENT_MEMBERS, e->event_id);
    e->member_count++;
    return 1;
}

/* Remove member from event (helper) */
static void event_delete_member(Event* e, int member_id) {
    hmchain_delete(e->member_map, member_id, OWNER_EVENT_MEMBERS, e->event_id);
    if (e->member_count > 0) e->member_count--;
}

/* Add feedback to event (helper) */
static int event_add_feedback(Event* e, Feedback* fb) {
    void* exists = hmchain_get(e->feedback_map, fb->feedback_id, OWNER_EVENT_FEEDBACK, e->event_id);
    if (exists) return 0;
    Feedback* fbp = (Feedback*)malloc(sizeof(Feedback));
    *fbp = *fb;
    hmchain_insert(e->feedback_map, fbp->feedback_id, fbp, OWNER_EVENT_FEEDBACK, e->event_id);
    e->feedback_count++;
    return 1;
}

/* Display members of event */
static void event_display_members(Event* e) {
    if (!e) { printf("Event not found.\n"); return; }
    if (e->member_count == 0) { printf("No members in event '%s'.\n", e->name); return; }
    printf("\nMembers in Event '%s' (ID:%d):\n", e->name, e->event_id);
    hmchain_traverse_owner(e->member_map, OWNER_EVENT_MEMBERS, e->event_id, display_member);
}

/* Display feedback for event */
static void event_display_feedback(Event* e) {
    if (!e) { printf("Event not found.\n"); return; }
    if (e->feedback_count == 0) { printf("No feedback for event '%s'.\n", e->name); return; }
    printf("\nFeedback for Event '%s' (ID:%d):\n", e->name, e->event_id);
    hmchain_traverse_owner(e->feedback_map, OWNER_EVENT_FEEDBACK, e->event_id, display_feedback);
}

/* ===========================
   Persistence: Save helpers (extracted)
   We persist:
     - departments.dat  : [int count][PersistDepartment + member blocks...]
     - events.dat       : [int count][PersistEvent + member blocks + feedback blocks...]
     - feedback.dat     : [int total_feedback][PersistFeedback ...]  (global archive)
   =========================== */

/* Write a single Member object to file */
static void persist_write_member(FILE* fp, Member* m) {
    fwrite(m, sizeof(Member), 1, fp);
}

/* Write a single Feedback object to file */
static void persist_write_feedback(FILE* fp, Feedback* f) {
    fwrite(f, sizeof(Feedback), 1, fp);
}

/* Save members from a chain's AVL tree in-order to file */
static void persist_save_members_from_avl(FILE* fp, AVL_Node* root) {
    if (!root) return;
    persist_save_members_from_avl(fp, root->left);
    Member* m = (Member*)root->data;
    persist_write_member(fp, m);
    persist_save_members_from_avl(fp, root->right);
}

/* Save feedback from a chain's AVL tree in-order to file */
static void persist_save_feedback_from_avl(FILE* fp, AVL_Node* root) {
    if (!root) return;
    persist_save_feedback_from_avl(fp, root->left);
    Feedback* f = (Feedback*)root->data;
    persist_write_feedback(fp, f);
    persist_save_feedback_from_avl(fp, root->right);
}

/* Save departments: in-order traverse departments and write PersistDepartment + members */
static void persist_write_departments_to_fp(AVL_Node* root, FILE* fp) {
    if (!root) return;
    persist_write_departments_to_fp(root->left, fp);
    Department* d = (Department*)root->data;
    PersistDepartment pd;
    pd.dept_id = d->dept_id;
    strncpy(pd.name, d->name, sizeof(pd.name)-1); pd.name[sizeof(pd.name)-1] = '\0';
    pd.member_count = d->member_count;
    fwrite(&pd, sizeof(PersistDepartment), 1, fp);

    /* Write members belonging to this department: iterate all chains and write ones matching owner */
    for (int i = 0; i < d->member_map->size; ++i) {
        AVLChain* cur = d->member_map->buckets[i];
        while (cur) {
            if (cur->owner_type == OWNER_DEPARTMENT && cur->owner_id == d->dept_id) {
                persist_save_members_from_avl(fp, cur->tree);
            }
            cur = cur->next;
        }
    }
    persist_write_departments_to_fp(root->right, fp);
}

/* Save all departments into departments.dat */
static void persist_save_departments(IT_Club_System* sys) {
    FILE* fp = fopen("departments.dat", "wb");
    if (!fp) { printf("Error: cannot open departments.dat for writing.\n"); return; }
    int dept_count = avl_count_nodes(sys->department_tree);
    fwrite(&dept_count, sizeof(int), 1, fp);
    persist_write_departments_to_fp(sys->department_tree, fp);
    fclose(fp);
}

/* Save events similarly: write PersistEvent and member & feedback blocks */
static void persist_write_events_to_fp(AVL_Node* root, FILE* fp) {
    if (!root) return;
    persist_write_events_to_fp(root->left, fp);
    Event* e = (Event*)root->data;
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
    fwrite(&pe, sizeof(PersistEvent), 1, fp);

    /* Members */
    for (int i = 0; i < e->member_map->size; ++i) {
        AVLChain* cur = e->member_map->buckets[i];
        while (cur) {
            if (cur->owner_type == OWNER_EVENT_MEMBERS && cur->owner_id == e->event_id) {
                persist_save_members_from_avl(fp, cur->tree);
            }
            cur = cur->next;
        }
    }

    /* Feedback */
    for (int i = 0; i < e->feedback_map->size; ++i) {
        AVLChain* cur = e->feedback_map->buckets[i];
        while (cur) {
            if (cur->owner_type == OWNER_EVENT_FEEDBACK && cur->owner_id == e->event_id) {
                persist_save_feedback_from_avl(fp, cur->tree);
            }
            cur = cur->next;
        }
    }
    persist_write_events_to_fp(root->right, fp);
}

/* Save all events to events.dat */
static void persist_save_events(IT_Club_System* sys) {
    FILE* fp = fopen("events.dat", "wb");
    if (!fp) { printf("Error: cannot open events.dat for writing.\n"); return; }
    int event_count = avl_count_nodes(sys->event_tree);
    fwrite(&event_count, sizeof(int), 1, fp);
    persist_write_events_to_fp(sys->event_tree, fp);
    fclose(fp);
}

/* Save global feedback archive: write total feedback count then PersistFeedback entries */
static void persist_write_feedback_archive_for_event(AVL_Node* root, FILE* fp) {
    if (!root) return;
    persist_write_feedback_archive_for_event(root->left, fp);
    Event* e = (Event*)root->data;
    for (int i = 0; i < e->feedback_map->size; ++i) {
        AVLChain* cur = e->feedback_map->buckets[i];
        while (cur) {
            if (cur->owner_type == OWNER_EVENT_FEEDBACK && cur->owner_id == e->event_id) {
                /* write all feedback nodes from this AVL */
                /* in-order traversal */
                /* we'll reuse a small traversal here */
                /* note: we write PersistFeedback (includes event_id) */
                /* helper nested traversal implemented outside */
                persist_save_feedback_from_avl(fp, cur->tree);
            }
            cur = cur->next;
        }
    }
    persist_write_feedback_archive_for_event(root->right, fp);
}

/* Count total feedback across all events */
static int persist_count_total_feedback(AVL_Node* root) {
    if (!root) return 0;
    Event* e = (Event*)root->data;
    int left = persist_count_total_feedback(root->left);
    int right = persist_count_total_feedback(root->right);
    return left + right + e->feedback_count;
}

/* Save global feedback.dat as (count followed by PersistFeedback entries)
   For simplicity we write PersistFeedback via a two-step: fetch event->feedback_map and write event_id + Feedback.
   We'll implement a traversal that writes event_id followed by its feedback structs. */
static void persist_save_global_feedback(IT_Club_System* sys) {
    FILE* fp = fopen("feedback.dat", "wb");
    if (!fp) { printf("Error: cannot open feedback.dat for writing.\n"); return; }
    int total = persist_count_total_feedback(sys->event_tree);
    fwrite(&total, sizeof(int), 1, fp);

    /* For each event in-order, write its feedbacks as PersistFeedback: event_id + Feedback */
    /* Implemented by traversing event tree and writing feedback nodes explicitly */
    /* We will use a recursive writer that writes each PersistFeedback */
    /* Helper defined below: persist_write_feedback_nodes_for_event */
    /* Iterate: in-order traverse events and for each event, traverse its chains and write PersistFeedback entries */
    /* We'll write inline here using helper loops */

    /* In-order loop using explicit function to avoid nested helpers (extracted) */
    /* Define a small local recursive function via named external helper implemented below */
    /* We'll use persist_write_feedbacks_for_event which accepts an Event* and FILE* */
    /* Implemented below (after function prototype) */
    /* We'll call through event tree traversal below */

    /* We'll call an external helper: persist_write_feedbacks_for_event(Event*, FILE*) */
    extern void persist_write_feedbacks_for_event(Event* e, FILE* fp); /* forward */
    /* Traverse events in-order and call helper */
    /* We implement a generic in-order traversal with function pointer callback */
    /* Use existing avl_inorder but it passes void* data - so define small wrapper callback that calls the helper */
    /* We'll create a static stateful pointer to FILE* for that callback - use static globals for this small scope */
    /* To keep single-file helpers extracted, implement a function persist_write_feedback_archive_traverse that uses above helper */
    extern void persist_write_feedback_archive_traverse(AVL_Node* root, FILE* fp);
    persist_write_feedback_archive_traverse(sys->event_tree, fp);

    fclose(fp);
}

/* Definitions for external helper used above (extracted below) */
void persist_write_feedbacks_for_event(Event* e, FILE* fp);
void persist_write_feedback_archive_traverse(AVL_Node* root, FILE* fp);

/* Save all data (departments, events, feedback) */
static void persist_save_all(IT_Club_System* sys) {
    persist_save_departments(sys);
    persist_save_events(sys);
    persist_save_global_feedback(sys);
    printf("Saved: departments.dat, events.dat, feedback.dat\n");
}

/* ===========================
   Persistence: Load helpers (extracted)
   Reverse of save:
    - load departments.dat: count then PersistDepartment + members
    - load events.dat: count then PersistEvent + members + feedbacks
    - load feedback.dat: count then PersistFeedback + attach to events
   NOTE: For simplicity these loaders append to existing structures.
         In production you might want to free/clear previous memory before loading.
   =========================== */

/* Load members from file into provided owner (Department/Event) using owner metadata */
static int persist_load_member_into_owner(FILE* fp, int owner_type, int owner_id, Department* dept_owner, Event* event_owner) {
    Member* m = (Member*)malloc(sizeof(Member));
    if (fread(m, sizeof(Member), 1, fp) != 1) { free(m); return 0; }
    if (owner_type == OWNER_DEPARTMENT && dept_owner) {
        hmchain_insert(dept_owner->member_map, m->member_id, m, OWNER_DEPARTMENT, dept_owner->dept_id);
        dept_owner->member_count++;
    } else if (owner_type == OWNER_EVENT_MEMBERS && event_owner) {
        hmchain_insert(event_owner->member_map, m->member_id, m, OWNER_EVENT_MEMBERS, event_owner->event_id);
        event_owner->member_count++;
    } else {
        /* unknown owner, free */
        free(m);
        return 0;
    }
    return 1;
}

/* Load feedback entry and attach to event */
static int persist_load_feedback_attach(FILE* fp, Event* e) {
    Feedback* f = (Feedback*)malloc(sizeof(Feedback));
    if (fread(f, sizeof(Feedback), 1, fp) != 1) { free(f); return 0; }
    hmchain_insert(e->feedback_map, f->feedback_id, f, OWNER_EVENT_FEEDBACK, e->event_id);
    e->feedback_count++;
    return 1;
}

/* Load departments.dat into system */
static void persist_load_departments(IT_Club_System* sys) {
    FILE* fp = fopen("departments.dat", "rb");
    if (!fp) return;
    int dept_count = 0;
    if (fread(&dept_count, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    for (int i = 0; i < dept_count; ++i) {
        PersistDepartment pd;
        if (fread(&pd, sizeof(PersistDepartment), 1, fp) != 1) break;
        Department* d = department_create(pd.dept_id, pd.name);
        for (int j = 0; j < pd.member_count; ++j) {
            Member* m = (Member*)malloc(sizeof(Member));
            if (fread(m, sizeof(Member), 1, fp) != 1) { free(m); break; }
            hmchain_insert(d->member_map, m->member_id, m, OWNER_DEPARTMENT, d->dept_id);
            d->member_count++;
        }
        sys->department_tree = avl_insert(sys->department_tree, d->dept_id, d);
    }
    fclose(fp);
}

/* Load events.dat into system */
static void persist_load_events(IT_Club_System* sys) {
    FILE* fp = fopen("events.dat", "rb");
    if (!fp) return;
    int event_count = 0;
    if (fread(&event_count, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    for (int i = 0; i < event_count; ++i) {
        PersistEvent pe;
        if (fread(&pe, sizeof(PersistEvent), 1, fp) != 1) break;
        Event* e = event_create(pe.event_id, pe.name, pe.venue, pe.date, pe.budget, pe.description);
        e->expenses = pe.expenses;
        e->profit_loss = pe.profit_loss;
        /* Members */
        for (int j = 0; j < pe.member_count; ++j) {
            Member* m = (Member*)malloc(sizeof(Member));
            if (fread(m, sizeof(Member), 1, fp) != 1) { free(m); break; }
            hmchain_insert(e->member_map, m->member_id, m, OWNER_EVENT_MEMBERS, e->event_id);
            e->member_count++;
        }
        /* Feedback */
        for (int j = 0; j < pe.feedback_count; ++j) {
            Feedback* fb = (Feedback*)malloc(sizeof(Feedback));
            if (fread(fb, sizeof(Feedback), 1, fp) != 1) { free(fb); break; }
            hmchain_insert(e->feedback_map, fb->feedback_id, fb, OWNER_EVENT_FEEDBACK, e->event_id);
            e->feedback_count++;
        }
        sys->event_tree = avl_insert(sys->event_tree, e->event_id, e);
    }
    fclose(fp);
}

/* Helper: persist_read_persistfeedback and attach to events */
static void persist_load_global_feedback(IT_Club_System* sys) {
    FILE* fp = fopen("feedback.dat", "rb");
    if (!fp) return;
    int total = 0;
    if (fread(&total, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    for (int i = 0; i < total; ++i) {
        PersistFeedback pf;
        if (fread(&pf, sizeof(PersistFeedback), 1, fp) != 1) break;
        Event* e = event_find(sys, pf.event_id);
        if (!e) continue;
        /* Skip duplicates */
        void* exists = hmchain_get(e->feedback_map, pf.fb.feedback_id, OWNER_EVENT_FEEDBACK, e->event_id);
        if (exists) continue;
        Feedback* fb = (Feedback*)malloc(sizeof(Feedback));
        *fb = pf.fb;
        hmchain_insert(e->feedback_map, fb->feedback_id, fb, OWNER_EVENT_FEEDBACK, e->event_id);
        e->feedback_count++;
    }
    fclose(fp);
}

/* Load all data (departments, events, feedback) */
static void persist_load_all(IT_Club_System* sys) {
    persist_load_departments(sys);
    persist_load_events(sys);
    persist_load_global_feedback(sys);
    printf("Loaded. Departments: %d  Events: %d\n", avl_count_nodes(sys->department_tree), avl_count_nodes(sys->event_tree));
}

/* External helper implementations referenced earlier */

/* Write PersistFeedback entries for a single Event into file (in-order across chains) */
void persist_write_feedbacks_for_event(Event* e, FILE* fp) {
    if (!e || !fp) return;
    for (int i = 0; i < e->feedback_map->size; ++i) {
        AVLChain* cur = e->feedback_map->buckets[i];
        while (cur) {
            if (cur->owner_type == OWNER_EVENT_FEEDBACK && cur->owner_id == e->event_id) {
                /* In-order traversal writing PersistFeedback records */
                /* We'll implement a small stack-free traversal by recursion using existing helper persist_save_feedback_from_avl,
                   but we must write PersistFeedback (event_id + Feedback) */
                /* Let's create a helper that writes PersistFeedback for a given AVL root */
                /* We'll implement that helper below and call it here */
                extern void persist_write_pfeedbacks_from_avl(FILE* fp, AVL_Node* root, int event_id);
                persist_write_pfeedbacks_from_avl(fp, cur->tree, e->event_id);
            }
            cur = cur->next;
        }
    }
}

/* Write PersistFeedback entries (helper writing event_id + Feedback for each node in-order) */
void persist_write_pfeedbacks_from_avl(FILE* fp, AVL_Node* root, int event_id) {
    if (!root || !fp) return;
    persist_write_pfeedbacks_from_avl(fp, root->left, event_id);
    Feedback* f = (Feedback*)root->data;
    PersistFeedback pf;
    pf.event_id = event_id;
    pf.fb = *f;
    fwrite(&pf, sizeof(PersistFeedback), 1, fp);
    persist_write_pfeedbacks_from_avl(fp, root->right, event_id);
}

/* In-order traverse event tree and write PersistFeedbacks using helper */
void persist_write_feedback_archive_traverse(AVL_Node* root, FILE* fp) {
    if (!root || !fp) return;
    persist_write_feedback_archive_traverse(root->left, fp);
    Event* e = (Event*)root->data;
    persist_write_feedbacks_for_event(e, fp);
    persist_write_feedback_archive_traverse(root->right, fp);
}

/* ===========================
   Encryption / Decryption helpers
   Simple XOR stream cipher (symmetric)
   =========================== */

/* Generate key */
static int crypto_generate_key() {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    return rand() % 255 + 1; /* 1..255 */
}

/* Encrypt file (input -> output) with XOR key. Skips missing input gracefully. */
static void crypto_encrypt_file(const char* inputFile, const char* outputFile, int key) {
    FILE* in = fopen(inputFile, "rb");
    if (!in) { printf("[WARN] %s not present (skipped)\n", inputFile); return; }
    FILE* out = fopen(outputFile, "wb");
    if (!out) { printf("[ERROR] cannot open %s for writing\n", outputFile); fclose(in); return; }
    int c;
    while ((c = fgetc(in)) != EOF) fputc(c ^ key, out);
    fclose(in); fclose(out);
    printf("[OK] Encrypted %s -> %s\n", inputFile, outputFile);
}

/* Decrypt file (input -> output) symmetric */
static void crypto_decrypt_file(const char* inputFile, const char* outputFile, int key) {
    /* XOR symmetric */
    crypto_encrypt_file(inputFile, outputFile, key);
    printf("[OK] Decrypted %s -> %s\n", inputFile, outputFile);
}

/* Encrypt all .dat files to .enc */
static void crypto_encrypt_all(int key) {
    printf("\n--- Encrypting all .dat files ---\n");
    crypto_encrypt_file("departments.dat", "departments.enc", key);
    crypto_encrypt_file("events.dat", "events.enc", key);
    crypto_encrypt_file("feedback.dat", "feedback.enc", key);
    printf("--- Encryption finished ---\n");
}

/* Decrypt all .enc files to .dat */
static void crypto_decrypt_all(int key) {
    printf("\n--- Decrypting all .enc files ---\n");
    crypto_decrypt_file("departments.enc", "departments.dat", key);
    crypto_decrypt_file("events.enc", "events.dat", key);
    crypto_decrypt_file("feedback.enc", "feedback.dat", key);
    printf("--- Decryption finished ---\n");
}

/* ===========================
   Auto-save thread
   =========================== */

static void* autosave_thread_func(void* arg) {
    IT_Club_System* sys = (IT_Club_System*)arg;
    while (1) {
        sleep(120); /* 2 minutes */
        pthread_mutex_lock(&sys->data_lock);
        printf("\n[Auto-save] Saving all data...\n");
        persist_save_all(sys);
        pthread_mutex_unlock(&sys->data_lock);
    }
    return NULL;
}

/* ===========================
   Reporting helpers (heap)
   =========================== */

/* Simple max-heap for top-K by profit_loss */
typedef struct Heap {
    void** data;
    double* scores;
    int size;
    int capacity;
} Heap;

static Heap* heap_create(int capacity) {
    Heap* h = (Heap*)malloc(sizeof(Heap));
    h->data = (void**)malloc(capacity * sizeof(void*));
    h->scores = (double*)malloc(capacity * sizeof(double));
    h->size = 0; h->capacity = capacity;
    return h;
}
static void heap_swap(Heap* h, int i, int j) {
    double t = h->scores[i]; h->scores[i] = h->scores[j]; h->scores[j] = t;
    void* td = h->data[i]; h->data[i] = h->data[j]; h->data[j] = td;
}
static void heapify_up(Heap* h, int idx) {
    while (idx > 0) {
        int p = (idx - 1) / 2;
        if (h->scores[idx] > h->scores[p]) { heap_swap(h, idx, p); idx = p; } else break;
    }
}
static void heapify_down(Heap* h, int idx) {
    while (1) {
        int largest = idx, l = 2*idx + 1, r = 2*idx + 2;
        if (l < h->size && h->scores[l] > h->scores[largest]) largest = l;
        if (r < h->size && h->scores[r] > h->scores[largest]) largest = r;
        if (largest != idx) { heap_swap(h, idx, largest); idx = largest; } else break;
    }
}
static void heap_insert(Heap* h, double score, void* data) {
    if (h->size >= h->capacity) return;
    h->scores[h->size] = score;
    h->data[h->size] = data;
    heapify_up(h, h->size);
    h->size++;
}
static void* heap_extract_max(Heap* h, double* out_score) {
    if (h->size == 0) return NULL;
    if (out_score) *out_score = h->scores[0];
    void* d = h->data[0];
    h->scores[0] = h->scores[h->size-1];
    h->data[0] = h->data[h->size-1];
    h->size--;
    heapify_down(h, 0);
    return d;
}

/* Collect events into heap */
static void reporting_collect_events(AVL_Node* root, Heap* h) {
    if (!root) return;
    reporting_collect_events(root->left, h);
    Event* e = (Event*)root->data;
    heap_insert(h, e->profit_loss, e);
    reporting_collect_events(root->right, h);
}

/* Print top-K events by profit/loss */
static void reporting_top_k(IT_Club_System* sys, int k) {
    Heap* h = heap_create(1024);
    reporting_collect_events(sys->event_tree, h);
    printf("\n=== Top %d Events by Profit/Loss ===\n", k);
    for (int i = 0; i < k && h->size > 0; ++i) {
        double sc; Event* e = (Event*)heap_extract_max(h, &sc);
        printf("%d. %s (ID:%d) — Profit: $%.2f\n", i+1, e->name, e->event_id, sc);
    }
    free(h->data); free(h->scores); free(h);
}

/* ===========================
   Input helpers (extract common I/O)
   =========================== */

/* Read a trimmed line from stdin into buffer (removes trailing newline) */
static void input_read_line(char* buf, int size) {
    if (!fgets(buf, size, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\n")] = '\0';
}

/* Read integer with validation; returns 1 on success and sets *out; leaves stdin newline consumed */
static int input_read_int(int* out) {
    if (scanf("%d", out) != 1) { while (getchar() != '\n'); return 0; }
    while (getchar() != '\n'); /* consume newline */
    return 1;
}

/* Read double with simple validation */
static int input_read_double(double* out) {
    if (scanf("%lf", out) != 1) { while (getchar() != '\n'); return 0; }
    while (getchar() != '\n');
    return 1;
}

/* ===========================
   Interactive menu actions (use extracted helpers)
   =========================== */

/* Create event interactively */
static void interactive_create_event(IT_Club_System* sys) {
    int event_id;
    char name[100], venue[100], date[20], desc[256];
    double budget, expenses;

    /* Event ID (user-provided) */
    while (1) {
        printf("Enter event ID (integer): ");
        if (!input_read_int(&event_id)) { 
            printf("Invalid. Try again.\n"); 
            continue; 
        }
        if (event_exists(sys, event_id)) { 
            printf("Event ID exists, provide unique ID.\n"); 
            continue; 
        }
        break;
    }

    /* Basic details */
    printf("Enter event name: "); 
    input_read_line(name, sizeof(name));

    printf("Enter venue: "); 
    input_read_line(venue, sizeof(venue));

    printf("Enter date (DD/MM/YYYY): "); 
    input_read_line(date, sizeof(date));

    /* Financial inputs */
    printf("Enter budget: ");
    if (!input_read_double(&budget)) {
        printf("Invalid input. Setting budget = 0.\n");
        budget = 0.0;
    }

    printf("Enter expenses: ");
    if (!input_read_double(&expenses)) {
        printf("Invalid input. Setting expenses = 0.\n");
        expenses = 0.0;
    }

    /* Ensure expenses don't exceed budget (optional rule) */
    if (expenses > budget) {
        printf("Warning: Expenses exceed budget. Profit will be negative.\n");
    }

    double profit_loss = budget - expenses;  // AUTO CALCULATION

    printf("Enter description: ");
    input_read_line(desc, sizeof(desc));

    /* Create new event */
    Event* e = event_create(event_id, name, venue, date, budget, desc);

    /* Override financial fields */
    e->expenses = expenses;
    e->profit_loss = profit_loss;

    /* Add to system */
    event_add(sys, e);

    printf("Event '%s' created (ID:%d). Profit/Loss calculated: %.2f\n", 
           name, event_id, profit_loss);
}


/* Create department interactively */
static void interactive_create_department(IT_Club_System* sys) {
    int dept_id;
    char name[100];
    while (1) {
        printf("Enter department ID (integer): ");
        if (!input_read_int(&dept_id)) { printf("Invalid. Try again.\n"); continue; }
        if (department_exists(sys, dept_id)) { printf("Department ID exists, provide unique ID.\n"); continue; }
        break;
    }
    printf("Enter department name: "); input_read_line(name, sizeof(name));
    Department* d = department_create(dept_id, name);
    department_add(sys, d);
    printf("Department '%s' created (ID:%d)\n", name, dept_id);
}

/* Add member to department interactively */
static void interactive_add_member_department(IT_Club_System* sys) {
    int dept_id;
    printf("Enter department ID: ");
    if (!input_read_int(&dept_id)) return;
    Department* d = department_find(sys, dept_id);
    if (!d) { printf("Department not found.\n"); return; }
    Member m;
    while (1) {
        printf("Enter member ID: ");
        if (!input_read_int(&m.member_id)) { printf("Invalid.\n"); continue; }
        void* exists = hmchain_get(d->member_map, m.member_id, OWNER_DEPARTMENT, d->dept_id);
        if (exists) { printf("Member ID exists in this department. Provide unique ID.\n"); continue; }
        break;
    }
    printf("Enter member name: "); input_read_line(m.name, sizeof(m.name));
    printf("Enter role: "); input_read_line(m.role, sizeof(m.role));
    printf("Enter contact: "); input_read_line(m.contact, sizeof(m.contact));
    strncpy(m.department_name, d->name, sizeof(m.department_name)-1); m.department_name[sizeof(m.department_name)-1] = '\0';
    if (department_add_member(d, &m)) printf("Member %s (ID:%d) added to department %s\n", m.name, m.member_id, d->name);
    else printf("Failed to add member.\n");
}

/* Add member to event interactively */
static void interactive_add_member_event(IT_Club_System* sys) {
    int event_id;
    printf("Enter event ID: "); if (!input_read_int(&event_id)) return;
    Event* e = event_find(sys, event_id);
    if (!e) { printf("Event not found.\n"); return; }
    Member m;
    while (1) {
        printf("Enter member ID: ");
        if (!input_read_int(&m.member_id)) { printf("Invalid.\n"); continue; }
        void* exists = hmchain_get(e->member_map, m.member_id, OWNER_EVENT_MEMBERS, e->event_id);
        if (exists) { printf("Member ID exists for this event. Provide unique ID.\n"); continue; }
        break;
    }
    printf("Enter member name: "); input_read_line(m.name, sizeof(m.name));
    printf("Enter role: "); input_read_line(m.role, sizeof(m.role));
    printf("Enter contact: "); input_read_line(m.contact, sizeof(m.contact));
    printf("Enter department name (for record): "); input_read_line(m.department_name, sizeof(m.department_name));
    if (event_add_member(e, &m)) printf("Member %s (ID:%d) added to event %s\n", m.name, m.member_id, e->name);
    else printf("Failed to add member.\n");
}

/* Remove member from event interactively */
static void interactive_remove_member_event(IT_Club_System* sys) {
    int event_id, member_id;
    printf("Enter event ID: "); if (!input_read_int(&event_id)) return;
    printf("Enter member ID to remove: "); if (!input_read_int(&member_id)) return;
    Event* e = event_find(sys, event_id);
    if (!e) { printf("Event not found.\n"); return; }
    event_delete_member(e, member_id);
    printf("Attempted to remove member ID %d from event ID %d.\n", member_id, event_id);
}

/* Add feedback interactively */
static void interactive_add_feedback(IT_Club_System* sys) {
    int event_id;
    printf("Enter event ID: "); if (!input_read_int(&event_id)) return;
    Event* e = event_find(sys, event_id);
    if (!e) { printf("Event not found.\n"); return; }
    Feedback fb;
    while (1) {
        printf("Enter feedback ID: ");
        if (!input_read_int(&fb.feedback_id)) { printf("Invalid.\n"); continue; }
        void* exists = hmchain_get(e->feedback_map, fb.feedback_id, OWNER_EVENT_FEEDBACK, e->event_id);
        if (exists) { printf("Feedback ID exists for this event. Provide unique ID.\n"); continue; }
        break;
    }
    printf("Enter member ID (author): "); if (!input_read_int(&fb.member_id)) fb.member_id = 0;
    printf("Enter rating (1-5): "); if (!input_read_int(&fb.rating)) fb.rating = 0;
    printf("Enter comment: "); input_read_line(fb.comment, sizeof(fb.comment));
    if (event_add_feedback(e, &fb)) printf("Feedback %d added to event %s\n", fb.feedback_id, e->name);
    else printf("Failed to add feedback.\n");
}

/* Find event by ID (fast via AVL search) and display details (helper) */
static void interactive_find_event(IT_Club_System* sys) {
    int id;
    printf("Enter event ID: "); if (!input_read_int(&id)) return;
    Event* e = event_find(sys, id);
    if (!e) { printf("Event not found.\n"); return; }
    display_event_basic(e);
}

/* Display all events */
static void interactive_display_all_events(IT_Club_System* sys) {
    if (!sys->event_tree) { printf("No events exist.\n"); return; }
    avl_inorder(sys->event_tree, display_event_basic);
}

/* Display all departments */
static void interactive_display_all_departments(IT_Club_System* sys) {
    if (!sys->department_tree) { printf("No departments exist.\n"); return; }
    avl_inorder(sys->department_tree, display_department_basic);
}

/* Display members in department (interactive wrapper) */
static void interactive_display_members_department(IT_Club_System* sys) {
    int dept_id;
    printf("Enter department ID: "); if (!input_read_int(&dept_id)) return;
    Department* d = department_find(sys, dept_id);
    if (!d) { printf("Department not found.\n"); return; }
    department_display_members(d);
}

/* Display event members (interactive wrapper) */
static void interactive_display_members_event(IT_Club_System* sys) {
    int event_id;
    printf("Enter event ID: "); if (!input_read_int(&event_id)) return;
    Event* e = event_find(sys, event_id);
    if (!e) { printf("Event not found.\n"); return; }
    event_display_members(e);
}

/* Display feedback for event (interactive wrapper) */
static void interactive_display_feedback_event(IT_Club_System* sys) {
    int event_id;
    printf("Enter event ID: "); if (!input_read_int(&event_id)) return;
    Event* e = event_find(sys, event_id);
    if (!e) { printf("Event not found.\n"); return; }
    event_display_feedback(e);
}

/* Export & Encrypt (interactive wrapper) */
static void interactive_encrypt_all(IT_Club_System* sys) {
    int key = crypto_generate_key();
    printf("Encryption key (save securely): %d\n", key);
    /* Save before encryption to ensure .dat files are current */
    persist_save_all(sys);
    crypto_encrypt_all(key);
}

/* Import & Decrypt (interactive wrapper) */
static void interactive_decrypt_all(IT_Club_System* sys) {
    int key;
    printf("Enter decryption key: "); if (!input_read_int(&key)) return;
    crypto_decrypt_all(key);
    /* After decrypt, reload */
    persist_load_all(sys);
}

/* Save & Exit wrapper */
static void interactive_save_and_exit(IT_Club_System* sys) {
    persist_save_all(sys);
    printf("Data saved. Exiting.\n");
    exit(0);
}

/* ===========================
   Menu & main
   =========================== */

/* Display menu (kept same as requested) */
static void display_menu() {
    printf("\n========== IT Club Management System ==========\n");
    printf("1.  Create Event (user-provided ID)\n");
    printf("2.  Create Department (user-provided ID)\n");
    printf("3.  Add Member to Department (user-provided member ID)\n");
    printf("4.  Add Member to Event (user-provided member ID)\n");
    printf("5.  Remove Member from Event\n");
    printf("6.  Add Feedback to Event (user-provided feedback ID)\n");
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
    printf("===============================================\n");
    printf("Enter choice: ");
}

/* Main */
int main(void) {
    IT_Club_System* sys = system_create();
    printf("Loading existing data (if any)...\n");
    persist_load_all(sys);

    /* Start autosave thread */
    pthread_t tid;
    if (pthread_create(&tid, NULL, autosave_thread_func, sys) == 0) {
        pthread_detach(tid);
    } else {
        printf("[WARN] Auto-save thread creation failed.\n");
    }

    int choice;
    while (1) {
        display_menu();
        if (!input_read_int(&choice)) { printf("Invalid input.\n"); continue; }
        pthread_mutex_lock(&sys->data_lock);
        switch (choice) {
            case 1: interactive_create_event(sys); break;
            case 2: interactive_create_department(sys); break;
            case 3: interactive_add_member_department(sys); break;
            case 4: interactive_add_member_event(sys); break;
            case 5: interactive_remove_member_event(sys); break;
            case 6: interactive_add_feedback(sys); break;
            case 7: interactive_find_event(sys); break;
            case 8: interactive_display_all_events(sys); break;
            case 9: interactive_display_all_departments(sys); break;
            case 10: interactive_display_members_department(sys); break;
            case 11: interactive_display_members_event(sys); break;
            case 12: interactive_display_feedback_event(sys); break;
            case 13: reporting_top_k(sys, 3); break;
            case 14: interactive_encrypt_all(sys); break;
            case 15: interactive_decrypt_all(sys); break;
            case 16: interactive_save_and_exit(sys); break;
            default: printf("Invalid choice.\n"); break;
        }
        pthread_mutex_unlock(&sys->data_lock);
    }
    return 0;
}
