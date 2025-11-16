#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* ----------------------------- Basic AVL & Heap ----------------------------- */

typedef struct AVL_Node {
    int key;
    void* data;
    int height;
    struct AVL_Node* left;
    struct AVL_Node* right;
} AVL_Node;

int max(int a, int b) { return (a > b) ? a : b; }
int getHeight(AVL_Node* node) { return node ? node->height : 0; }
int getBalanceFactor(AVL_Node* node) { return node ? getHeight(node->left) - getHeight(node->right) : 0; }

AVL_Node* rightRotate(AVL_Node* y) {
    AVL_Node* x = y->left;
    AVL_Node* T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = max(getHeight(y->left), getHeight(y->right)) + 1;
    x->height = max(getHeight(x->left), getHeight(x->right)) + 1;
    return x;
}

AVL_Node* leftRotate(AVL_Node* x) {
    AVL_Node* y = x->right;
    AVL_Node* T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = max(getHeight(x->left), getHeight(x->right)) + 1;
    y->height = max(getHeight(y->left), getHeight(y->right)) + 1;
    return y;
}

AVL_Node* insertNode(AVL_Node* node, int key, void* data) {
    if (!node) {
        AVL_Node* newNode = (AVL_Node*)malloc(sizeof(AVL_Node));
        newNode->key = key;
        newNode->data = data;
        newNode->height = 1;
        newNode->left = newNode->right = NULL;
        return newNode;
    }
    if (key < node->key)
        node->left = insertNode(node->left, key, data);
    else if (key > node->key)
        node->right = insertNode(node->right, key, data);
    else {
        /* duplicate keys: caller should avoid inserting duplicates */
        return node;
    }

    node->height = 1 + max(getHeight(node->left), getHeight(node->right));
    int balance = getBalanceFactor(node);

    if (balance > 1 && key < node->left->key) return rightRotate(node);
    if (balance < -1 && key > node->right->key) return leftRotate(node);
    if (balance > 1 && key > node->left->key) {
        node->left = leftRotate(node->left);
        return rightRotate(node);
    }
    if (balance < -1 && key < node->right->key) {
        node->right = rightRotate(node->right);
        return leftRotate(node);
    }
    return node;
}

AVL_Node* minValueNode(AVL_Node* node) {
    AVL_Node* current = node;
    while (current && current->left) current = current->left;
    return current;
}

AVL_Node* deleteNode(AVL_Node* root, int key) {
    if (!root) return root;
    if (key < root->key) root->left = deleteNode(root->left, key);
    else if (key > root->key) root->right = deleteNode(root->right, key);
    else {
        if (!root->left || !root->right) {
            AVL_Node* temp = root->left ? root->left : root->right;
            if (!temp) {
                temp = root;
                root = NULL;
            } else *root = *temp;
            free(temp);
        } else {
            AVL_Node* temp = minValueNode(root->right);
            root->key = temp->key;
            root->data = temp->data;
            root->right = deleteNode(root->right, temp->key);
        }
    }
    if (!root) return root;
    root->height = 1 + max(getHeight(root->left), getHeight(root->right));
    int balance = getBalanceFactor(root);
    if (balance > 1 && getBalanceFactor(root->left) >= 0) return rightRotate(root);
    if (balance > 1 && getBalanceFactor(root->left) < 0) { root->left = leftRotate(root->left); return rightRotate(root); }
    if (balance < -1 && getBalanceFactor(root->right) <= 0) return leftRotate(root);
    if (balance < -1 && getBalanceFactor(root->right) > 0) { root->right = rightRotate(root->right); return leftRotate(root); }
    return root;
}

AVL_Node* searchAVL(AVL_Node* root, int key) {
    if (!root || root->key == key) return root;
    if (key < root->key) return searchAVL(root->left, key);
    return searchAVL(root->right, key);
}

void inOrder(AVL_Node* root, void (*callback)(void*)) {
    if (!root) return;
    inOrder(root->left, callback);
    if (callback) callback(root->data);
    inOrder(root->right, callback);
}

/* ----------------------------- Domain models ----------------------------- */

typedef struct Member {
    int member_id;
    char name[100];
    char role[50];
    char contact[15];
    char department_name[50];
} Member;

typedef struct Feedback {
    int feedback_id;
    int member_id;
    int rating;
    char comment[256];
} Feedback;

typedef struct HashMap_Chain HashMap_Chain; /* forward */

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

typedef struct Department {
    int dept_id;
    char name[100];
    HashMap_Chain* member_map;
    int member_count;
} Department;

/* Persistable structs (no pointers) */
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

/* ----------------------------- HashMap_Chain: chaining of AVL trees ----------------------------- */

typedef enum {
    OWNER_DEPARTMENT = 1,
    OWNER_EVENT_MEMBERS = 2,
    OWNER_EVENT_FEEDBACK = 3
} OwnerType;

typedef struct AVLChain {
    AVL_Node* tree;        /* AVL tree storing records keyed by record id (member_id or feedback_id) */
    int owner_type;
    int owner_id;
    struct AVLChain* next;
} AVLChain;

struct HashMap_Chain {
    AVLChain** buckets;
    int size;
};

int hashInt(int key, int size) { return abs(key) % size; }

HashMap_Chain* create_HashMap_Chain(int size) {
    HashMap_Chain* map = (HashMap_Chain*)malloc(sizeof(HashMap_Chain));
    map->size = size;
    map->buckets = (AVLChain**)calloc(size, sizeof(AVLChain*));
    return map;
}

AVLChain* findChainInBucket(AVLChain* head, int owner_type, int owner_id) {
    AVLChain* cur = head;
    while (cur) {
        if (cur->owner_type == owner_type && cur->owner_id == owner_id) return cur;
        cur = cur->next;
    }
    return NULL;
}

AVLChain* createAndPrependChain(HashMap_Chain* map, int bucket_idx, int owner_type, int owner_id) {
    AVLChain* chain = (AVLChain*)malloc(sizeof(AVLChain));
    chain->tree = NULL;
    chain->owner_type = owner_type;
    chain->owner_id = owner_id;
    chain->next = map->buckets[bucket_idx];
    map->buckets[bucket_idx] = chain;
    return chain;
}

/* Insert record into the chain that matches owner_type + owner_id; create chain if absent */
void hashChain_insert(HashMap_Chain* map, int key, void* data, int owner_type, int owner_id) {
    if (!map) return;
    int idx = hashInt(key, map->size);
    AVLChain* chain = findChainInBucket(map->buckets[idx], owner_type, owner_id);
    if (!chain) chain = createAndPrependChain(map, idx, owner_type, owner_id);
    AVL_Node* found = searchAVL(chain->tree, key);
    if (found) {
        found->data = data; /* update */
        return;
    }
    chain->tree = insertNode(chain->tree, key, data);
}

/* Get record by searching in the chain that matches owner_type+owner_id */
void* hashChain_get(HashMap_Chain* map, int key, int owner_type, int owner_id) {
    if (!map) return NULL;
    int idx = hashInt(key, map->size);
    AVLChain* chain = findChainInBucket(map->buckets[idx], owner_type, owner_id);
    if (!chain) return NULL;
    AVL_Node* node = searchAVL(chain->tree, key);
    return node ? node->data : NULL;
}

/* Delete record from owner-specific chain */
void hashChain_delete(HashMap_Chain* map, int key, int owner_type, int owner_id) {
    if (!map) return;
    int idx = hashInt(key, map->size);
    AVLChain* chain = findChainInBucket(map->buckets[idx], owner_type, owner_id);
    if (!chain) return;
    chain->tree = deleteNode(chain->tree, key);
}

/* Traverse all chains and call callback only for chains matching owner_type & owner_id */
void hashChain_traverseOwner(HashMap_Chain* map, int owner_type, int owner_id, void (*nodeCallback)(void*)) {
    if (!map) return;
    for (int i = 0; i < map->size; i++) {
        AVLChain* cur = map->buckets[i];
        while (cur) {
            if (cur->owner_type == owner_type && cur->owner_id == owner_id) {
                inOrder(cur->tree, nodeCallback);
            }
            cur = cur->next;
        }
    }
}

/* Helper to count total nodes in an AVL tree */
int countTreeNodes(AVL_Node* node) {
    if (!node) return 0;
    return 1 + countTreeNodes(node->left) + countTreeNodes(node->right);
}

/* Count owner nodes across chains */
int hashChain_countOwnerNodes(HashMap_Chain* map, int owner_type, int owner_id) {
    if (!map) return 0;
    int total = 0;
    for (int i = 0; i < map->size; i++) {
        AVLChain* cur = map->buckets[i];
        while (cur) {
            if (cur->owner_type == owner_type && cur->owner_id == owner_id) {
                total += countTreeNodes(cur->tree);
            }
            cur = cur->next;
        }
    }
    return total;
}

/* ----------------------------- System ----------------------------- */

typedef struct IT_Club_System {
    AVL_Node* department_tree;  /* dept_id -> Department* */
    AVL_Node* event_tree;       /* event_id -> Event* */
    pthread_mutex_t data_lock;
} IT_Club_System;

IT_Club_System* createSystem() {
    IT_Club_System* sys = (IT_Club_System*)malloc(sizeof(IT_Club_System));
    sys->department_tree = NULL;
    sys->event_tree = NULL;
    pthread_mutex_init(&sys->data_lock, NULL);
    return sys;
}

/* ----------------------------- Helpers & Display ----------------------------- */

int countNodes(AVL_Node* node) {
    if (!node) return 0;
    return 1 + countNodes(node->left) + countNodes(node->right);
}

void printMember(void* data) {
    Member* m = (Member*)data;
    if (!m) return;
    printf("  ID:%d | Name:%s | Role:%s | Contact:%s | Dept:%s\n", m->member_id, m->name, m->role, m->contact, m->department_name);
}

void printFeedback(void* data) {
    Feedback* fb = (Feedback*)data;
    if (!fb) return;
    printf("  Feedback ID:%d | Member ID:%d | Rating:%d/5\n", fb->feedback_id, fb->member_id, fb->rating);
    printf("    Comment: %s\n", fb->comment);
}

void printEventBasic(void* data) {
    Event* e = (Event*)data;
    if (!e) return;
    printf("\n--- Event ID:%d | %s ---\nVenue:%s | Date:%s\nBudget:%.2f | Expenses:%.2f | Profit/Loss:%.2f\nMembers:%d | Feedback:%d\n",
           e->event_id, e->name, e->venue, e->date, e->budget, e->expenses, e->profit_loss, e->member_count, e->feedback_count);
}

void printDepartmentBasic(void* data) {
    Department* d = (Department*)data;
    if (!d) return;
    printf("\n--- Department ID:%d | %s ---\nMembers:%d\n", d->dept_id, d->name, d->member_count);
}

/* ----------------------------- Department functions ----------------------------- */

int departmentExists(IT_Club_System* sys, int dept_id) {
    return searchAVL(sys->department_tree, dept_id) != NULL;
}

Department* createDepartmentStruct(int dept_id, const char* name) {
    Department* d = (Department*)malloc(sizeof(Department));
    d->dept_id = dept_id;
    strncpy(d->name, name, sizeof(d->name)-1);
    d->name[sizeof(d->name)-1] = '\0';
    d->member_map = create_HashMap_Chain(10);
    d->member_count = 0;
    return d;
}

void addDepartmentInteractive(IT_Club_System* sys) {
    int dept_id;
    char name[100];
    while (1) {
        printf("Enter department ID (integer): ");
        if (scanf("%d", &dept_id) != 1) { printf("Invalid. Try again.\n"); while(getchar()!='\n'); continue; }
        while(getchar()!='\n');
        if (departmentExists(sys, dept_id)) { printf("Department ID %d exists. Provide unique ID.\n", dept_id); continue; }
        break;
    }
    printf("Enter department name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';
    Department* d = createDepartmentStruct(dept_id, name);
    sys->department_tree = insertNode(sys->department_tree, dept_id, d);
    printf("Created department '%s' (ID:%d)\n", name, dept_id);
}

Department* findDepartment(IT_Club_System* sys, int dept_id) {
    AVL_Node* node = searchAVL(sys->department_tree, dept_id);
    return node ? (Department*)node->data : NULL;
}

void displayAllDepartments(IT_Club_System* sys) {
    if (!sys->department_tree) { printf("No departments found.\n"); return; }
    inOrder(sys->department_tree, printDepartmentBasic);
}

void addMemberToDepartmentInteractive(IT_Club_System* sys) {
    int dept_id;
    printf("Enter department ID: ");
    if (scanf("%d", &dept_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); return; }
    while(getchar()!='\n');
    Department* d = findDepartment(sys, dept_id);
    if (!d) { printf("Department not found.\n"); return; }

    Member m;
    while (1) {
        printf("Enter member ID (integer): ");
        if (scanf("%d", &m.member_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); continue; }
        while(getchar()!='\n');
        void* exists = hashChain_get(d->member_map, m.member_id, OWNER_DEPARTMENT, dept_id);
        if (exists) { printf("Member ID %d exists in this department. Unique ID required.\n", m.member_id); continue; }
        break;
    }
    printf("Enter member name: "); fgets(m.name, sizeof(m.name), stdin); m.name[strcspn(m.name, "\n")] = '\0';
    printf("Enter role: "); fgets(m.role, sizeof(m.role), stdin); m.role[strcspn(m.role, "\n")] = '\0';
    printf("Enter contact: "); fgets(m.contact, sizeof(m.contact), stdin); m.contact[strcspn(m.contact, "\n")] = '\0';
    strncpy(m.department_name, d->name, sizeof(m.department_name)-1); m.department_name[sizeof(m.department_name)-1] = '\0';

    Member* mp = (Member*)malloc(sizeof(Member)); *mp = m;
    hashChain_insert(d->member_map, mp->member_id, mp, OWNER_DEPARTMENT, dept_id);
    d->member_count++;
    printf("Added Member '%s' (ID:%d) to department '%s'.\n", mp->name, mp->member_id, d->name);
}

void displayMembersInDepartmentInteractive(IT_Club_System* sys) {
    int dept_id;
    printf("Enter department ID: ");
    if (scanf("%d", &dept_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); return; }
    while(getchar()!='\n');
    Department* d = findDepartment(sys, dept_id);
    if (!d) { printf("Department not found.\n"); return; }
    if (d->member_count == 0) { printf("No members in department '%s'.\n", d->name); return; }
    printf("\nMembers of department '%s' (ID:%d):\n", d->name, d->dept_id);
    hashChain_traverseOwner(d->member_map, OWNER_DEPARTMENT, dept_id, printMember);
}

/* ----------------------------- Events & feedback ----------------------------- */

int eventExists(IT_Club_System* sys, int event_id) {
    return searchAVL(sys->event_tree, event_id) != NULL;
}

Event* createEventStruct(int event_id, const char* name, const char* venue, const char* date, double budget, const char* desc) {
    Event* e = (Event*)malloc(sizeof(Event));
    e->event_id = event_id;
    strncpy(e->name, name, sizeof(e->name)-1); e->name[sizeof(e->name)-1] = '\0';
    strncpy(e->venue, venue, sizeof(e->venue)-1); e->venue[sizeof(e->venue)-1] = '\0';
    strncpy(e->date, date, sizeof(e->date)-1); e->date[sizeof(e->date)-1] = '\0';
    e->budget = budget;
    e->expenses = 0.0;
    e->profit_loss = budget;
    strncpy(e->description, desc, sizeof(e->description)-1); e->description[sizeof(e->description)-1] = '\0';
    e->member_map = create_HashMap_Chain(10);
    e->feedback_map = create_HashMap_Chain(10);
    e->member_count = 0;
    e->feedback_count = 0;
    return e;
}

void createEventInteractive(IT_Club_System* sys) {
    int event_id;
    char name[100], venue[100], date[20], desc[256];
    double budget;
    while (1) {
        printf("Enter event ID (integer): ");
        if (scanf("%d", &event_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); continue; }
        while(getchar()!='\n');
        if (eventExists(sys, event_id)) { printf("Event ID exists. Provide unique ID.\n"); continue; }
        break;
    }
    printf("Enter event name: "); fgets(name, sizeof(name), stdin); name[strcspn(name, "\n")] = '\0';
    printf("Enter venue: "); fgets(venue, sizeof(venue), stdin); venue[strcspn(venue, "\n")] = '\0';
    printf("Enter date (DD/MM/YYYY): "); fgets(date, sizeof(date), stdin); date[strcspn(date, "\n")] = '\0';
    printf("Enter budget: "); if (scanf("%lf", &budget) != 1) { budget = 0.0; while(getchar()!='\n'); } else while(getchar()!='\n');
    printf("Enter description: "); fgets(desc, sizeof(desc), stdin); desc[strcspn(desc, "\n")] = '\0';

    Event* e = createEventStruct(event_id, name, venue, date, budget, desc);
    sys->event_tree = insertNode(sys->event_tree, event_id, e);
    printf("Created Event '%s' (ID:%d)\n", name, event_id);
}

Event* findEvent(IT_Club_System* sys, int event_id) {
    AVL_Node* node = searchAVL(sys->event_tree, event_id);
    return node ? (Event*)node->data : NULL;
}

void addMemberToEventInteractive(IT_Club_System* sys) {
    int event_id;
    printf("Enter event ID: "); if (scanf("%d", &event_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); return; }
    while(getchar()!='\n');
    Event* e = findEvent(sys, event_id);
    if (!e) { printf("Event not found.\n"); return; }

    Member m;
    while (1) {
        printf("Enter member ID (integer): ");
        if (scanf("%d", &m.member_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); continue; }
        while(getchar()!='\n');
        void* exists = hashChain_get(e->member_map, m.member_id, OWNER_EVENT_MEMBERS, event_id);
        if (exists) { printf("Member ID %d exists for this event. Unique ID required.\n", m.member_id); continue; }
        break;
    }
    printf("Enter member name: "); fgets(m.name, sizeof(m.name), stdin); m.name[strcspn(m.name, "\n")] = '\0';
    printf("Enter role: "); fgets(m.role, sizeof(m.role), stdin); m.role[strcspn(m.role, "\n")] = '\0';
    printf("Enter contact: "); fgets(m.contact, sizeof(m.contact), stdin); m.contact[strcspn(m.contact, "\n")] = '\0';
    printf("Enter department name (for record): "); fgets(m.department_name, sizeof(m.department_name), stdin); m.department_name[strcspn(m.department_name, "\n")] = '\0';

    Member* mp = (Member*)malloc(sizeof(Member)); *mp = m;
    hashChain_insert(e->member_map, mp->member_id, mp, OWNER_EVENT_MEMBERS, event_id);
    e->member_count++;
    printf("Added member '%s' (ID:%d) to event '%s'.\n", mp->name, mp->member_id, e->name);
}

void displayEventMembersInteractive(IT_Club_System* sys) {
    int event_id;
    printf("Enter event ID: "); if (scanf("%d", &event_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); return; }
    while(getchar()!='\n');
    Event* e = findEvent(sys, event_id);
    if (!e) { printf("Event not found.\n"); return; }
    if (e->member_count == 0) { printf("No members for this event.\n"); return; }
    printf("\nMembers for event '%s' (ID:%d):\n", e->name, e->event_id);
    hashChain_traverseOwner(e->member_map, OWNER_EVENT_MEMBERS, event_id, printMember);
}

void deleteMemberFromEventInteractive(IT_Club_System* sys) {
    int event_id, member_id;
    printf("Enter event ID: "); if (scanf("%d", &event_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); return; }
    printf("Enter member ID to remove: "); if (scanf("%d", &member_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); return; }
    while(getchar()!='\n');
    Event* e = findEvent(sys, event_id);
    if (!e) { printf("Event not found.\n"); return; }
    hashChain_delete(e->member_map, member_id, OWNER_EVENT_MEMBERS, event_id);
    if (e->member_count > 0) e->member_count--;
    printf("Attempted to remove member ID %d from event ID %d.\n", member_id, event_id);
}

/* Feedback insert/display */

void addFeedbackToEventInteractive(IT_Club_System* sys) {
    int event_id;
    printf("Enter event ID: "); if (scanf("%d", &event_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); return; }
    while(getchar()!='\n');
    Event* e = findEvent(sys, event_id);
    if (!e) { printf("Event not found.\n"); return; }

    Feedback fb;
    while (1) {
        printf("Enter feedback ID (integer): ");
        if (scanf("%d", &fb.feedback_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); continue; }
        while(getchar()!='\n');
        void* exists = hashChain_get(e->feedback_map, fb.feedback_id, OWNER_EVENT_FEEDBACK, event_id);
        if (exists) { printf("Feedback ID %d exists for this event. Unique ID required.\n", fb.feedback_id); continue; }
        break;
    }
    printf("Enter member ID (author): "); if (scanf("%d", &fb.member_id) != 1) { fb.member_id = 0; while(getchar()!='\n'); } else while(getchar()!='\n');
    printf("Enter rating (1-5): "); if (scanf("%d", &fb.rating) != 1) fb.rating = 0; while(getchar()!='\n');
    printf("Enter comment: "); fgets(fb.comment, sizeof(fb.comment), stdin); fb.comment[strcspn(fb.comment, "\n")] = '\0';

    Feedback* fbp = (Feedback*)malloc(sizeof(Feedback)); *fbp = fb;
    hashChain_insert(e->feedback_map, fbp->feedback_id, fbp, OWNER_EVENT_FEEDBACK, event_id);
    e->feedback_count++;
    printf("Added feedback ID %d to event '%s'.\n", fbp->feedback_id, e->name);
}

void displayEventFeedbackInteractive(IT_Club_System* sys) {
    int event_id;
    printf("Enter event ID: "); if (scanf("%d", &event_id) != 1) { printf("Invalid.\n"); while(getchar()!='\n'); return; }
    while(getchar()!='\n');
    Event* e = findEvent(sys, event_id);
    if (!e) { printf("Event not found.\n"); return; }
    if (e->feedback_count == 0) { printf("No feedback for this event.\n"); return; }
    printf("\nFeedback for event '%s' (ID:%d):\n", e->name, e->event_id);
    hashChain_traverseOwner(e->feedback_map, OWNER_EVENT_FEEDBACK, event_id, printFeedback);
}

/* ----------------------------- Persistence: Save / Load ----------------------------- */

/* Save members from an AVL tree to file in-order */
void saveMembersFromAVL(FILE* fp, AVL_Node* node) {
    if (!node) return;
    saveMembersFromAVL(fp, node->left);
    Member* m = (Member*)node->data;
    fwrite(m, sizeof(Member), 1, fp);
    saveMembersFromAVL(fp, node->right);
}

/* Save feedback from AVL tree to file in-order */
void saveFeedbackFromAVL(FILE* fp, AVL_Node* node) {
    if (!node) return;
    saveFeedbackFromAVL(fp, node->left);
    Feedback* fb = (Feedback*)node->data;
    fwrite(fb, sizeof(Feedback), 1, fp);
    saveFeedbackFromAVL(fp, node->right);
}

/* Write departments.dat: count, then PersistDepartment + member blocks per department */
void writeDepartmentsToFile(AVL_Node* node, FILE* fp) {
    if (!node) return;
    writeDepartmentsToFile(node->left, fp);
    Department* d = (Department*)node->data;
    PersistDepartment pd;
    pd.dept_id = d->dept_id;
    strncpy(pd.name, d->name, sizeof(pd.name)-1); pd.name[sizeof(pd.name)-1] = '\0';
    pd.member_count = d->member_count;
    fwrite(&pd, sizeof(PersistDepartment), 1, fp);
    /* write members belonging to this department via chains */
    for (int i = 0; i < d->member_map->size; i++) {
        AVLChain* cur = d->member_map->buckets[i];
        while (cur) {
            if (cur->owner_type == OWNER_DEPARTMENT && cur->owner_id == d->dept_id) {
                saveMembersFromAVL(fp, cur->tree);
            }
            cur = cur->next;
        }
    }
    writeDepartmentsToFile(node->right, fp);
}

void saveDepartments(IT_Club_System* sys) {
    FILE* fp = fopen("departments.dat", "wb");
    if (!fp) { printf("Error opening departments.dat for writing.\n"); return; }
    int dept_count = countNodes(sys->department_tree);
    fwrite(&dept_count, sizeof(int), 1, fp);
    writeDepartmentsToFile(sys->department_tree, fp);
    fclose(fp);
}

/* Write events.dat: count, then PersistEvent + members + feedback blocks per event */
void writeEventsToFile(AVL_Node* node, FILE* fp) {
    if (!node) return;
    writeEventsToFile(node->left, fp);
    Event* e = (Event*)node->data;
    PersistEvent pe;
    pe.event_id = e->event_id;
    strncpy(pe.name, e->name, sizeof(pe.name)-1); pe.name[sizeof(pe.name)-1] = '\0';
    strncpy(pe.venue, e->venue, sizeof(pe.venue)-1); pe.venue[sizeof(pe.venue)-1] = '\0';
    strncpy(pe.date, e->date, sizeof(pe.date)-1); pe.date[sizeof(pe.date)-1] = '\0';
    pe.budget = e->budget; pe.expenses = e->expenses; pe.profit_loss = e->profit_loss;
    strncpy(pe.description, e->description, sizeof(pe.description)-1); pe.description[sizeof(pe.description)-1] = '\0';
    pe.member_count = e->member_count;
    pe.feedback_count = e->feedback_count;
    fwrite(&pe, sizeof(PersistEvent), 1, fp);
    /* write members for this event */
    for (int i = 0; i < e->member_map->size; i++) {
        AVLChain* cur = e->member_map->buckets[i];
        while (cur) {
            if (cur->owner_type == OWNER_EVENT_MEMBERS && cur->owner_id == e->event_id) {
                saveMembersFromAVL(fp, cur->tree);
            }
            cur = cur->next;
        }
    }
    /* write feedback for this event */
    for (int i = 0; i < e->feedback_map->size; i++) {
        AVLChain* cur = e->feedback_map->buckets[i];
        while (cur) {
            if (cur->owner_type == OWNER_EVENT_FEEDBACK && cur->owner_id == e->event_id) {
                saveFeedbackFromAVL(fp, cur->tree);
            }
            cur = cur->next;
        }
    }
    writeEventsToFile(node->right, fp);
}

void saveEvents(IT_Club_System* sys) {
    FILE* fp = fopen("events.dat", "wb");
    if (!fp) { printf("Error opening events.dat for writing.\n"); return; }
    int event_count = countNodes(sys->event_tree);
    fwrite(&event_count, sizeof(int), 1, fp);
    writeEventsToFile(sys->event_tree, fp);
    fclose(fp);
}

/* Write feedback.dat as global archive: count then PersistFeedback entries for all events */
void writePersistFeedbackFromAVL(FILE* fp, AVL_Node* node, int event_id) {
    if (!node) return;
    writePersistFeedbackFromAVL(fp, node->left, event_id);
    Feedback* fb = (Feedback*)node->data;
    PersistFeedback pf;
    pf.event_id = event_id;
    pf.fb = *fb;
    fwrite(&pf, sizeof(PersistFeedback), 1, fp);
    writePersistFeedbackFromAVL(fp, node->right, event_id);
}

void writeFeedbackArchiveForEvent(AVL_Node* node, FILE* fp) {
    if (!node) return;
    writeFeedbackArchiveForEvent(node->left, fp);
    Event* e = (Event*)node->data;
    for (int i = 0; i < e->feedback_map->size; i++) {
        AVLChain* cur = e->feedback_map->buckets[i];
        while (cur) {
            if (cur->owner_type == OWNER_EVENT_FEEDBACK && cur->owner_id == e->event_id) {
                writePersistFeedbackFromAVL(fp, cur->tree, e->event_id);
            }
            cur = cur->next;
        }
    }
    writeFeedbackArchiveForEvent(node->right, fp);
}

void saveGlobalFeedback(IT_Club_System* sys) {
    FILE* fp = fopen("feedback.dat", "wb");
    if (!fp) { printf("Error opening feedback.dat for writing.\n"); return; }
    int total_fb = 0;
    /* count */
    void count_fb(void* data) { Event* e = (Event*)data; total_fb += e->feedback_count; }
    inOrder(sys->event_tree, count_fb);
    fwrite(&total_fb, sizeof(int), 1, fp);
    writeFeedbackArchiveForEvent(sys->event_tree, fp);
    fclose(fp);
}

void saveAllData(IT_Club_System* sys) {
    saveDepartments(sys);
    saveEvents(sys);
    saveGlobalFeedback(sys);
    printf("Saved departments.dat, events.dat, feedback.dat\n");
}

/* ----------------------------- Load routines ----------------------------- */

void loadDepartments(IT_Club_System* sys) {
    FILE* fp = fopen("departments.dat", "rb");
    if (!fp) return;
    int dept_count = 0;
    if (fread(&dept_count, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    for (int i = 0; i < dept_count; i++) {
        PersistDepartment pd;
        if (fread(&pd, sizeof(PersistDepartment), 1, fp) != 1) break;
        Department* d = createDepartmentStruct(pd.dept_id, pd.name);
        for (int j = 0; j < pd.member_count; j++) {
            Member* m = (Member*)malloc(sizeof(Member));
            if (fread(m, sizeof(Member), 1, fp) != 1) { free(m); break; }
            hashChain_insert(d->member_map, m->member_id, m, OWNER_DEPARTMENT, d->dept_id);
            d->member_count++;
        }
        sys->department_tree = insertNode(sys->department_tree, d->dept_id, d);
    }
    fclose(fp);
}

void loadEvents(IT_Club_System* sys) {
    FILE* fp = fopen("events.dat", "rb");
    if (!fp) return;
    int event_count = 0;
    if (fread(&event_count, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    for (int i = 0; i < event_count; i++) {
        PersistEvent pe;
        if (fread(&pe, sizeof(PersistEvent), 1, fp) != 1) break;
        Event* e = createEventStruct(pe.event_id, pe.name, pe.venue, pe.date, pe.budget, pe.description);
        e->expenses = pe.expenses; e->profit_loss = pe.profit_loss;
        for (int j = 0; j < pe.member_count; j++) {
            Member* m = (Member*)malloc(sizeof(Member));
            if (fread(m, sizeof(Member), 1, fp) != 1) { free(m); break; }
            hashChain_insert(e->member_map, m->member_id, m, OWNER_EVENT_MEMBERS, e->event_id);
            e->member_count++;
        }
        for (int j = 0; j < pe.feedback_count; j++) {
            Feedback* fb = (Feedback*)malloc(sizeof(Feedback));
            if (fread(fb, sizeof(Feedback), 1, fp) != 1) { free(fb); break; }
            hashChain_insert(e->feedback_map, fb->feedback_id, fb, OWNER_EVENT_FEEDBACK, e->event_id);
            e->feedback_count++;
        }
        sys->event_tree = insertNode(sys->event_tree, e->event_id, e);
    }
    fclose(fp);
}

void loadGlobalFeedback(IT_Club_System* sys) {
    FILE* fp = fopen("feedback.dat", "rb");
    if (!fp) return;
    int total_fb = 0;
    if (fread(&total_fb, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    for (int i = 0; i < total_fb; i++) {
        PersistFeedback pf;
        if (fread(&pf, sizeof(PersistFeedback), 1, fp) != 1) break;
        Event* e = findEvent(sys, pf.event_id);
        if (!e) continue;
        Feedback* exists = (Feedback*)hashChain_get(e->feedback_map, pf.fb.feedback_id, OWNER_EVENT_FEEDBACK, e->event_id);
        if (exists) continue;
        Feedback* fb = (Feedback*)malloc(sizeof(Feedback)); *fb = pf.fb;
        hashChain_insert(e->feedback_map, fb->feedback_id, fb, OWNER_EVENT_FEEDBACK, e->event_id);
        e->feedback_count++;
    }
    fclose(fp);
}

void loadAllData(IT_Club_System* sys) {
    /* NOTE: this simple loader appends to current structures.
       In production you'd free old memory before loading to avoid leaks.
    */
    loadDepartments(sys);
    loadEvents(sys);
    loadGlobalFeedback(sys);
    printf("Loaded. Departments: %d  Events: %d\n", countNodes(sys->department_tree), countNodes(sys->event_tree));
}

/* ----------------------------- Encryption / Decryption ----------------------------- */

int generateKey() { srand((unsigned)time(NULL) ^ (unsigned)getpid()); return rand() % 255 + 1; }

void encryptFile(const char* inputFile, const char* outputFile, int key) {
    FILE *in = fopen(inputFile, "rb");
    if (!in) { printf("[WARN] '%s' not found, skipping encryption.\n", inputFile); return; }
    FILE *out = fopen(outputFile, "wb");
    if (!out) { printf("[ERROR] Could not open '%s' for writing.\n", outputFile); fclose(in); return; }
    int c;
    while ((c = fgetc(in)) != EOF) fputc(c ^ key, out);
    fclose(in); fclose(out);
    printf("[OK] Encrypted: %s -> %s\n", inputFile, outputFile);
}

void decryptFile(const char* inputFile, const char* outputFile, int key) {
    /* symmetric XOR */
    encryptFile(inputFile, outputFile, key);
    printf("[OK] Decrypted: %s -> %s\n", inputFile, outputFile);
}

void encryptAll(int key) {
    printf("\n--- Encrypting all .dat files ---\n");
    encryptFile("departments.dat", "departments.enc", key);
    encryptFile("events.dat", "events.enc", key);
    encryptFile("feedback.dat", "feedback.enc", key);
    printf("--- Encryption complete ---\n");
}

void decryptAll(int key) {
    printf("\n--- Decrypting all .enc files ---\n");
    decryptFile("departments.enc", "departments.dat", key);
    decryptFile("events.enc", "events.dat", key);
    decryptFile("feedback.enc", "feedback.dat", key);
    printf("--- Decryption complete ---\n");
}

/* ----------------------------- Auto-save thread ----------------------------- */

void* autoSaveThread(void* arg) {
    IT_Club_System* sys = (IT_Club_System*)arg;
    while (1) {
        sleep(300); /* 5 minutes */
        pthread_mutex_lock(&sys->data_lock);
        printf("\n[Auto-save] Saving data...\n");
        saveAllData(sys);
        pthread_mutex_unlock(&sys->data_lock);
    }
    return NULL;
}

/* ----------------------------- Reporting ----------------------------- */

typedef struct Heap {
    void** data;
    double* scores;
    int size;
    int capacity;
} Heap;

Heap* createHeap(int capacity) {
    Heap* h = (Heap*)malloc(sizeof(Heap));
    h->data = (void**)malloc(capacity * sizeof(void*));
    h->scores = (double*)malloc(capacity * sizeof(double));
    h->size = 0; h->capacity = capacity;
    return h;
}
void heapifyUp(Heap* heap, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (heap->scores[idx] > heap->scores[parent]) {
            double ts = heap->scores[idx]; heap->scores[idx] = heap->scores[parent]; heap->scores[parent] = ts;
            void* td = heap->data[idx]; heap->data[idx] = heap->data[parent]; heap->data[parent] = td;
            idx = parent;
        } else break;
    }
}
void heapifyDown(Heap* heap, int idx) {
    while (1) {
        int largest = idx;
        int l = 2*idx + 1, r = 2*idx + 2;
        if (l < heap->size && heap->scores[l] > heap->scores[largest]) largest = l;
        if (r < heap->size && heap->scores[r] > heap->scores[largest]) largest = r;
        if (largest != idx) {
            double ts = heap->scores[idx]; heap->scores[idx] = heap->scores[largest]; heap->scores[largest] = ts;
            void* td = heap->data[idx]; heap->data[idx] = heap->data[largest]; heap->data[largest] = td;
            idx = largest;
        } else break;
    }
}
void heapInsert(Heap* heap, double score, void* data) {
    if (heap->size >= heap->capacity) return;
    heap->scores[heap->size] = score;
    heap->data[heap->size] = data;
    heapifyUp(heap, heap->size);
    heap->size++;
}
void* extractMax(Heap* heap, double* score) {
    if (heap->size == 0) return NULL;
    *score = heap->scores[0];
    void* d = heap->data[0];
    heap->scores[0] = heap->scores[heap->size-1];
    heap->data[0] = heap->data[heap->size-1];
    heap->size--;
    heapifyDown(heap, 0);
    return d;
}

void collectEvents(AVL_Node* root, Heap* heap) {
    if (!root) return;
    collectEvents(root->left, heap);
    Event* e = (Event*)root->data;
    heapInsert(heap, e->profit_loss, e);
    collectEvents(root->right, heap);
}

void getTopKEvents(IT_Club_System* sys, int k) {
    Heap* heap = createHeap(1000);
    collectEvents(sys->event_tree, heap);
    printf("\n=== Top %d Events by Profit/Loss ===\n", k);
    for (int i = 0; i < k && heap->size > 0; i++) {
        double sc; Event* e = (Event*)extractMax(heap, &sc);
        printf("%d. %s (ID:%d) — Profit: $%.2f\n", i+1, e->name, e->event_id, sc);
    }
    free(heap->data); free(heap->scores); free(heap);
}

/* ----------------------------- Menu & Main ----------------------------- */

void displayMenu() {
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

int main() {
    IT_Club_System* sys = createSystem();
    printf("Loading existing data (if present)...\n");
    loadAllData(sys);

    pthread_t autosave_thread;
    pthread_create(&autosave_thread, NULL, autoSaveThread, sys);
    pthread_detach(autosave_thread);

    int choice;
    while (1) {
        displayMenu();
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Enter a number.\n");
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n'); /* consume newline */
        pthread_mutex_lock(&sys->data_lock);
        switch (choice) {
            case 1: createEventInteractive(sys); break;
            case 2: addDepartmentInteractive(sys); break;
            case 3: addMemberToDepartmentInteractive(sys); break;
            case 4: addMemberToEventInteractive(sys); break;
            case 5: deleteMemberFromEventInteractive(sys); break;
            case 6: addFeedbackToEventInteractive(sys); break;
            case 7: {
                int id; printf("Enter event ID: "); if (scanf("%d", &id) == 1) { while(getchar()!='\n'); Event* e = findEvent(sys, id); if (e) printEventBasic(e); else printf("Event not found.\n"); } else { printf("Invalid.\n"); while(getchar()!='\n'); }
                break;
            }
            case 8: if (!sys->event_tree) printf("No events.\n"); else inOrder(sys->event_tree, printEventBasic); break;
            case 9: displayAllDepartments(sys); break;
            case 10: displayMembersInDepartmentInteractive(sys); break;
            case 11: displayEventMembersInteractive(sys); break;
            case 12: displayEventFeedbackInteractive(sys); break;
            case 13: getTopKEvents(sys, 3); break;
            case 14: {
                int key = generateKey();
                printf("Encryption key (store safely): %d\n", key);
                saveAllData(sys); /* ensure .dat files are current */
                encryptAll(key);
                break;
            }
            case 15: {
                int key;
                printf("Enter decryption key: "); if (scanf("%d", &key) != 1) { printf("Invalid key.\n"); while(getchar()!='\n'); break; }
                while(getchar()!='\n');
                decryptAll(key);
                /* reload after decryption */
                loadAllData(sys);
                break;
            }
            case 16: saveAllData(sys); pthread_mutex_unlock(&sys->data_lock); printf("Saved. Exiting.\n"); exit(0);
            default: printf("Invalid choice.\n"); break;
        }
        pthread_mutex_unlock(&sys->data_lock);
    }
    return 0;
}
