#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/* define */
#define CTRL_KEY(k) ((k) & 0x1f)

struct editorConfig {
    struct termios orig_termios;
};

struct editorConfig E;

/* terminal settings */

// when tcsetattr, tcgetattr, read() return -1, call die
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

/* output */

void editorDrawRows() {
    for(int y = 0; y < 24; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void editorRefreshScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);
    
}

/* input */

void editorProcessInput() {
    char c = editorReadKey();

    switch(c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }

}

/* init */

int main() {
    enableRawMode();

    while(1) {
        editorRefreshScreen();
        editorProcessInput();
    }

    return 0;

}