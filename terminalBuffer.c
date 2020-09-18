#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h> // aqui se trae la funcion para obtener el tamaño de la terminal
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

typedef struct erow { // estructura para guardar una linea de texto entro de la terminal 
    int size;
    char *chars;
} erow;

struct editorConfig { // estructura para obtener el valor de la terminal para poder calcular rows 
  int cx, cy; // curso x y cursos y es la posicion del cursor dentro de la terminal para poder moverlo
  int rowoff; // current row position of the file
  int coloff; // current column offset
  int screenrows;
  int screencols;
  int numrows;
  erow *row; // dynamic array of rows of the file
  struct termios orig_termios;
};
struct abuf { // estructura de un buffer, para tener strings dinamicos
  char *b;
  int len;
};



enum editorKey { // enum para mappear las teclas con formato WASD
  ARROW_LEFT = 1000, // se le da este valor para que las teclas no interfieran con los valores de ascci normales 
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  PAGE_UP,
  PAGE_DOWN,
  DEL_KEY 
};

struct editorConfig E;

#define CTRL_KEY(k) ((k) & 0x1f) // se define la funcion macro de CTRL_KEY para obtener valores de clt q, ctl m y keyboard
#define ABUF_INIT {NULL, 0} // inicializar el buffer con apuntador nulo y tamaño de 0

void abAppend(struct abuf *ab, const char *s, int len) { // append al buffer
  char *new = realloc(ab->b, ab->len + len); // se alloca memoria para la nueva string 
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len); // se copia la stiring s despues de la info de buffer b
  ab->b = new;
  ab->len += len;
}
void abFree(struct abuf *ab) { // liberar el apuntador y la string del buffer
  free(ab->b);
}

void clearScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void die(const char *s) {
  clearScreen();

  perror(s);
  exit(1);
}

void disableRawMode() {
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");
}
/* Just press Ctrl-C to start a fresh line of input to your shell, and type in reset and press Enter.*/
void enableRawMode() {
  if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode); // comes from stdlib.h
  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(ICRNL |IXON | INPCK | ISTRIP | BRKINT); // c_iflag = input flag  BRKINT, INPCK, ISTRIP no son necesarios son por costumbre
  raw.c_oflag &= ~(OPOST); // c_oflag = output flags 
  raw.c_cflag |= (CS8); // solo por rutina no es necesario realmente 
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);  
  raw.c_cc[VMIN] = 0; // c_cc control characters
  raw.c_cc[VTIME] = 1;
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}
// ISIG para ctl c / ctl z 
// ICANON  read byte 4 byte
// ECHO  no repetir el input
// IXON ctls s ctlq xoff ( pause trasmision (ctl s) y xon resume transmission (ctl q)))
// IEXTEN DISABLES CTL V AND CTL C 
// ICRNL  disabels CTL M so it takes value of 13 instead of 10 now enter and ctl m are 13
// OPOST new lines and new r  /r/n saldria necesitamos solo /n  hace que para escribir uan nueva linea se ponga \r\n

// VMIN sets minmun number of bytes of inpute needed before read happens
// vtime sets maximum amount of time to wait before read it is tenths of a second 


// Escape Sequences for Keys 
// \xb1[A -> up 
// \xb1[B -> down 
// \xb1[C -> right 
// \xb1[D -> lefft 
// \xb1[5~ -> PAGE UP 
// \xb1[6~ -> PAGE DOWN  
// \xb1[3~ -> DELETE

int editorReadKey() { // wait for one keypress and return it
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1) die("read");
  }
  if (c == '\x1b') { // si el input es  \x1b significa que es un escape sequence puede ser un arrow , mouse, etc
    char seq[3]; // se leen trees bytes mas del buffer podemos aplicar algo parecido para leer :q, :qw, :f etc.
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[') { // si el segundo byte es [ sigue siendo escape sequence 
       if(seq[1] >= '0' && seq[1] <= '9'){ // en caso de que el escape secuence entre dentro del rango eqeu bsucamos
           if (seq[2] == '~') { 
               switch(seq[1]){
                   case '3': return DEL_KEY;
                   case '5': return PAGE_UP;
                   case '6': return PAGE_DOWN;
               }
           }
        } else {
            switch (seq[1]) { // en caso de que haya sido un valor de keypress de las escape sequences con el segudno byte
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
            }
       }
    }
    return '\x1b';
  } else {
  return c;
  }
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;  // winsize, ioctl y el cons TIOCGWINSZ (terminal input output window size) vienend e la lib sys/ioctl.h
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) { // function ioctl brings the windowsize aqui checamos que no truene con el == -1 y que sea mayor a 0
    return -1;
  } else { // por referencia asignaos el tamaño de las columnas y renglones
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

void editorAppendRow(char *s, size_t len) {
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1)); // se realloca memoria, haciendo crecer a e.row con la nueva linea se usa el espacio de erow
  
  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1); // se prepara el espacio para el renglon
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}

void editorOpen(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  char *line = NULL;
  size_t linecap = 0; // se inicializa en 0 para que la funcion getline te de el valor del tamaño de la linea
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) { // mientras se siga podiendo leer unea linea del archivo correra esta parte
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen); // aqui se agrega un renglon leido
  }
  free(line);
  fclose(fp);
}



void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key) { // un switch para mover el cursor dependiendo  de WASD
    case ARROW_LEFT:
      if(E.cx != 0)
      E.cx--;
      else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size)
      E.cx++;
      else if (row && E.cx == row->size) {
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if(E.cy != 0)
      E.cy--;
      break;
    case ARROW_DOWN:
      if(E.cy < E.numrows) // no puedes bajar mas de los renglones
      E.cy++;
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) { // en caso de que al bajar el renglon este sea mas corto al actual
    E.cx = rowlen;
  }
}

void editorProcessKeypress() { // handles keypress 
  int c = editorReadKey();
  switch (c) {
    case CTRL_KEY('q'): // si es ctl + q se cierra la pantalla 
      clearScreen();
      exit(0);
      break;
    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;
  }
}

// function to scroll and know in which row we are 
void editorScroll() {
  if (E.cy < E.rowoff) { // se basa en la posicion actual de y en la terminal  y del rowoff para calcularlo
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) { // si esta "out of bounds"
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.cx < E.coloff) { // calcula posicion de columna
    E.coloff = E.cx;
  }
  if (E.cx >= E.coloff + E.screencols) { // si esta out of bounds en x
    E.coloff = E.cx - E.screencols + 1;
  }
}

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if( y >= E.numrows){
    if (y == E.screenrows / 2 && E.numrows == 0) { // cuando se encuentra  la mitad de la terminal
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
        "Bienvenidos al Editor");
      if (welcomelen > E.screencols) welcomelen = E.screencols;
      int padding = (E.screencols - welcomelen) / 2; // solo se calcula el padding , de rows para que el mensaje este en el centro
      if (padding) {
        abAppend(ab, "*", 1); // cuando es zero (mera izq de la terminal)
        padding--;
      }
      while (padding--) abAppend(ab, " ", 1);
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "*", 1);
    }
   } else {
       int len = E.row[filerow].size - E.coloff; 
       if (len < 0) len = 0;
       if (len > E.screencols) len = E.screencols; 
       abAppend(ab, &E.row[filerow].chars[E.coloff], len); // se agregan los erows al buffer actual
   }

    abAppend(ab, "\x1b[K", 3);  // escape sequence [K  erases part of the current line, 2 toda la linea, 1 a la izq del cursor, y 0 a la derecha , 0 es el default, y se dan 3 bytes
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() { // limpia la pantalla
  editorScroll();
  struct abuf ab = ABUF_INIT; // inicializo el bugger ABUF_INIT es funcion macro que pone al apuntador viendo a null y el lenght en 0
                               //abAppend(&ab, "\x1b[2J", 4); el 4 significa 5 bytoues of of terminal \x1b es el escpaee character  escape sequences always start with this and are followe by [ characters 
  abAppend(&ab, "\x1b[H", 3);  // estas funciones [ dicen  a la terminal como mover el keyboard,mouse etc. el comando J  limpia la pantalla despues de /x1b el 2 es un param que 
  editorDrawRows(&ab);                    // le dice sea toda la pantalla, si usas 1 envez de 2 seria hasta donde esta el cursos
                                      // aqui estan todas las escape sequences http://ascii-table.com/ansi-escape-sequences-vt-100.php
                                      // user guide -> https://vt100.net/docs/vt100-ug/chapter3.html
                                      // AQUi el [H,3 mueve el cursor al inicio del aterminal 
  char buf[32];                               // se resta E.rowoff para tener la posicion relativa tanto a la terminal como al archivo
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.cx - E.coloff) + 1); // se usa escape sequence [H  la cual mueve el cursos, usando 2 variables %d y %d que serian E.cy y E.cx ambas + 1
  abAppend(&ab, buf, strlen(buf));
  write(STDOUT_FILENO, ab.b , ab.len);  
  abFree(&ab);
}
// Escape routes 
// [H , 3 bytes  command actually takes two arguments: the row number and the column number at which to position the cursor argumentos se separan con ; [12;20H] row 12 columna 20 



void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize"); // llamo get windows size al iniciar el proyecto , para la struct de la terminal
}

int main (int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if(argc >= 2) editorOpen(argv[1]);
    //char c; 
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}