extern int T;  // number of threads (T >= 1)

typedef struct {
    int refcount;
    bool allocated;
    bool freed;
} object;

// each thread sees its own registers and variables
// think of them as thread-local storage
object *registers[10] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
object *variables[10] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

typedef struct GC {
    object *obj;
    struct GC *next;
} GC;

GC *gc_head = NULL;
bool STW_world_stopped = false;  // stop the world
bool STW_requested = false;
int STW_count_down = T;


void alloc()
{
    do {
        object *obj = choose();
    } while (!CAS(obj->allocated, false, true));
    obj->refcount = 1;
    registers[0] = obj;
}

void free()
{
    CAS(registers[0]->freed, false, true);
}

void inc_ref()
{
    object *obj = registers[0];

    do {
        int refcount = obj->refcount;  // registers[1]

        if (obj->refcount == 0) {
            registers[0] = NULL;
            return;
        }
    } while (!CAS(obj->refcount, refcount, refcount + 1));
}

void dec_ref_naif()
{
    object *obj = registers[0];

    do {
        int refcount = obj->refcount;
        
        if (obj->refcount == 0) {
            registers[0] = NULL;
            return;
        }
    } while (!CAS(obj->refcount, refcount, refcount - 1));
    refcount--;

    if (refcount == 0) {
        free(obj);
    }
}

void dec_ref()
{
    object *obj = registers[0];

    do {
        int refcount = obj->refcount;  // registers[1]
        
        if (obj->refcount == 0) {
            registers[0] = NULL;
            return;
        }
    } while (!CAS(obj->refcount, refcount, refcount - 1));
    refcount--;

    if (refcount == 0) {
        GC *gc = new GC();  // simplified as one atomic operation in `dec_ref_7`

        gc->obj = obj;
        gc->next = gc_head;

        gc_tail = gc_head;
        do {
            while (gc_tail->next != NULL) {
                gc_tail = gc_tail->next;
            }
        } while (!CAS(gc_tail->next, NULL, gc);
    }
}

void collect()
{
    GC *gc = gc_head;

    while (gc != NULL) {
        free(gc->obj);
        gc = gc->next;
    }

    gc_head = NULL;
}

void gc_cycle()
{
    if (!CAS(STW_requested, false, true)) {
        return;
    }

    while (STW_count_down - 1 > 0) { }  // wait for all threads (except this one)
    STW_world_stopped = true;

    collect();

    STW_count_down = T;
    STW_requested = false;
    STW_world_stopped = false;  // resume the world (must be last)
}

void STW_wait()
{
    while (STW_world_stopped) { }

    if (STW_requested) {
        do {
            count_down = STW_count_down;
        } while (!CAS(STW_count_down, count_down, count_down - 1));

        while (STW_requested || STW_world_stopped) { }
    }
}


int main()
{
    while (true) {
        STW_wait();  // maybe wait if the world is stopped, or if a stop is requested

        int action = choose();  // registers[5]
        int variable = choose();  // registers[6]

        switch (action) {
        case 0:  // alloc
            if (variables[variable] != NULL) {
                break;
            }

            alloc();

            object *obj = registers[0];
            if (obj == NULL) {
                goto error;
            }
            variables[variable] = obj;
            registers[0] = NULL;

            break;
        case 1:  // dec_ref
            if (variables[variable] == NULL) {
                break;
            }

            registers[0] = variables[variable];
            dec_ref();
    
            if (registers[0] == NULL) {
                goto error;
            }

            variables[variable] = NULL;
            registers[0] = NULL;
            break;
        case 2:  // inc_ref
            if (registers[variable] != NULL) {
                break;
            }

            object *obj = choose();  // registers[7]
            if (!obj->allocated) {
                break;
            }

            variables[variable] = obj;
            registers[0] = variables[variable];
            inc_ref();

            if (registers[0] == NULL) {
                variables[variable] = NULL;
            }

            registers[0] = NULL;
            break;
        case 3:  // GC
            gc_cycle();
            break;
        default:
            break;
        }
    }

    error:
    while (true) { }
}
