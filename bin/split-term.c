/**
 * splitvterm - A program that splits the terminal into two virtual terminals
 * 
 * This program uses libvterm to create two independent terminal instances
 * and manages their input/output, similar to splitvt.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>  /* For waitpid() */
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <pty.h>
#include <vterm.h>
#include <ncurses.h>

#define MAX_TERMINALS 2

/* Define UTF8_MAX_LENGTH if not provided by libvterm headers */
#ifndef UTF8_MAX_LENGTH
#define UTF8_MAX_LENGTH 6  /* Maximum bytes in a UTF-8 encoded character */
#endif

typedef struct {
    VTerm *vt;
    VTermScreen *vts;
    pid_t child_pid;
    int pty_fd;
    WINDOW *win;
    int active;
    int row_count;
    int col_count;
    int start_row;
} Terminal;

// Flag to ignore the next Ctrl+A after a toggle
int ignore_next_ctrl_a = 0;

// Global variables
Terminal terminals[MAX_TERMINALS];
int active_terminal = 0;
int should_exit = 0;
struct termios orig_termios;
int log_file = -1;

// Function prototypes
void init_terminals(void);
void cleanup(void);
void handle_signals(void);
void handle_input(void);
void handle_output(void);
void toggle_active_terminal(void);
void vterm_output_callback(const char *s, size_t len, void *user);
void handle_screen_callbacks(Terminal *term);

void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    endwin();
}

void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_TERMINALS; i++) {
            if (terminals[i].child_pid == pid) {
                printf("Child process %d terminated\n", pid);
                should_exit = 1;
                break;
            }
        }
    }
}

void sigwinch_handler(int sig) {
    struct winsize ws;
    
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("ioctl TIOCGWINSZ");
        return;
    }

    // Resize ncurses windows
    resizeterm(ws.ws_row, ws.ws_col);
    
    // Update terminal dimensions
    terminals[0].row_count = ws.ws_row / 2;
    terminals[1].row_count = ws.ws_row - terminals[0].row_count;
    terminals[0].col_count = ws.ws_col;
    terminals[1].col_count = ws.ws_col;
    terminals[1].start_row = terminals[0].row_count;
    
    // Resize windows
    wresize(terminals[0].win, terminals[0].row_count, terminals[0].col_count);
    wresize(terminals[1].win, terminals[1].row_count, terminals[1].col_count);
    mvwin(terminals[1].win, terminals[1].start_row, 0);
    
    // Resize VTerms
    for (int i = 0; i < MAX_TERMINALS; i++) {
        vterm_set_size(terminals[i].vt, terminals[i].row_count, terminals[i].col_count);
        
        // Update the PTY window size
        ws.ws_row = terminals[i].row_count;
        ws.ws_col = terminals[i].col_count;
        if (ioctl(terminals[i].pty_fd, TIOCSWINSZ, &ws) == -1) {
            perror("ioctl TIOCSWINSZ");
        }
    }
    
    // Force complete redraw
    clearok(curscr, TRUE);
    refresh();
    for (int i = 0; i < MAX_TERMINALS; i++) {
        wrefresh(terminals[i].win);
    }
}

void vterm_output_callback(const char *s, size_t len, void *user) {
    Terminal *term = (Terminal *)user;
    write(term->pty_fd, s, len);
}

int screen_damage(VTermRect rect, void *user) {
    Terminal *term = (Terminal *)user;
    VTermScreen *screen = term->vts;
    VTermScreenCell cell;
    
    // Redraw the damaged area
    for (int row = rect.start_row; row < rect.end_row; row++) {
        wmove(term->win, row, rect.start_col);
        
        for (int col = rect.start_col; col < rect.end_col; col++) {
            vterm_screen_get_cell(screen, (VTermPos){.row = row, .col = col}, &cell);
            
            // Set attributes
            if (cell.attrs.bold)
                wattron(term->win, A_BOLD);
            if (cell.attrs.underline)
                wattron(term->win, A_UNDERLINE);
            if (cell.attrs.reverse)
                wattron(term->win, A_REVERSE);
            
            // Add the character to the window
            if (cell.chars[0]) {
                // Manual UTF-8 encoding for Unicode codepoints
                uint32_t c = cell.chars[0];
                char buffer[UTF8_MAX_LENGTH + 1] = {0};
                
                if (c < 0x80) {
                    // 1-byte UTF-8
                    buffer[0] = c;
                    buffer[1] = '\0';
                } else if (c < 0x800) {
                    // 2-byte UTF-8
                    buffer[0] = 0xC0 | (c >> 6);
                    buffer[1] = 0x80 | (c & 0x3F);
                    buffer[2] = '\0';
                } else if (c < 0x10000) {
                    // 3-byte UTF-8
                    buffer[0] = 0xE0 | (c >> 12);
                    buffer[1] = 0x80 | ((c >> 6) & 0x3F);
                    buffer[2] = 0x80 | (c & 0x3F);
                    buffer[3] = '\0';
                } else if (c < 0x110000) {
                    // 4-byte UTF-8
                    buffer[0] = 0xF0 | (c >> 18);
                    buffer[1] = 0x80 | ((c >> 12) & 0x3F);
                    buffer[2] = 0x80 | ((c >> 6) & 0x3F);
                    buffer[3] = 0x80 | (c & 0x3F);
                    buffer[4] = '\0';
                } else {
                    // Invalid Unicode, use replacement character
                    buffer[0] = '?';
                    buffer[1] = '\0';
                }
                
                waddstr(term->win, buffer);
            } else {
                waddch(term->win, ' ');
            }
            
            // Reset attributes
            if (cell.attrs.bold)
                wattroff(term->win, A_BOLD);
            if (cell.attrs.underline)
                wattroff(term->win, A_UNDERLINE);
            if (cell.attrs.reverse)
                wattroff(term->win, A_REVERSE);
        }
    }
    
    // Draw active terminal indicator
    if (term->active) {
        mvwprintw(term->win, 0, term->col_count - 8, "[ACTIVE]");
    } else {
        mvwprintw(term->win, 0, term->col_count - 8, "        ");
    }
    
    wrefresh(term->win);
    return 1;
}

int screen_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user) {
    Terminal *term = (Terminal *)user;
    wmove(term->win, pos.row, pos.col);
    wrefresh(term->win);
    return 1;
}

int screen_bell(void *user) {
    // Flash the screen or beep
    flash();
    return 1;
}

VTermScreenCallbacks screen_callbacks = {
    .damage = screen_damage,
    .movecursor = screen_movecursor,
    .bell = screen_bell,
    .resize = NULL, // We handle resize separately
    .sb_pushline = NULL, // Not using scrollback functionality
    .sb_popline = NULL,  // Not using scrollback functionality
};

void init_terminal(int index, int rows, int cols, int start_row) {
    Terminal *term = &terminals[index];
    
    // Create VTerm
    term->vt = vterm_new(rows, cols);
    term->vts = vterm_obtain_screen(term->vt);
    
    // Set up callbacks
    vterm_screen_set_callbacks(term->vts, &screen_callbacks, term);
    vterm_screen_reset(term->vts, 1);
    vterm_output_set_callback(term->vt, vterm_output_callback, term);
    
    // Create window
    term->win = newwin(rows, cols, start_row, 0);
    scrollok(term->win, TRUE);
    
    // Initialize properties
    term->row_count = rows;
    term->col_count = cols;
    term->start_row = start_row;
    term->active = (index == 0) ? 1 : 0;
    
    // Create PTY
    struct winsize ws = { .ws_row = rows, .ws_col = cols };
    term->child_pid = forkpty(&term->pty_fd, NULL, NULL, &ws);
    
    if (term->child_pid == -1) {
        perror("forkpty");
        exit(EXIT_FAILURE);
    }
    
    if (term->child_pid == 0) {
        // Child process
        char *shell = getenv("SHELL");
        if (!shell) shell = "/bin/sh";
        
        char *args[] = { shell, NULL };
        execvp(shell, args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    
    // Set PTY to non-blocking mode
    int flags = fcntl(term->pty_fd, F_GETFL, 0);
    fcntl(term->pty_fd, F_SETFL, flags | O_NONBLOCK);
}

void init_terminals(void) {
    // Get terminal size
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("ioctl TIOCGWINSZ");
        exit(EXIT_FAILURE);
    }
    
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    raw();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    
    // Save original terminal settings
    tcgetattr(STDIN_FILENO, &orig_termios);
    
    // Set up signal handlers
    signal(SIGCHLD, sigchld_handler);
    signal(SIGWINCH, sigwinch_handler);
    
    // Register cleanup function
    atexit(restore_terminal);
    
    // Open log file
    log_file = open("splitvterm.bin", O_WRONLY|O_TRUNC|O_CREAT,0);
    if (!log_file) {
        perror("Failed to open log file");
    }
    
    // Determine terminal split
    int top_rows = ws.ws_row / 2;
    int bottom_rows = ws.ws_row - top_rows;
    
    // Initialize both terminals
    init_terminal(0, top_rows, ws.ws_col, 0);
    init_terminal(1, bottom_rows, ws.ws_col, top_rows);
    
    // Draw initial border
    mvhline(top_rows - 1, 0, ACS_HLINE, ws.ws_col);
    refresh();
}

void toggle_active_terminal(void) {
    terminals[active_terminal].active = 0;
    active_terminal = (active_terminal + 1) % MAX_TERMINALS;
    terminals[active_terminal].active = 1;
    
    // Redraw terminal indicators
    for (int i = 0; i < MAX_TERMINALS; i++) {
        if (terminals[i].active) {
            mvwprintw(terminals[i].win, 0, terminals[i].col_count - 8, "[ACTIVE]");
        } else {
            mvwprintw(terminals[i].win, 0, terminals[i].col_count - 8, "        ");
        }
        wrefresh(terminals[i].win);
    }
    
    // Set flag to ignore the next Ctrl+A
    ignore_next_ctrl_a = 1;
}

void log_key(int ch, int terminal_id) {
  uint8_t key_code = (uint8_t)ch;
  write(log_file,&key_code,1);
}

void handle_input(void) {
    int ch = getch();
    
    if (ch == ERR) {
        return;  // No input available
    }
    
    // Log input before processing
    log_key(ch, active_terminal);
    
    // Check for control key to switch terminals (Ctrl+A)
    if (ch == 1) {  // Ctrl+A
        if (ignore_next_ctrl_a) {
            // Ignore this Ctrl+A as it's immediately after a toggle
            ignore_next_ctrl_a = 0;
        } else {
            ch = getch();
            if (ch == ERR || ch == 1) {  // Timeout or another Ctrl+A
                if (ch == 1) {
                    log_key(ch, active_terminal);  // Log the second Ctrl+A
                }
                toggle_active_terminal();
                return;
            }
            // Log the character after Ctrl+A
            log_key(ch, active_terminal);
        }
    } else {
        // Any key that's not Ctrl+A clears the ignore flag
        ignore_next_ctrl_a = 0;
    }
    
    // Send the character to the active terminal
    char buffer[8] = {0};
    size_t len = 1;
    buffer[0] = ch;
    
    vterm_input_write(terminals[active_terminal].vt, buffer, len);
}

void handle_output(void) {
    struct pollfd fds[MAX_TERMINALS];
    char buffer[1024];
    ssize_t bytes_read;
    
    // Set up poll structures
    for (int i = 0; i < MAX_TERMINALS; i++) {
        fds[i].fd = terminals[i].pty_fd;
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }
    
    // Check for data from terminals with a very short timeout
    if (poll(fds, MAX_TERMINALS, 0) > 0) {
        for (int i = 0; i < MAX_TERMINALS; i++) {
            if (fds[i].revents & POLLIN) {
                bytes_read = read(terminals[i].pty_fd, buffer, sizeof(buffer));
                
                if (bytes_read > 0) {
                    vterm_input_write(terminals[i].vt, buffer, bytes_read);
                } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("read from pty");
                }
            }
        }
    }
}

void cleanup(void) {
    // Terminate child processes
    for (int i = 0; i < MAX_TERMINALS; i++) {
        if (terminals[i].child_pid > 0) {
            kill(terminals[i].child_pid, SIGTERM);
        }
        
        if (terminals[i].pty_fd >= 0) {
            close(terminals[i].pty_fd);
        }
        
        if (terminals[i].vt) {
            vterm_free(terminals[i].vt);
        }
    }
    
    close(log_file);
}

int main(void) {
    init_terminals();
    
    // Main loop
    while (!should_exit) {
        handle_input();
        handle_output();
        usleep(10000); // 10ms sleep to reduce CPU usage
    }
    
    cleanup();
    return 0;
}
