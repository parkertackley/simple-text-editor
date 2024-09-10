#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

/* define */
#define CONCRETE_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

struct editorConfig {
    int cx, cy;
    int screenRows;
    int screenCols;
    struct termios orig_termios;
};

struct editorConfig E;

/* terminal settings */

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

// save original terminal attributes to restore at exit
void disableRawMode() {
    if(tcsetattr(STDERR_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

// disables echoing, canonical mode, control signals (ctrl+c, etc.), software control (ctrl+s, etc.)
// echoing - displays what is being typed, canonical - reads line by line rather than bytes by byte
// terminal enters raw mode
void enableRawMode() {
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); /* break condition(ctrl+c) | ctrl+m | parity checking | 8-bit set to 0 turned off | ctrl+s, ctrl+q*/
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag &= ~(CS8); /*char size to 8 bits*/
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); /*echo | canon mode | ... | control signals */
    raw.c_cc[VMIN] = 0; /*anything over 1 byte gets read*/
    raw.c_cc[VTIME] = 1; /*read times out after 1/10 second*/

    if(tcsetattr(STDERR_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");

}

int editorReadKey() {
    int nread;
    char c;

    while((nread = read(STDERR_FILENO, &c, 1) != 1)) {
        if(nread == -1 && errno != EAGAIN) die("read");
    }

    return c;

}

int getwindowSize(int *rows, int *cols) {

    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }

}

/* append buffer */

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if(new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/* output */

void editorDrawRows(struct abuf *ab) {
    for(int y = 0; y < E.screenRows; y++) {
        if( y == E.screenRows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), "Concrete Editor -- verzion %s", CONCRETE_VERSION);
            if(welcomelen > E.screenCols) welcomelen = E.screenCols;

            int padding = (E.screenCols - welcomelen) / 2;
            if(padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while(padding--) abAppend(ab, " ", 1);

            abAppend(ab, welcome, welcomelen);
        } else {
            abAppend(ab, "~", 1);
        }

        abAppend(ab, "\x1b[K", 3);
        if(y < E.screenRows - 1) abAppend(ab, "\r\n", 2);

    }
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
    
}

/* input */

void editorMoveCursor(char key) {
    switch(key) {
        case 'w':
            E.cy--;
            break;
        case 'a':
            E.cx--;
            break;
        case 's':
            E.cy++;
            break;
        case 'd':
            E.cx++;
            break;
    }
}

void editorProcessInput() {
    char c = editorReadKey();

    switch(c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        
        case 'w':
        case 'a':
        case 's':
        case 'd':
            editorMoveCursor(c);
            break;
    }

}

/* init */

void initEditor() {
    E.cx = 0;
    E.cy = 0;

    if(getwindowSize(&E.screenRows, &E.screenCols) == -1) die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();

    while(1) {
        editorRefreshScreen();
        editorProcessInput();
    }

    return 0;

}
