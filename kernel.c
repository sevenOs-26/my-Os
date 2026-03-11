/* --- SEVENOS TYPE DEFINITIONS --- */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

#define PORT 0x3F8
#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

volatile unsigned short* terminal_buffer = (volatile unsigned short*)0xB8000;
static int cursor_x = 0;
static int cursor_y = 0;
static char cmd_buffer[64];
static int cmd_idx = 0;

unsigned char current_theme_color = 0x0F; // Default: White

unsigned char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', 0,
    '\\', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 0, '*', 0, ' '
};

/* --- 3. SEVENOS FILE SYSTEM (V1.0) --- */
#define MAX_FILES 10
#define FILENAME_SIZE 12
#define MAX_FILE_SIZE 512

struct File {
    char name[FILENAME_SIZE];
    int size;
    int active; 
    char data[MAX_FILE_SIZE];
};

struct File file_system[MAX_FILES];
int file_count = 0;
int inactivity_timer = 0; // Para sa Matrix

/* --- 1. ARCHITECTURE BRIDGE (Diri nimo i-add ang inw) --- */

// I-send ang 8-bit data
static inline void outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// I-send ang 16-bit data (Gamit sa Disk Write)
static inline void outw(unsigned short port, unsigned short val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

// I-receive ang 8-bit data
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// I-receive ang 16-bit data (Gamit sa Disk Read - KANI ANG MISSING)
static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    asm volatile ("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

typedef unsigned int uint32_t;
extern uint32_t end;

int system_running = 1; // Kinahanglan naa ni sa gawas para makita sa tanan

// I-define ni sa gawas (Global Variables) para ma-access sa keyboard_handler
int screensaver_active = 0;

/* --- PROTOTYPES: PAILA-ILA SA COMPILER --- */
void draw_status_bar();
void draw_toolbar();
void write_char_at(char c, char color, int x, int y);
void print_at(char* str, char color, int x, int y);

/* --- 2. BASIC VGA & SCROLLING --- */
void vga_put_char(int x, int y, char c, unsigned char color) {
    if (x >= 80 || y >= 25) return;
    terminal_buffer[y * 80 + x] = (unsigned short)c | (unsigned short)color << 8;
}

void write_char_at(char c, char color, int x, int y) {
    volatile char* video_mem = (volatile char*)0xB8000;
    int offset = (y * 80 + x) * 2;
    video_mem[offset] = c;
    video_mem[offset + 1] = color;
}

void print_at(char* str, char color, int x, int y) {
    for (int i = 0; str[i] != '\0'; i++) {
        write_char_at(str[i], color, x + i, y);
    }
}

void check_scroll() {
    // 1. I-check kon ang cursor naa na ba sa "Danger Zone" (Row 24 - Toolbar)
    if (cursor_y >= 24) {
        volatile char* video_mem = (volatile char*)0xB8000;
        
        // 2. I-irog ang sulod sa Shell (Row 1 hangtod Row 23)
        // Gilaktawan nato ang Row 0 (Status Bar) ug Row 24 (Toolbar)
        for (int y = 1; y < 23; y++) {
            for (int x = 0; x < 80 * 2; x++) {
                // I-copy ang line sa ubos padulong sa line sa ibabaw
                video_mem[y * 160 + x] = video_mem[(y + 1) * 160 + x];
            }
        }

        // 3. Limpyohan ang Row 23 (ang last line sa shell area)
        // Para dili mapisat ang text sa toolbar
        for (int x = 0; x < 80; x++) {
            write_char_at(' ', 0x0F, x, 23);
        }

        // 4. I-lock ang cursor sa Row 23 (sa ibabaw ra gyud sa toolbar)
        cursor_y = 23;

        // 5. I-refresh ang UI para sigurado nga wala na-kuskus
        draw_status_bar();
        draw_toolbar();
    }
}

void vga_print(const char* str, unsigned char color) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') { cursor_x = 0; cursor_y++; }
        else { vga_put_char(cursor_x++, cursor_y, str[i], color); }
        if (cursor_x >= 80) { cursor_x = 0; cursor_y++; }
        check_scroll();
    }
}

void clear_screen() {
    for (int i = 0; i < 80 * 25; i++) terminal_buffer[i] = (unsigned short)' ' | (unsigned short)0x07 << 8;
    cursor_x = 0; cursor_y = 0;
}

/* --- 3. MATH & CONVERSION TOOLS --- */
void vga_print_int(int n) {
    if (n == 0) { vga_put_char(cursor_x++, cursor_y, '0', 0x0F); return; }
    if (n < 0) { vga_put_char(cursor_x++, cursor_y, '-', 0x0F); n = -n; }
    char buf[12]; int i = 0;
    while (n > 0) { buf[i++] = (n % 10) + '0'; n /= 10; }
    while (--i >= 0) {
        vga_put_char(cursor_x++, cursor_y, buf[i], 0x0F);
        if (cursor_x >= 80) { cursor_x = 0; cursor_y++; check_scroll(); }
    }
}

// I-declare nato ang helper para kaila ang ME command
void vga_print_hex_32(uint32_t n) {
    char *chars = "0123456789ABCDEF";
    // I-print ang matag nibble (total 8 nibbles para sa 32-bit)
    for (int i = 7; i >= 0; i--) {
        vga_put_char(cursor_x++, cursor_y, chars[(n >> (i * 4)) & 0x0F], 0x0E);
    }
}

void vga_print_binary(unsigned char n) {
    for (int i = 7; i >= 0; i--) {
        vga_put_char(cursor_x++, cursor_y, (n & (1 << i)) ? '1' : '0', (n & (1 << i)) ? 0x0E : 0x07);
        if (cursor_x >= 80) { cursor_x = 0; cursor_y++; check_scroll(); }
    }
}

int string_to_int(char* s, int* pos) {
    int res = 0;
    while (s[*pos] != '\0' && (s[*pos] < '0' || s[*pos] > '9')) (*pos)++;
    while (s[*pos] >= '0' && s[*pos] <= '9') { res = res * 10 + (s[*pos] - '0'); (*pos)++; }
    return res;
}

/* --- 4. RTC CLOCK LOGIC --- */
void vga_print_at_pos(const char* str, unsigned char color, int row, int col) {
    int old_x = cursor_x; int old_y = cursor_y;
    cursor_x = col; cursor_y = row;
    vga_print(str, color);
    cursor_x = old_x; cursor_y = old_y;
}

void vga_print_int_at(int n, int row, int col) {
    int old_x = cursor_x; int old_y = cursor_y;
    cursor_x = col; cursor_y = row;
    vga_print_int(n);
    cursor_x = old_x; cursor_y = old_y;
}

/* --- EXISTING: RTC REGISTER ACCESS --- */
unsigned char get_rtc_register(int reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

void display_date() {
    outb(CMOS_ADDR, 0x0A);
    while (inb(CMOS_DATA) & 0x80); 

    unsigned char day   = get_rtc_register(0x07);
    unsigned char month = get_rtc_register(0x08);
    unsigned char year  = get_rtc_register(0x09);

    day   = (day & 0x0F) + ((day / 16) * 10);
    month = (month & 0x0F) + ((month / 16) * 10);
    year  = (year & 0x0F) + ((year / 16) * 10);

    vga_print("\nDATE: ", 0x0B); 
    vga_print_int(month); vga_print("/", 0x0F);
    vga_print_int(day);   vga_print("/", 0x0F);
    vga_print("20", 0x0F); // GI-FIX: Gidugangan og 0x0F nga color
    vga_print_int(year); 
    vga_print("\n", 0x07);
}

void display_time() {
    outb(CMOS_ADDR, 0x0A);
    if (inb(CMOS_DATA) & 0x80) return;

    unsigned char hour = get_rtc_register(0x04);
    unsigned char min  = get_rtc_register(0x02);
    unsigned char sec  = get_rtc_register(0x00);

    hour = (hour & 0x0F) + ((hour / 16) * 10);
    min  = (min & 0x0F) + ((min / 16) * 10);
    sec  = (sec & 0x0F) + ((sec / 16) * 10);

    vga_print_at_pos("TIME: ", 0x1E, 0, 60);
    vga_print_int_at(hour, 0, 66); vga_print_at_pos(":", 0x1E, 0, 68);
    vga_print_int_at(min, 0, 69);  vga_print_at_pos(":", 0x1E, 0, 71);
    vga_print_int_at(sec, 0, 72);
}

/* --- 5. UI BRANDING & SECURITY --- */
void draw_status_bar() {
    for(int i = 0; i < 80; i++) vga_put_char(i, 0, ' ', 0x1F);
    vga_print_at_pos(" SevenOS v3.2 | MASTER ALOY | [HE] Help ", 0x1F, 0, 1);
    display_time();
}

void display_banner() {
    // Ang Main Logo (Seven) - High Tech Cyan (0x0B)
    vga_print("  ____  _______      _______ _   _ \n", 0x0B);
    vga_print(" / ___|| ____\\ \\   / / ____| \\ | |\n", 0x0B);
    vga_print(" \\___ \\|  _|   \\ \\ / /|  _| |  \\| |\n", 0x0B);
    vga_print("  ___) | |___   \\ V / | |___| |\\  |\n", 0x0B);
    vga_print(" |____/|_____|   \\_/  |_____|_| \\_|\n", 0x0B);

    // Branding ug Version - Pure White (0x0F)
    vga_print("\n        SEVEN OS v3.2 - ARCHITECT ELITE\n", 0x0F);
    
    // Ang Full Brand Mantra
    // "Build the Future." (Cyan 0x0B) ug "Start before you're ready." (Gray 0x07)
    vga_print("    Build the Future. ", 0x0B);
    vga_print("\"Start before you're ready.\"\n", 0x07);
    
    // Separator para limpyo tan-awon - Dark Gray (0x08)
    vga_print(" ------------------------------------------\n", 0x08);
}

void encrypt_text(char* input) {
    vga_print("Original: ", 0x07); vga_print(input, 0x07); vga_print("\nEncrypted: ", 0x0A);
    for (int i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        if (c >= 'A' && c <= 'Z') c = ((c - 'A' + 3) % 26) + 'A';
        else if (c >= '0' && c <= '9') c = ((c - '0' + 3) % 10) + '0';
        char out[2] = {c, '\0'}; vga_print(out, 0x0A);
    }
    vga_print("\n", 0x07);
}

void decrypt_text(char* input) {
    vga_print("Ciphertext: ", 0x07); vga_print(input, 0x07); vga_print("\nDecrypted: ", 0x0B);
    for (int i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        if (c >= 'A' && c <= 'Z') c = ((c - 'A' + 23) % 26) + 'A';
        else if (c >= '0' && c <= '9') c = ((c - '0' + 7) % 10) + '0';
        char out[2] = {c, '\0'}; vga_print(out, 0x0B);
    }
    vga_print("\n", 0x07);
}

void crack_password(char* target) {
    char guess[6]; guess[5] = '\0';
    int attempts = 0;
    static const char* charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    vga_print("STRESS TEST (5-Chars): ", 0x0E); vga_print(target, 0x0E); vga_print("\n", 0x07);
    for (int i=0; i<36; i++) {
        for (int j=0; j<36; j++) {
            for (int k=0; k<36; k++) {
                for (int l=0; l<36; l++) {
                    for (int m=0; m<36; m++) {
                        guess[0]=charset[i]; guess[1]=charset[j]; guess[2]=charset[k]; guess[3]=charset[l]; guess[4]=charset[m];
                        attempts++;
                        if (guess[0]==target[0] && guess[1]==target[1] && guess[2]==target[2] && guess[3]==target[3] && guess[4]==target[4]) {
                            vga_print("\nSUCCESS! Pass: ", 0x0A); vga_print(guess, 0x0A);
                            vga_print("\nAttempts: ", 0x07); vga_print_int(attempts); vga_print("\n", 0x07);
                            return;
                        }
                    }
                }
            }
        }
    }
}

void reboot() {
    vga_print("\nSystem Rebooting...", 0x0E);
    // 8042 Keyboard Controller Reset 
    unsigned char good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
}

void display_whoami() {
    vga_print("\n--- [ ARCHITECT PROFILE ] ---\n", 0x0B); // Cyan
    vga_print("NAME    : ", 0x0F); vga_print("MASTER ALOY\n", 0x0E); // Yellow
    vga_print("ROLE    : ", 0x0F); vga_print("SYSTEM ARCHITECT ELITE\n", 0x0E);
    vga_print("OS      : ", 0x0F); vga_print("SevenOS v3.2 (Custom Kernel)\n", 0x0E);
    vga_print("STRENGTH: ", 0x0F); vga_print("Brute Force & Logic Cracking\n", 0x0A); // Green
    vga_print("-----------------------------\n", 0x0B);
}

void change_color(const char* hex) {
    // Simple hex to int conversion para sa 2 digits
    unsigned char high = 0, low = 0;
    
    // Convert first digit
    if (hex[0] >= '0' && hex[0] <= '9') high = hex[0] - '0';
    else if (hex[0] >= 'A' && hex[0] <= 'F') high = hex[0] - 'A' + 10;
    
    // Convert second digit
    if (hex[1] >= '0' && hex[1] <= '9') low = hex[1] - '0';
    else if (hex[1] >= 'A' && hex[1] <= 'F') low = hex[1] - 'A' + 10;
    
    current_theme_color = (high << 4) | low;
    vga_print("\nTheme Updated!", current_theme_color);
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void update_cursor(int x, int y) {
    unsigned short pos = y * 80 + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char) ((pos >> 8) & 0xFF));
}

void draw_toolbar() {
    int row = 24; // Last row sa 80x25 VGA screen
    // 1. Draw the background bar (Grey Background 0x7, Black Text 0x0)
    for (int col = 0; col < 80; col++) {
        write_char_at(' ', 0x70, col, row); 
    }

    // 2. I-butang ang mga Hotkeys para dali makita
    print_at(" F1-HELP | F2-CLEAR | F3-7OS | F5-CRACK | F10-REBOOT ", 0x70, 2, row);
}

unsigned long long read_tsc() {
    unsigned int lo, hi;
    asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((unsigned long long)hi << 32) | lo;
}

int get_random() {
    return (int)(read_tsc() & 0xFFF); // Kuhaon lang ang last digits
}

int check_keyboard_raw() {
    // I-check ang Status Register sa Keyboard (Port 0x64)
    // Kon ang bit 0 kay 1, naay naghuwat nga key
    unsigned char status;
    asm volatile("inb $0x64, %0" : "=a"(status));
    return (status & 0x01); 
}

void run_matrix() {
    clear_screen();
    int running = 1;

    while(running) {
        // 1. Paghimo og "Rain" sa random columns
        for (int i = 0; i < 5; i++) { // 5 characters kada "tick"
            int x = get_random() % 80;
            int y = get_random() % 25;
            char c = (char)((get_random() % 94) + 33); // Random ASCII characters
            
            // 0x02 = Green text on Black background
            write_char_at(c, 0x02, x, y);
        }

        // 2. IMPORTANTE: Exit Check (Para dili ma-stuck)
        // I-check ang Port 0x64 (Keyboard Status)
        unsigned char status;
        asm volatile("inb $0x64, %0" : "=a"(status));
        if (status & 0x01) {
            unsigned char dummy;
            asm volatile("inb $0x60, %0" : "=a"(dummy)); // Limpyohan ang buffer
            running = 0; // Gawas sa loop
        }

        // 3. Delay (Kon wala ni, paspas ra kaayo, dli makita sa mata)
        for (volatile int d = 0; d < 1000000; d++); 
    }

    // 4. Inig mata sa OS, i-redraw tanan UI
    clear_screen();
    draw_status_bar();
    draw_toolbar();
    display_banner();
    vga_print("7shell >> ", 0x0B);
}

void list_files() {
    vga_print("\n--- SevenOS Explorer ---", 0x1F); // Blue background (XP vibe)
    vga_print("\n", 0x0F);

    int found = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_system[i].active == 1) {
            vga_print("[FILE] ", 0x0A); // Green label
            vga_print(file_system[i].name, 0x0F);
            vga_print("\n", 0x0F);
            found = 1;
        }
    }

    if (!found) {
        vga_print("No files found. Try 'touch' to create one.\n", 0x07);
    }
    vga_print("------------------------\n", 0x1F);
}

void create_file(char* input) {
    // Karon, ang 'input' naay format nga "FILENAME CONTENT"
    // Atong i-split (simple logic)
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_system[i].active == 0) {
            int j = 0;
            // 1. Copy Filename hangtod makakita og space
            while (input[j] != ' ' && input[j] != '\0' && j < FILENAME_SIZE - 1) {
                file_system[i].name[j] = input[j];
                j++;
            }
            file_system[i].name[j] = '\0';

            // 2. Kon naay space, ang sunod kay CONTENT na
            if (input[j] == ' ') {
                j++; // Laktawan ang space
                int k = 0;
                while (input[j] != '\0' && k < 255) {
                    file_system[i].data[k] = input[j];
                    j++; k++;
                }
                file_system[i].data[k] = '\0';
            }

            file_system[i].active = 1;
            vga_print("Created & Saved: ", 0x0B);
            vga_print(file_system[i].name, 0x0F);
            vga_print("\n", 0x0F);
            return;
        }
    }
}

// --- 1. STRING UTILITY ---
int str_compare(char* s1, char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return 0;
        i++;
    }
    return (s1[i] == '\0' && s2[i] == '\0');
}

// --- 2. FS UTILITIES ---
int find_file(char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        // Karon, kaila na ang compiler sa str_compare!
        if (file_system[i].active == 1 && str_compare(file_system[i].name, name)) {
            return i;
        }
    }
    return -1;
}

// ATA PIO: I-save ang usa ka sector (512 bytes) ngadto sa Hard Disk
void disk_write_sector(uint32_t lba, uint8_t *buffer) {
    outb(0x1F6, (lba >> 24) | 0xE0); // Select drive & LBA
    outb(0x1F2, 1);                  // Pila ka sector (1)
    outb(0x1F3, (uint8_t)lba);       // LBA bits 0-7
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30);               // Command 0x30 = Write Sectors

    // Wait for disk to be ready
    while ((inb(0x1F7) & 0x80) || !(inb(0x1F7) & 0x08));

    // I-transfer ang buffer (256 words = 512 bytes)
    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i*2] | (buffer[i*2+1] << 8);
        outw(0x1F0, data);
    }
}

// ATA PIO: I-read ang usa ka sector gikan sa disk
void disk_read_sector(uint32_t lba, uint8_t *buffer) {
    outb(0x1F6, (lba >> 24) | 0xE0);
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x20);               // Command 0x20 = Read Sectors

    while ((inb(0x1F7) & 0x80) || !(inb(0x1F7) & 0x08));

    for (int i = 0; i < 256; i++) {
        uint16_t data = inw(0x1F0); // Kinahanglan pa nimo i-define ang inw
        buffer[i*2] = (uint8_t)data;
        buffer[i*2+1] = (uint8_t)(data >> 8);
    }
}

void load_all_files() {
    // I-point ang pointer sa sugod sa imong file_system array
    uint8_t *ptr = (uint8_t*)file_system;
    uint32_t start_lba = 10; // Dinhi nato gi-save sa SV command
    
    // Atong kalkulahon pila ka 512-byte blocks ang gikinahanglan
    // Tungod kay ang file_system array dako man (10 files * ~528 bytes)
    int total_sectors = (sizeof(file_system) / 512) + 1;

    for (int i = 0; i < total_sectors; i++) {
        // I-read ang matag sector padulong sa RAM address
        disk_read_sector(start_lba + i, ptr + (i * 512));
    }
}

void remove_file(char* filename) {
    int found = 0;
    // Siguroha nga ang '16' o 'MAX_FILES' match sa imong struct definition
    for (int i = 0; i < 16; i++) { 
        if (file_system[i].active && str_compare(file_system[i].name, filename)) {
            file_system[i].active = 0; // Logical delete
            found = 1;
            break;
        }
    }

    if (found) {
        // I-sync sa Sector 10 (Gikuha ni nako sa imong SV command logic)
        disk_write_sector(10, (uint8_t*)file_system);
        vga_print("\n[FS] File removed and Disk Synced.\n", 0x0A);
    } else {
        vga_print("\n[ERR] File not found.\n", 0x0C);
    }
}

void exit_system() {
    vga_print("\n[SYSTEM] Syncing Disk...", 0x0E);
    disk_write_sector(10, (uint8_t*)file_system);
    
    vga_print("\n[SYSTEM] Exiting SevenOS. Returning to Alpine...", 0x0A);

    // Kani nga code mo-send og signal sa QEMU para mo-close ang window
    // ug mobalik ka sa imong terminal prompt.
    outw(0x604, 0x2000); 
    
    // Safety halt kon dili mo-work ang ACPI shutdown
    system_running = 0; 
}

/* --- 6. COMMAND ENGINE --- */

void process_command() {
    // 1. I-declare ang symbols sa taas
    extern uint32_t end; 
    uint32_t k_end = (uint32_t)&end;
    int p = 2;
    int n1;

    cursor_x = 0;
    cursor_y++;
    check_scroll();

    if (cmd_idx == 0) goto prompt;

    // --- 1. SYSTEM & FILE OPS ---
    if (str_compare(cmd_buffer, "HE")) {
        vga_print("CMDS: LS, LD, SV, RM, TO, RD, CL, RE, EX, WH, CO, DT, EN, DE, PC, ME, BI, AD, SU, MU, DI\n", 0x0F);
    }
    else if (str_compare(cmd_buffer, "LS")) {
        list_files();
    }
    else if (str_compare(cmd_buffer, "SV")) {
        vga_print("\n[DISK] Saving FS to Sector 10...", 0x0E);
        disk_write_sector(10, (uint8_t*)file_system);
        vga_print(" DONE.\n", 0x0A);
    }
    else if (str_compare(cmd_buffer, "LD")) {
        vga_print("\n[DISK] Loading FS from Sector 10...", 0x0B);
        disk_read_sector(10, (uint8_t*)file_system);
        vga_print(" DONE.\n", 0x0F);
    }
    else if (cmd_buffer[0] == 'R' && cmd_buffer[1] == 'M') {
        remove_file(&cmd_buffer[3]);
    }
    else if (cmd_buffer[0] == 'T' && cmd_buffer[1] == 'O') {
        create_file(&cmd_buffer[3]);
    }
    else if (cmd_buffer[0] == 'R' && cmd_buffer[1] == 'D') {
        int idx = find_file(&cmd_buffer[3]);
        if (idx != -1) {
            vga_print("\n--- CONTENT ---\n", 0x0E);
            vga_print(file_system[idx].data, 0x0F);
            vga_print("\n---------------\n", 0x0E);
        } else { vga_print("\nERR: NOT FOUND\n", 0x0C); }
    }

    // --- 2. SECURITY & UTILS ---
    else if (str_compare(cmd_buffer, "DT")) {
        display_date();
    }
    else if (str_compare(cmd_buffer, "WH")) {
        display_whoami();
    }
    else if (str_compare(cmd_buffer, "RE")) {
        reboot();
    }
    else if (str_compare(cmd_buffer, "EX")) {
        exit_system();
    }
    else if (str_compare(cmd_buffer, "CL")) {
        clear_screen(); draw_status_bar(); draw_toolbar(); display_banner();
    }
    else if (cmd_buffer[0] == 'C' && cmd_buffer[1] == 'O') {
        change_color(&cmd_buffer[3]);
    }
    else if (cmd_buffer[0] == 'E' && cmd_buffer[1] == 'N') {
        encrypt_text(&cmd_buffer[3]);
    }
    else if (cmd_buffer[0] == 'D' && cmd_buffer[1] == 'E') {
        decrypt_text(&cmd_buffer[3]);
    }
    else if (cmd_buffer[0] == 'P' && cmd_buffer[1] == 'C') {
        crack_password(&cmd_buffer[3]);
    }

    // --- 3. MATH & VISUALIZATION ---
    else if (cmd_buffer[0] == 'M' && cmd_buffer[1] == 'E') {
        vga_print("\n--- [ KERNEL MEMORY MONITOR ] ---\n", 0x0E);
        vga_print("KERNEL BASE : 0x00100000\n", 0x0F);
        vga_print("KERNEL END  : ", 0x0F);

        vga_print_hex_32(k_end); // Siguradong kaila ang compiler ani

        vga_print("\nSTATUS      : STABLE\n", 0x0A);
        vga_print("---------------------------------\n", 0x0E);
    }

    else {
        // I-reuse nato ang variable p ug n1
        p = 2;
        n1 = string_to_int(cmd_buffer, &p);

        if (cmd_buffer[0] == 'B' && cmd_buffer[1] == 'I') {
            vga_print("\nBIN: ", 0x0B); vga_print_binary((unsigned char)n1); vga_print("\n", 0x07);
        }
        else if (cmd_buffer[0] == 'A' && cmd_buffer[1] == 'D') {
            int n2 = string_to_int(cmd_buffer, &p);
            vga_print("\nRES: ", 0x0A); vga_print_int(n1 + n2); vga_print("\n", 0x07);
        }
        else if (cmd_buffer[0] == 'S' && cmd_buffer[1] == 'U') {
            int n2 = string_to_int(cmd_buffer, &p);
            vga_print("\nRES: ", 0x0C); vga_print_int(n1 - n2); vga_print("\n", 0x07);
        }
        else if (cmd_buffer[0] == 'M' && cmd_buffer[1] == 'U') {
            int n2 = string_to_int(cmd_buffer, &p);
            vga_print("\nRES: ", 0x0E); vga_print_int(n1 * n2); vga_print("\n", 0x07);
        }
        else if (cmd_buffer[0] == 'D' && cmd_buffer[1] == 'I') {
            int n2 = string_to_int(cmd_buffer, &p);
            if (n2 == 0) vga_print("\nERR: DIV/0", 0x04);
            else { vga_print("\nRES: ", 0x0B); vga_print_int(n1 / n2); }
            vga_print("\n", 0x07);
        }
        else {
            vga_print("\nWN: UNKNOWN CMD\n", 0x04);
        }
    }

prompt:
    cursor_x = 0;
    cursor_y++;
    check_scroll();
    vga_print("7shell >> ", 0x0B);
    cursor_x = 10;
    for(int i = 0; i < 64; i++) cmd_buffer[i] = 0;
    cmd_idx = 0;
    update_cursor(cursor_x, cursor_y);
} // <--- KANI NGA BRACKET ANG IMPORTANTE!

/* --- 7. SYSTEM FLOW --- */
void keyboard_handler() {
    if (!(inb(0x64) & 1)) return; 

    unsigned char scancode = inb(0x60);

    if (!(scancode & 0x80)) {
        char key = kbd_us[scancode];
        inactivity_timer = 0; 

        // 1. ENTER KEY
        if (key == '\n') {
            cmd_buffer[cmd_idx] = '\0'; 
            process_command();          
        }

        // 2. BACKSPACE
        else if (key == '\b') {
            if (cmd_idx > 0) {
                cmd_idx--;
                // Sigurohon nga dili mapapas ang "7shell >> "
                if (cursor_x > 10) { 
                    cursor_x--;
                    vga_put_char(cursor_x, cursor_y, ' ', 0x07);
                    update_cursor(cursor_x, cursor_y); // I-sync ang blinking line
                }
            }
        }

        // 3. REGULAR KEYS
        else if (key != 0 && cmd_idx < 63) {
            // --- SAFETY CHECK PARA SA "RUNNING RIGHT" BUG ---
            if (cursor_x < 79) { // Ayaw palapasa sa 80 characters
                cmd_buffer[cmd_idx++] = key;
                vga_put_char(cursor_x, cursor_y, key, 0x0F);
                cursor_x++; // I-advance ang cursor position
                update_cursor(cursor_x, cursor_y); // Importante para dili mag-ghosting
            }
        }
    }
}

void delay(int count) {
    for (volatile int i = 0; i < count * 10000; i++) {
        __asm__("nop");
    }
}

void boot_animation() {
    // 33 characters tanan apil ang spaces
    char* title = "S E V E N O S   U N L E A S H E D "; 
    unsigned char gold = 0x0E;
    unsigned char white = 0x0F;
    unsigned char gray = 0x08;
    unsigned char cyan = 0x0B;

    // Gi-adjust ang loop ngadto sa 35 para sigurado mahuman ang text
    for (int i = 0; i < 35; i++) {
        clear_screen();
        vga_print("\n\n\n\n", 0x07);

        // Dynamic Glow Logic
        unsigned char current_shine = (i % 5 == 0) ? white : gold;

        // Ang imong "True 7" Logo
        vga_print("                __________  \n", current_shine);
        vga_print("               |________  | \n", current_shine);
        vga_print("                       / /  \n", current_shine);
        vga_print("                      / /   \n", current_shine);
        vga_print("                     / /    \n", current_shine);
        vga_print("                    /_/     \n", current_shine);

        vga_print("\n\n      ", 0x07);

        // Smooth Text Reveal
        for (int j = 0; j <= i; j++) {
            if (title[j] == '\0') break; 
            char temp[2] = {title[j], '\0'};
            vga_print(temp, cyan);
        }

        // Animated Loading Bar
        vga_print("\n\n    ", 0x07);
        for (int bar = 0; bar < 40; bar++) {
            if (bar == i) vga_print(">", white); 
            else if (bar < i) vga_print("=", gold);
            else vga_print("-", gray);
        }

        // Mao ni ang imong "pacing" - 350-400 para chill ra
        delay(400); 
    }

    delay(1000); // Pahuway kadiyot aron ma-admire ang tibuok screen
    clear_screen();
}

/* --- SEVENOS v3.3 ALPINE EDITION --- */

// Kinahanglan ni para sa Alpine/Clang alignment
__attribute__((section(".text.start")))
void _start() {
    // Debug Signal: Letter 'A' (Alpine)
    // Kon makita nimo ang 'A', pasabot nakasulod na ang CPU sa imong C code.
    *((unsigned short*)0xB8000) = 0x0F41; 

    extern void kernel_main();
    kernel_main();

    // Infinite loop kon mogawas man gani sa kernel_main
    while(1) { __asm__ __volatile__("hlt"); }
}

void kernel_main() {
    // 1. VGA & INITIAL SCREEN SETUP
    clear_screen();

    // --- CRITICAL FIX: DILI NATO I-ZERO ANG FS ---
    // Imbes nga i-set og 0, atong i-load ang gikan sa Disk (Sector 10)
    vga_print("[SYSTEM] Loading File System from Disk...", 0x0E);
    load_all_files(); // Kani dapat ang mo-populate sa file_system array

    // Kon empty gyud ang disk (first time boot), dinhi ra nato i-initialize
    if (file_system[0].active != 1 && file_system[0].active != 0) {
        for (int i = 0; i < MAX_FILES; i++) file_system[i].active = 0;
        file_count = 0;
    }

    // 2. THE INTRO
    boot_animation();

    // 3. UI INITIAL LAYOUT
    clear_screen();
    draw_status_bar();
    draw_toolbar();

    cursor_y = 1;
    display_banner();

    // I-report kon naay na-restore nga files
    if (file_system[0].active == 1) {
        vga_print("\n[SYSTEM] Session Restored from Disk.\n", 0x0A);
    }

    vga_print("7shell >> ", 0x0B);
    update_cursor(10, cursor_y);

    unsigned long long time_counter = 0;
    const unsigned long long target_idle = 30000000;

    // 4. MAIN OPERATING LOOP
    while(1) {
        keyboard_handler();
        inactivity_timer++;
        time_counter++;

        // Matrix Screensaver
        if (inactivity_timer > target_idle) {
            inactivity_timer = 0;
            run_matrix();
        }

        // Clock Update (Kada 0.5 sec approx)
        if (time_counter > 500000) {
            display_time();
            time_counter = 0;
        }

        // --- CRITICAL UI FIX ---
        if (cursor_y >= 24) {
            check_scroll();
            draw_status_bar();
            draw_toolbar();
        }

        __asm__ __volatile__ ("pause");
    }
}
