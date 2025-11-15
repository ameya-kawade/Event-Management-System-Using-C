#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // For strcasecmp

// === Configuration ===
#define MAX_DEPTS 10
#define MAX_DEPT_NAME_LEN 20
#define MAX_EVENTS 500

// Departments mapping (adjust as needed)
const char *DEPT_NAMES[MAX_DEPTS] = {
    "IT", "COMP", "ENTC", "MECH", "CIVIL",
    "ECE", "CHEM", "BIO", "EE", "OTHER"
};

// ====== Data Structures ======
typedef struct MemberNode {
    int id;
    char name[30];
    char department[MAX_DEPT_NAME_LEN];
    char contact[15];
    struct MemberNode *next;
} MemberNode;

typedef struct EventNode {
    int eid;
    char event_name[40];
    char date[15];
    char description[200];
    float cost;
    float revenue;
    struct EventNode *next;
} EventNode;

typedef struct FeedbackNode {
    int eventid;
    char feedback[256];
    struct FeedbackNode *next;
} FeedbackNode;

// ====== Generalized Linked List Heads (arrays) ======
MemberNode *memberLists[MAX_DEPTS];      // department-wise member lists
EventNode *eventLists[MAX_EVENTS];       // eventid-indexed lists
FeedbackNode *feedbackLists[MAX_EVENTS]; // feedback lists per eventid

// ====== Function Declarations ======
int login();
void memberMenu();
void eventMenu();
void feedbackMenu();
void reportMenu();
void addMember();
void viewMembers();
void editMember();
void searchMember();
void deleteMember();
void addEvent();
void viewEvents();
void deleteEvent();
void collectFeedback();
void viewFeedbacks();
void generateReport();

// Persistence
void saveData();
void loadData();
static void csv_escape(const char *src, char *dst, size_t dst_size);
static char *read_csv_field(const char *line, int field_index, char *out, size_t out_size);

// Utilities
int getDeptIndex(const char *dept);

// --- NEW HELPER FUNCTIONS ---
void getString(const char *prompt, char *buffer, int size);
int getInt(const char *prompt);
float getFloat(const char *prompt);
void appendMember(int idx, MemberNode *newNode);
void appendEvent(int eid, EventNode *newNode);
void appendFeedback(int eid, FeedbackNode *newNode);
// --- (clearInputBuffer is no longer needed) ---


// ====== NEW: Robust Input Helper Functions ======

/**
 * @brief Safely reads a line of text from stdin into a buffer.
 * Replaces the old scanf + clearInputBuffer pattern.
 */
void getString(const char *prompt, char *buffer, int size) {
    printf("%s", prompt);
    if (fgets(buffer, size, stdin) != NULL) {
        // Remove trailing newline, if present
        buffer[strcspn(buffer, "\n\r")] = '\0';
    } else {
        buffer[0] = '\0'; // Ensure null terminator on read error
    }
}

/**
 * @brief Safely reads an integer from stdin.
 * Handles invalid input and re-prompts.
 */
int getInt(const char *prompt) {
    char buffer[32];
    int value;
    while (1) {
        getString(prompt, buffer, sizeof(buffer));
        if (sscanf(buffer, "%d", &value) == 1) {
            return value;
        } else {
            printf("Invalid input. Please enter a whole number.\n");
        }
    }
}

/**
 * @brief Safely reads a float from stdin.
 * Handles invalid input and re-prompts.
 */
float getFloat(const char *prompt) {
    char buffer[32];
    float value;
    while (1) {
        getString(prompt, buffer, sizeof(buffer));
        if (sscanf(buffer, "%f", &value) == 1) {
            return value;
        } else {
            printf("Invalid input. Please enter a number (e.g., 123.45).\n");
        }
    }
}

// ====== NEW: Linked List Append Helpers ======

/**
 * @brief Appends a new member node to the end of the list for a given index.
 */
void appendMember(int idx, MemberNode *newNode) {
    newNode->next = NULL;
    if (idx < 0 || idx >= MAX_DEPTS) {
        printf("Error: Invalid department index %d.\n", idx);
        idx = MAX_DEPTS - 1; // Default to OTHER
    }
    
    MemberNode **head = &memberLists[idx];
    if (!*head) {
        *head = newNode; // List is empty
    } else {
        MemberNode *t = *head;
        while (t->next) { // Traverse to the last node
            t = t->next;
        }
        t->next = newNode; // Append
    }
}

/**
 * @brief Appends a new event node to the end of the list for a given event ID.
 */
void appendEvent(int eid, EventNode *newNode) {
    newNode->next = NULL;
    if (eid < 0 || eid >= MAX_EVENTS) return; // Invalid ID

    EventNode **head = &eventLists[eid];
    if (!*head) {
        *head = newNode;
    } else {
        EventNode *t = *head;
        while (t->next) {
            t = t->next;
        }
        t->next = newNode;
    }
}

/**
 * @brief Appends a new feedback node to the end of the list for a given event ID.
 */
void appendFeedback(int eid, FeedbackNode *newNode) {
    newNode->next = NULL;
    if (eid < 0 || eid >= MAX_EVENTS) return; // Invalid ID
    
    FeedbackNode **head = &feedbackLists[eid];
    if (!*head) {
        *head = newNode;
    } else {
        FeedbackNode *t = *head;
        while (t->next) {
            t = t->next;
        }
        t->next = newNode;
    }
}

// ====== Utility Implementations ======

/**
 * @brief (Original function, no longer needed with fgets)
 */
// void clearInputBuffer() {
//     int c;
//     while ((c = getchar()) != '\n' && c != EOF);
// }

/**
 * @brief (SIMPLIFIED) Gets the index for a department name, case-insensitive.
 */
int getDeptIndex(const char *dept) {
    if (!dept) return -1;
    for (int i = 0; i < MAX_DEPTS; ++i) {
        // Use case-insensitive comparison
        if (strcasecmp(DEPT_NAMES[i], dept) == 0) {
            return i;
        }
    }
    return -1; // Not found
}

// ====== Main Application ======
int main() {
    // initialize lists
    for (int i = 0; i < MAX_DEPTS; ++i) memberLists[i] = NULL;
    for (int i = 0; i < MAX_EVENTS; ++i) {
        eventLists[i] = NULL;
        feedbackLists[i] = NULL;
    }

    loadData();

    if (!login()) {
        printf("Access Denied.\n");
        return 0;
    }

    int choice;
    do {
        printf("\n===== IT Club Management System (Generalized Linked Lists) =====\n");
        printf("1. Member Management\n2. Event Management\n3. Feedback\n4. Reports\n5. Save & Exit\n");
        
        choice = getInt("Select: "); // SIMPLIFIED

        switch (choice) {
            case 1: memberMenu(); break;
            case 2: eventMenu(); break;
            case 3: feedbackMenu(); break;
            case 4: reportMenu(); break;
            case 5:
                printf("Saving data...\n");
                saveData();
                printf("Exiting...\n");
                break;
            default:
                printf("Invalid choice!\n");
        }
    } while (choice != 5);

    return 0;
}

// ====== Login ======
int login() {
    char user[64], pass[64];
    // SIMPLIFIED input
    printf("Login:\n");
    getString("Username: ", user, sizeof(user));
    getString("Password: ", pass, sizeof(pass));
    return (strcmp(user, "admin") == 0 && strcmp(pass, "1234") == 0);
}

// ====== Member Management ======
void memberMenu() {
    int ch;
    do {
        printf("\n--- Member Management ---\n");
        printf("1. Add Member\n2. View Members\n3. Edit Member\n4. Search Member\n5. Delete Member\n6. Back\n");
        ch = getInt("Select: "); // SIMPLIFIED

        switch (ch) {
            case 1: addMember(); break;
            case 2: viewMembers(); break;
            case 3: editMember(); break;
            case 4: searchMember(); break;
            case 5: deleteMember(); break;
            case 6: break;
            default: printf("Invalid choice!\n");
        }
    } while (ch != 6);
}

void addMember() {
    MemberNode *newM = (MemberNode *)malloc(sizeof(MemberNode));
    if (!newM) { printf("Memory error!\n"); return; }

    // SIMPLIFIED input
    newM->id = getInt("Enter ID: ");
    getString("Name: ", newM->name, sizeof(newM->name));
    getString("Department: ", newM->department, sizeof(newM->department));
    getString("Contact: ", newM->contact, sizeof(newM->contact));
    newM->next = NULL;

    int idx = getDeptIndex(newM->department);
    if (idx == -1) {
        printf("Unknown department '%s'. Assigning to OTHER.\n", newM->department);
        idx = MAX_DEPTS - 1; // OTHER
        // Update the node's data to match the official "OTHER" name
        strncpy(newM->department, DEPT_NAMES[idx], sizeof(newM->department) - 1);
        newM->department[sizeof(newM->department) - 1] = '\0';
    }

    // SIMPLIFIED append logic
    appendMember(idx, newM);

    printf("Member Added to %s department.\n", DEPT_NAMES[idx]);
}

void viewMembers() {
    int any = 0;
    printf("\n--- Members by Department ---\n");
    for (int i = 0; i < MAX_DEPTS; ++i) {
        MemberNode *m = memberLists[i];
        if (!m) continue;
        any = 1;
        printf("\n[%s]\nID\tName\tContact\n--------------------------------\n", DEPT_NAMES[i]);
        while (m) {
            printf("%d\t%s\t%s\n", m->id, m->name, m->contact);
            m = m->next;
        }
    }
    if (!any) printf("No members to show.\n");
}

void editMember() {
    int id = getInt("Enter ID to edit: "); // SIMPLIFIED
    char newDept[MAX_DEPT_NAME_LEN];
    int newIdx;

    for (int i = 0; i < MAX_DEPTS; ++i) { // Find in old department list 'i'
        MemberNode *prev = NULL;
        MemberNode *cur = memberLists[i];

        while (cur) {
            if (cur->id == id) {
                printf("Editing member %d (%s)\n", cur->id, cur->name);
                
                // Get new data (SIMPLIFIED)
                getString("New Name: ", cur->name, sizeof(cur->name));
                getString("New Department: ", newDept, sizeof(newDept));
                getString("New Contact: ", cur->contact, sizeof(cur->contact));
                
                newIdx = getDeptIndex(newDept);
                if (newIdx == -1) {
                    newIdx = MAX_DEPTS - 1; // OTHER
                    strncpy(cur->department, DEPT_NAMES[newIdx], sizeof(cur->department)-1);
                } else {
                    strncpy(cur->department, DEPT_NAMES[newIdx], sizeof(cur->department)-1);
                }
                cur->department[sizeof(cur->department)-1] = '\0';


                // If department changed, move node
                if (newIdx != i) {
                    // 1. Unlink from old list (i)
                    if (prev) {
                        prev->next = cur->next; // Unlink from middle/end
                    } else {
                        memberLists[i] = cur->next; // Unlink from head
                    }
                    
                    // 2. Append to new list (newIdx) (SIMPLIFIED)
                    appendMember(newIdx, cur); 
                    printf("Member moved to %s department.\n", DEPT_NAMES[newIdx]);
                } else {
                     printf("Member updated.\n");
                }
                return;
            }
            prev = cur;
            cur = cur->next;
        }
    }
    printf("No such member.\n");
}

void searchMember() {
    int id = getInt("Enter ID to search: "); // SIMPLIFIED

    for (int i = 0; i < MAX_DEPTS; ++i) {
        MemberNode *m = memberLists[i];
        while (m) {
            if (m->id == id) {
                printf("Found: ID=%d, Name=%s, Dept=%s, Contact=%s\n", m->id, m->name, m->department, m->contact);
                return;
            }
            m = m->next;
        }
    }
    printf("Not found.\n");
}

void deleteMember() {
    int id = getInt("Enter ID to delete: "); // SIMPLIFIED

    for (int i = 0; i < MAX_DEPTS; ++i) {
        MemberNode *prev = NULL, *cur = memberLists[i];
        while (cur) {
            if (cur->id == id) {
                if (prev) prev->next = cur->next; else memberLists[i] = cur->next;
                free(cur);
                printf("Member deleted.\n");
                return;
            }
            prev = cur; cur = cur->next;
        }
    }
    printf("No such member.\n");
}

// ====== Event Management ======
void eventMenu() {
    int ch;
    do {
        printf("\n--- Event Management ---\n");
        printf("1. Add Event\n2. View Events\n3. Delete Event\n4. Back\n");
        ch = getInt("Select: "); // SIMPLIFIED

        switch (ch) {
            case 1: addEvent(); break;
            case 2: viewEvents(); break;
            case 3: deleteEvent(); break;
            case 4: break;
            default: printf("Invalid choice!\n");
        }
    } while (ch != 4);
}

void addEvent() {
    EventNode *newE = (EventNode *)malloc(sizeof(EventNode));
    if (!newE) { printf("Memory error!\n"); return; }
    
    // SIMPLIFIED input
    newE->eid = getInt("Event ID (0-499): ");
    if (newE->eid < 0 || newE->eid >= MAX_EVENTS) { 
        printf("Event ID out of range.\n"); free(newE); return; 
    }

    getString("Event Name: ", newE->event_name, sizeof(newE->event_name));
    getString("Date: ", newE->date, sizeof(newE->date));
    getString("Description: ", newE->description, sizeof(newE->description));
    newE->cost = getFloat("Cost Incurred: ");
    newE->revenue = getFloat("Revenue Generated: ");
    newE->next = NULL;

    // SIMPLIFIED append logic
    appendEvent(newE->eid, newE);
    
    printf("Event added under ID %d.\n", newE->eid);
}

void viewEvents() {
    int any = 0;
    printf("\n--- Events by ID ---\n");
    for (int i = 0; i < MAX_EVENTS; ++i) {
        EventNode *e = eventLists[i];
        if (!e) continue;
        any = 1;
        printf("\n[Event ID %d]\n", i);
        while (e) {
            float profit = e->revenue - e->cost;
            printf("%d\t%s\t%s\tCost: %.2f\tRev: %.2f\tProfit: %.2f\n",
                   e->eid, e->event_name, e->date, e->cost, e->revenue, profit);
            e = e->next;
        }
    }
    if (!any) printf("No events to show.\n");
}

void deleteEvent() {
    int eid = getInt("Enter Event ID to delete: "); // SIMPLIFIED
    if (eid < 0 || eid >= MAX_EVENTS) { printf("Out of range.\n"); return; }

    EventNode *cur = eventLists[eid];
    if (!cur) { printf("No events with that ID.\n"); return; }
    
    // delete first node of that ID list (as per original logic)
    EventNode *toDelete = cur;
    eventLists[eid] = cur->next; // Unlink
    free(toDelete);
    
    // Also free any feedbacks associated with this event id
    FeedbackNode *f = feedbackLists[eid];
    while (f) { FeedbackNode *tmp = f; f = f->next; free(tmp); }
    feedbackLists[eid] = NULL;
    
    printf("Event (first entry) deleted and feedbacks cleared for ID %d.\n", eid);
}

// ====== Feedback Management ======
void feedbackMenu() {
    int ch;
    do {
        printf("\n--- Feedback ---\n");
        printf("1. Collect Feedback\n2. View Feedback\n3. Back\n");
        ch = getInt("Select: "); // SIMPLIFIED

        switch (ch) {
            case 1: collectFeedback(); break;
            case 2: viewFeedbacks(); break;
            case 3: break;
            default: printf("Invalid!\n");
        }
    } while (ch != 3);
}

void collectFeedback() {
    FeedbackNode *newF = (FeedbackNode *)malloc(sizeof(FeedbackNode));
    if (!newF) { printf("Memory error!\n"); return; }
    
    // SIMPLIFIED input
    newF->eventid = getInt("Event ID: ");
    if (newF->eventid < 0 || newF->eventid >= MAX_EVENTS) { 
        printf("Event ID out of range.\n"); free(newF); return; 
    }
    getString("Feedback: ", newF->feedback, sizeof(newF->feedback));
    newF->next = NULL;

    // SIMPLIFIED append logic
    appendFeedback(newF->eventid, newF);
    
    printf("Feedback stored for event %d.\n", newF->eventid);
}

void viewFeedbacks() {
    int any = 0;
    printf("\n--- Feedback by Event ---\n");
    for (int i = 0; i < MAX_EVENTS; ++i) {
        FeedbackNode *f = feedbackLists[i];
        if (!f) continue;
        any = 1;
        printf("\n[Event %d]\n", i);
        while (f) { printf("- %s\n", f->feedback); f = f->next; }
    }
    if (!any) printf("No feedbacks yet.\n");
}

// ====== Report Module ======
void reportMenu() { generateReport(); }

void generateReport() {
    int memberCount = 0, eventCount = 0;
    float totalCost = 0.0f, totalRevenue = 0.0f;
    int deptCount[MAX_DEPTS] = {0};

    for (int i = 0; i < MAX_DEPTS; ++i) {
        MemberNode *m = memberLists[i];
        while (m) { memberCount++; deptCount[i]++; m = m->next; }
    }

    for (int i = 0; i < MAX_EVENTS; ++i) {
        EventNode *e = eventLists[i];
        while (e) { eventCount++; totalCost += e->cost; totalRevenue += e->revenue; e = e->next; }
    }

    float totalProfit = totalRevenue - totalCost;

    printf("\n--- Club Report ---\n");
    printf("Total Members: %d\n", memberCount);
    printf("Total Events: %d\n", eventCount);
    printf("Department-wise Members:\n");
    for (int i = 0; i < MAX_DEPTS; ++i) {
        if (deptCount[i] > 0) printf("  %s : %d\n", DEPT_NAMES[i], deptCount[i]);
    }

    printf("\n--- Financial Summary ---\n");
    printf("Total Cost Incurred: %.2f\n", totalCost);
    printf("Total Revenue Generated: %.2f\n", totalRevenue);
    printf("Net Profit/Loss: %.2f\n", totalProfit);
}

// ====== CSV Helper Functions (UNCHANGED) ======
// (This complex logic is left as-is to preserve functionality)
static void csv_escape(const char *src, char *dst, size_t dst_size) {
    size_t di = 0;
    if (di + 1 < dst_size) dst[di++] = '"';
    for (size_t i = 0; src[i] != '\0' && di + 1 < dst_size; ++i) {
        if (src[i] == '"') {
            if (di + 2 < dst_size) { dst[di++] = '"'; dst[di++] = '"'; }
            else break;
        } else {
            dst[di++] = src[i];
        }
    }
    if (di + 1 < dst_size) dst[di++] = '"';
    dst[di] = '\0';
}

static char *read_csv_field(const char *line, int field_index, char *out, size_t out_size) {
    const char *p = line;
    int idx = 0;
    size_t oi = 0;

    while (*p && idx <= field_index) {
        if (*p == '"') {
            p++; // skip opening quote
            while (*p) {
                if (*p == '"') {
                    if (*(p+1) == '"') { // escaped quote
                        if (idx == field_index && oi + 1 < out_size) out[oi++] = '"';
                        p += 2; continue;
                    } else { p++; break; }
                } else {
                    if (idx == field_index && oi + 1 < out_size) out[oi++] = *p;
                    p++;
                }
            }
            while (*p && *p != ',') p++;
            if (*p == ',') p++;
        } else {
            while (*p && *p != ',') {
                if (idx == field_index && oi + 1 < out_size) out[oi++] = *p;
                p++;
            }
            if (*p == ',') p++;
        }
        if (idx == field_index) { out[oi] = '\0'; return out; }
        idx++;
    }
    out[0] = '\0';
    return out;
}

// ====== File Save/Load (CSV) ======

// (UNCHANGED)
void saveData() {
    // Members
    FILE *fm = fopen("members.csv", "w");
    if (!fm) { printf("Warning: could not open members.csv for writing.\n"); }
    else {
        fprintf(fm, "id,name,department,contact\n");
        for (int i = 0; i < MAX_DEPTS; ++i) {
            MemberNode *m = memberLists[i];
            while (m) {
                char esc[512];
                csv_escape(m->name, esc, sizeof(esc));
                fprintf(fm, "%d,%s,", m->id, esc);
                csv_escape(m->department, esc, sizeof(esc)); fprintf(fm, "%s,", esc);
                csv_escape(m->contact, esc, sizeof(esc)); fprintf(fm, "%s\n", esc);
                m = m->next;
            }
        }
        fclose(fm);
    }

    // Events
    FILE *fe = fopen("events.csv", "w");
    if (!fe) { printf("Warning: could not open events.csv for writing.\n"); }
    else {
        fprintf(fe, "eid,event_name,date,description,cost,revenue\n");
        for (int i = 0; i < MAX_EVENTS; ++i) {
            EventNode *e = eventLists[i];
            while (e) {
                char esc[1024];
                fprintf(fe, "%d,", e->eid);
                csv_escape(e->event_name, esc, sizeof(esc)); fprintf(fe, "%s,", esc);
                csv_escape(e->date, esc, sizeof(esc)); fprintf(fe, "%s,", esc);
                csv_escape(e->description, esc, sizeof(esc)); fprintf(fe, "%s,", esc);
                fprintf(fe, "%.2f,%.2f\n", e->cost, e->revenue);
                e = e->next;
            }
        }
        fclose(fe);
    }

    // Feedback
    FILE *ff = fopen("feedback.csv", "w");
    if (!ff) { printf("Warning: could not open feedback.csv for writing.\n"); }
    else {
        fprintf(ff, "eventid,feedback\n");
        for (int i = 0; i < MAX_EVENTS; ++i) {
            FeedbackNode *f = feedbackLists[i];
            while (f) {
                char esc[1024]; csv_escape(f->feedback, esc, sizeof(esc));
                fprintf(ff, "%d,%s\n", f->eventid, esc);
                f = f->next;
            }
        }
        fclose(ff);
    }
}

// (SIMPLIFIED by using append helpers)
void loadData() {
    char line[2048];

    // Load Members
    FILE *fm = fopen("members.csv", "r");
    if (fm) {
        if (fgets(line, sizeof(line), fm) != NULL) { // skip header
            while (fgets(line, sizeof(line), fm)) {
                size_t ln = strlen(line);
                if (ln && (line[ln-1] == '\n' || line[ln-1] == '\r')) line[--ln] = '\0';
                char idbuf[32], name[256], dept[128], contact[64];
                read_csv_field(line, 0, idbuf, sizeof(idbuf));
                read_csv_field(line, 1, name, sizeof(name));
                read_csv_field(line, 2, dept, sizeof(dept));
                read_csv_field(line, 3, contact, sizeof(contact));

                MemberNode *m = (MemberNode *)malloc(sizeof(MemberNode));
                if (!m) break;
                m->id = atoi(idbuf);
                strncpy(m->name, name, sizeof(m->name)-1); m->name[sizeof(m->name)-1] = '\0';
                strncpy(m->department, dept, sizeof(m->department)-1); m->department[sizeof(m->department)-1] = '\0';
                strncpy(m->contact, contact, sizeof(m->contact)-1); m->contact[sizeof(m->contact)-1] = '\0';
                m->next = NULL;

                int idx = getDeptIndex(m->department);
                if (idx == -1) idx = MAX_DEPTS - 1; // OTHER
                
                // SIMPLIFIED append logic
                appendMember(idx, m); 
            }
        }
        fclose(fm);
    }

    // Load Events
    FILE *fe = fopen("events.csv", "r");
    if (fe) {
        if (fgets(line, sizeof(line), fe) != NULL) {
            while (fgets(line, sizeof(line), fe)) {
                size_t ln = strlen(line);
                if (ln && (line[ln-1] == '\n' || line[ln-1] == '\r')) line[--ln] = '\0';
                char eidbuf[32], name[512], date[64], desc[512], costbuf[64], revbuf[64];
                read_csv_field(line, 0, eidbuf, sizeof(eidbuf));
                read_csv_field(line, 1, name, sizeof(name));
                read_csv_field(line, 2, date, sizeof(date));
                read_csv_field(line, 3, desc, sizeof(desc));
                read_csv_field(line, 4, costbuf, sizeof(costbuf));
                read_csv_field(line, 5, revbuf, sizeof(revbuf));

                EventNode *e = (EventNode *)malloc(sizeof(EventNode));
                if (!e) break;
                e->eid = atoi(eidbuf);
                if (e->eid < 0 || e->eid >= MAX_EVENTS) { free(e); continue; }
                strncpy(e->event_name, name, sizeof(e->event_name)-1); e->event_name[sizeof(e->event_name)-1] = '\0';
                strncpy(e->date, date, sizeof(e->date)-1); e->date[sizeof(e->date)-1] = '\0';
                strncpy(e->description, desc, sizeof(e->description)-1); e->description[sizeof(e->description)-1] = '\0';
                e->cost = atof(costbuf);
                e->revenue = atof(revbuf);
                e->next = NULL;

                // SIMPLIFIED append logic
                appendEvent(e->eid, e);
            }
        }
        fclose(fe);
    }

    // Load Feedback
    FILE *ff = fopen("feedback.csv", "r");
    if (ff) {
        if (fgets(line, sizeof(line), ff) != NULL) {
            while (fgets(line, sizeof(line), ff)) {
                size_t ln = strlen(line);
                if (ln && (line[ln-1] == '\n' || line[ln-1] == '\r')) line[--ln] = '\0';
                char idbuf[32], fback[512];
                read_csv_field(line, 0, idbuf, sizeof(idbuf));
                read_csv_field(line, 1, fback, sizeof(fback));
                FeedbackNode *f = (FeedbackNode *)malloc(sizeof(FeedbackNode));
                if (!f) break;
                f->eventid = atoi(idbuf);
                if (f->eventid < 0 || f->eventid >= MAX_EVENTS) { free(f); continue; }
                strncpy(f->feedback, fback, sizeof(f->feedback)-1); f->feedback[sizeof(f->feedback)-1] = '\0';
                f->next = NULL;

                // SIMPLIFIED append logic
                appendFeedback(f->eventid, f);
            }
        }
        fclose(ff);
    }
}