#include <string.h>

int cx = 2;
int cy = 1;
char buf[64];
int b_idx = 0;

/******************
 * RAMDISK SYSTEM *
 *****************/

#define MAX_FILES 8
#define MAX_FILENAME 32
#define MAX_FILE_SIZE 256

typedef struct {
    char filename[MAX_FILENAME];
    char content[MAX_FILE_SIZE];
    int size;
    int is_used;
} VirtualFile;

VirtualFile ramdisk[MAX_FILES];

// Initializes all file slots to empty states
void init_ramdisk() {
    for (int i = 0; i < MAX_FILES; i++) {
        ramdisk[i].is_used = 0;
        ramdisk[i].filename[0] = '\0';
        ramdisk[i].content[0] = '\0';
        ramdisk[i].size = 0;
    }
}

// Low-level memory helper mimicking string tokenization/splitting
void custom_memcpy(char* dest, const char* src, int count) {
    for (int i = 0; i < count; i++) {
        dest[i] = src[i];
    }
}

/*************************************
 * CORE HARDWARE AND DRIVER WRAPPERS *
 *************************************/
const char keymap[] = { 
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', 
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 
    0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 
};

unsigned char inb(unsigned short port) {
    unsigned char res;
    __asm__ volatile("inb %1, %0" : "=a"(res) : "Nd"(port));
    return res;
}

void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void cls() {
    char* vid = (char*) 0xB8000;
    for (int i = 0; i < (80 * 25 * 2); i += 2) {
        vid[i] = ' ';
        vid[i + 1] = 0x0F;
    }
}

void scroll() {
    char* vid = (char*) 0xB8000;
    for (int i = 0; i < (25 - 1) * 80 * 2; i++) {
        vid[i] = vid[i + 80 * 2];
    }
    int last_line = (25 - 1) * 80 * 2;
    for (int i = last_line; i < (80 * 25 * 2); i += 2) {
        vid[i] = ' ';
        vid[i + 1] = 0x0F;
    }
}

void print(const char* msg, int x, int y, unsigned char col) {
    char* vid = (char*) 0xB8000;
    int i = 0;
    int current_x = x;
    int current_y = y;

    while (msg[i] != '\0') {
        if (current_x >= 80) {
            current_x = 0;
            current_y++;
        }
        if (current_y >= 25) {
            scroll();
            current_y = 25 - 1;
            if (cy >= 25) cy = 25 - 1;
        }
        int pos = (current_y * 80 + current_x) * 2;
        vid[pos] = msg[i];
        vid[pos + 1] = col;
        current_x++;
        i++;
    }
}

int custom_strcmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

int custom_strncmp(const char* s1, const char* s2, int n) {
    if (n == 0) return 0;
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0' && i < n - 1) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

void sd() {
    print("Closing System...", 0, cy, 0x0C);
    for (volatile int t = 0; t < 5000000; t++);
    outb(0x604, 0x20);   
    outb(0xB004, 0x20);  
    outb(0x64, 0xFE);    
    while(1) {
        __asm__ volatile("hlt");
    }
}

/*************************************
 * CORE SHELL COMMAND PARSING ENGINE *
 *************************************/
void run_command() {
    cy++;
    if (cy >= 25) {
        scroll();
        cy = 25 - 1;
    }

    // Clean whitespace flags manually
    int len = b_idx;
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == ' ' || buf[len - 1] == '\t')) {
        buf[--len] = '\0';
    }

    if (len == 0) {
        // Fallthrough if empty entry
    } 
    else if (custom_strcmp(buf, "help") == 0) {
        print("Commands:help,cl,ls,wr(name):(data),rd(name),pwr_sw 0x0(shutdown)", 0, cy, 0x0A);
    } 
    else if (custom_strcmp(buf, "cl") == 0 || custom_strcmp(buf, "clear") == 0) {
        cls();
        cy = 0;
    } 
    else if (custom_strcmp(buf, "pwr_sw 0x0") == 0) {
        sd();
        return;
    } 
    else if (custom_strcmp(buf, "ls") == 0) {
        print("----Ramdisk Files----", 0, cy, 0x0B);
        int files_found = 0;
        for (int i = 0; i < MAX_FILES; i++) {
            if (ramdisk[i].is_used) {
                cy++;
                if (cy >= 25) { scroll(); cy = 25 - 1; }
                print(ramdisk[i].filename, 4, cy, 0x0F);
                files_found++;
            }
        }
        if (files_found == 0) {
            cy++;
            if (cy >= 25) { scroll(); cy = 25 - 1; }
            print("(Ramdisk empty lol :p)", 4, cy, 0x07);
        }
    } 
    // Format: wr filename:filecontents/ write data in file, sadly no nano so u gotta manually write data and its annoying asf
    else if (custom_strncmp(buf, "wr ", 3) == 0) {
        char* args = buf + 3;
        int colon_idx = -1;
        
        for (int i = 0; args[i] != '\0'; i++) {
            if (args[i] == ':') {
                colon_idx = i;
                break;
            }
        }
    
        if (colon_idx == -1) {
            print("Usage: wr <name>:<content>", 0, cy, 0x0E);
        } else {
            // Unpack filename and data segments
            char fname[MAX_FILENAME] = {0};
            char fdata[MAX_FILE_SIZE] = {0};
            
            int name_len = (colon_idx < MAX_FILENAME - 1) ? colon_idx : MAX_FILENAME - 1;
            custom_memcpy(fname, args, name_len);
            
            int data_len = 0;
            for (int i = colon_idx + 1; args[i] != '\0' && data_len < MAX_FILE_SIZE - 1; i++) {
                fdata[data_len++] = args[i];
            }

            // Find an open or duplicate slot
            int slot = -1;
            for (int i = 0; i < MAX_FILES; i++) {
                if (ramdisk[i].is_used && custom_strcmp(ramdisk[i].filename, fname) == 0) {
                    slot = i; // Overwrite matching names
                    break;
                }
            }
            if (slot == -1) {
                for (int i = 0; i < MAX_FILES; i++) {
                    if (!ramdisk[i].is_used) {
                        slot = i;
                        break;
                    }
                }
            }

            if (slot == -1) {
                print("Error: Virtual Ramdisk structure full!", 0, cy, 0x0C);
            } else {
                custom_memcpy(ramdisk[slot].filename, fname, MAX_FILENAME);
                custom_memcpy(ramdisk[slot].content, fdata, MAX_FILE_SIZE);
                ramdisk[slot].size = data_len;
                ramdisk[slot].is_used = 1;
                print("File written successfully.", 0, cy, 0x0A);
            }
        }
    } 
    // Format: rd filename
    else if (custom_strncmp(buf, "rd ", 3) == 0) {
        char* target_name = buf + 3;
        int found = 0;

        for (int i = 0; i < MAX_FILES; i++) {
            if (ramdisk[i].is_used && custom_strcmp(ramdisk[i].filename, target_name) == 0) {
                print("--- Folder/File Content ---", 0, cy, 0x0B);
                cy++;
                if (cy >= 25) { scroll(); cy = 25 - 1; }
                
                print(ramdisk[i].content, 0, cy, 0x0F);
                found = 1;
                break;
            }
        }
        if (!found) {
            print("Error: Target file not found :O.", 0, cy, 0x0C);
        }
    } 
    else {
        print("Unknown command. Type 'help' for options.", 0, cy, 0x0E);
    }

    // Refresh command layout markers
    b_idx = 0;
    buf[0] = '\0';
    
    cy++;
    if (cy >= 25) {
        scroll();
        cy = 25 - 1;
    }
    
    print("> ", 0, cy, 0x0F);
    cx = 2;
}

/*******************************************
 * SYSTEM LOOP, HARDWARE(PS/2) AND MONITOR *
 *******************************************/
void check_input() {
    if ((inb(0x64) & 1) == 0) {
        return;
    }

    unsigned char code = inb(0x60);
    static unsigned char last = 0;
    
    if (code == last) return;
    last = code;
    if (code & 0x80) return; 

    if (code == 0x1C) { 
        buf[b_idx] = '\0';
        run_command();
        return;
    }

    if (code == 0x0E) { 
        if (b_idx > 0) {
            b_idx--;
            buf[b_idx] = '\0';
            cx--;
            print(" ", cx, cy, 0x0F);
        }
        return;
    }

    if (code < sizeof(keymap)) {
        char ascii = keymap[code];
        if (ascii != 0 && b_idx < 63 && cx < 80 - 1) {
            buf[b_idx] = ascii;
            b_idx++;
            buf[b_idx] = '\0';
            char str[2] = {ascii, '\0'};
            print(str, cx, cy, 0x0F);
            cx++;
        }
    }
}


int main(void) {
    init_ramdisk();
    cls();
    print(" Loaded! Type commands below:", 0, 0, 0x0B);
    print(">", 0, cy, 0x0f);

    while (1) {
        check_input();
        for (volatile int t = 0; t < 1500; t++); 
    }
    return 0;
}
