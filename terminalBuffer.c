#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h> // aqui se trae la funcion para obtener el tamaño de la terminal
#include <sys/types.h>
#include <stdarg.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdbool.h>

typedef struct erow { // estructura para guardar una linea de texto entro de la terminal 
    int size;
    int rsize;
    char *chars;
    char *render; // render para tabs y espacios  no veo que sea necesario podriamos quitarlo all together
} erow;

struct editorConfig { // estructura para obtener el valor de la terminal para poder calcular rows 
  int cx, cy; // curso x y cursos y es la posicion del cursor dentro de la terminal para poder moverlo
  int rowoff; // current row position of the file
  int coloff; // current column offset
  int screenrows;
  int screencols;
  int numrows;
  erow *row; // dynamic array of rows of the file
  char statusmsg[80]; // mensajes para que despliegue el usuario
  char *command; // comandos desplegados por el usuario
  time_t statusmsg_time;
  struct termios orig_termios;
};
struct abuf { // estructura de un buffer, para tener strings dinamicos
  char *b;
  int len;
};

char fileName[20];
bool rawMode = 0;

enum editorKey { // enum para mappear las teclas con formato WASD
  BACKSPACE = 127,
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


void editorUpdateRow(erow *row) { // esta funcion toma los renders del renglon para rehacer el renglanon en la terminal y mostrarlo de manera correcta si es que tiene tabs etc
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++; // va eliminando tabs
  free(row->render);
  row->render = malloc(row->size + tabs*7 + 1);

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % 8 != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx; // idx tiene el numero de caracteres copiados a render 
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows) return;
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1)); // se realloca memoria, haciendo crecer a e.row con la nueva linea se usa el espacio de erow
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));  // make room at specified index

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1); // se prepara el espacio para el renglon
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]); // updatea el renglon
  E.numrows++;
}

void editorFreeRow(erow *row) {
  free(row->render); // libera espacio 
  free(row->chars); // libera chars 
}

void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(&E.row[at]);  //free the memory owned by the row 
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1)); // overwrite deleted row
  E.numrows--; 
}

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1; // length of each row o text 
  *buflen = totlen;
  char *buf = malloc(totlen); // allocate required memory 
  char *p = buf; // p apunta a buf  y se modifica en el loop 
  for (j = 0; j < E.numrows; j++) { // loop through the rows
    memcpy(p, E.row[j].chars, E.row[j].size); // memcopy the contents of each row to the end of the buffer appending new lines as weell 
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
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
    editorInsertRow(E.numrows,line, linelen); // aqui se agrega un renglon leido
  }
  free(line);
  fclose(fp);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
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

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1); // como memcpy pero salva que el source y destination arrays sean el mismo
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1); // reallocate 
  memcpy(&row->chars[row->size], s, len); // copy memory
  row->size += len; // se añade el length 
  row->chars[row->size] = '\0'; // el endline
  editorUpdateRow(row);
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) return; // valida que sea una posicion valida
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at); // se mueve un bit para atras "borrando el char"
  row->size--;
  editorUpdateRow(row);
}


void editorInsertChar(int c) {
  if (E.cy == E.numrows) { // cursor esta en el final 
    editorInsertRow(E.numrows,"", 0); // se agrega renglon nuevo 
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorInsertNewline() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx); // la columna se mantiene 
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row); // la row actual se updatea con espacio vacio
  }
  E.cy++;
  E.cx = 0;
}
void editorDelChar() {
  if (E.cy == E.numrows) return; // si el cursor esta fuera del tamaño regresa
  if (E.cx == 0 && E.cy == 0) return; // si es el inicio de un renglon
  erow *row = &E.row[E.cy]; // se trae el row actual
  if (E.cx > 0) { // si hay un caracter a la izq se borra sino, no 
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.row[E.cy -1].size;
    editorRowAppendString(&E.row[E.cy -1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
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
       int len = E.row[filerow].rsize - E.coloff;  // se usa rsize por el render de cada row y evitar malformaciones
       if (len < 0) len = 0;
       if (len > E.screencols) len = E.screencols; 
       abAppend(ab, &E.row[filerow].render[E.coloff], len); // se agregan los erows al buffer actual usando render para que este clean
   }

    abAppend(ab, "\x1b[K", 3);  // escape sequence [K  erases part of the current line, 2 toda la linea, 1 a la izq del cursor, y 0 a la derecha , 0 es el default, y se dan 3 bytes
    abAppend(ab, "\r\n", 2);
    
  }
}


void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);  // escape sequence [K clears message bar ]
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols; // se corta el mensaje para que quedpa en todo el width de la pantalla
  if (msglen && time(NULL) - E.statusmsg_time < 5) // message time solo dura 5 segundos por el momento la pantalla se refreseca despues de cada keypress
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() { // limpia la pantalla
  editorScroll();
  struct abuf ab = ABUF_INIT; // inicializo el bugger ABUF_INIT es funcion macro que pone al apuntador viendo a null y el lenght en 0
                               //abAppend(&ab, "\x1b[2J", 4); el 4 significa 5 bytoues of of terminal \x1b es el escpaee character  escape sequences always start with this and are followe by [ characters 
  abAppend(&ab, "\x1b[H", 3);  // estas funciones [ dicen  a la terminal como mover el keyboard,mouse etc. el comando J  limpia la pantalla despues de /x1b el 2 es un param que 
  editorDrawRows(&ab);         // le dice sea toda la pantalla, si usas 1 envez de 2 seria hasta donde esta el cursos
  editorDrawMessageBar(&ab);    // aqui estan todas las escape sequences http://ascii-table.com/ansi-escape-sequences-vt-100.php
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

char *editorPrompt(char *prompt) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize); // buf is dynamically allocated by bufsize
  size_t buflen = 0;
  buf[0] = '\0';
  while (1) { // loop infinito que pone el mensaje , refresca la pantalla y espera por keypress 
              // prompt will be a format 
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();
    int c = editorReadKey();
    if (c == DEL_KEY  || c == BACKSPACE) {
      if (buflen != 0) buf[--buflen] = '\0';// adding backspace, delete  to editor Prompt
    } else if (c == '\x1b') { // escape key is pressed, to exit the editorPrompt
      editorSetStatusMessage("");
      free(buf);
      return NULL;
    } else if (c == '\r') { // si presionan enter y el input no esta vacio , el input es regresado sino se añade al bug
      if (buflen != 0) {
        editorSetStatusMessage("");
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) { // si el tamaño llega a max se multiplica x 2 del buffer
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
  }
}

char* replaceWord(const char* oldW, const char* newW) 
{ 
    char* result; 
    int i, cnt = 0; 
    int newWlen = strlen(newW); 
    int oldWlen = strlen(oldW); 
  
    // Counting the number of times old word 
    // occur in the string 
    for (i = 0; s[i] != '\0'; i++) { 
        if (strstr(&s[i], oldW) == &s[i]) { 
            cnt++; 
  
            // Jumping to index after the old word. 
            i += oldWlen - 1; 
        } 
    } 
  
    // Making new string of enough length 
    result = (char*)malloc(i + cnt * (newWlen - oldWlen) + 1); 
  
    i = 0; 
    while (*s) { 
        // compare the substring with the result 
        if (strstr(s, oldW) == s) { 
            strcpy(&result[i], newW); 
            i += newWlen; 
            s += oldWlen; 
        } 
        else
            result[i++] = *s++; 
    } 
  
    result[i] = '\0'; 
    return result; 
} 


void numberOcurrences(const char* findtext){
  unsigned counter = 0;

  if(findtext == NULL){
    return;
  }
  
  for (int rows = 0; rows < E.numrows; rows++){
    erow *actualRow = &E.row[rows];
    const char *p = actualRow->chars;
    while( (p = strstr(p,findtext)) != NULL){
      p += strlen(findtext);
      counter++;
    }
  }
  printf("%s aparece: %u veces", findtext, counter);
  editorSetStatusMessage("%s aparece: %u veces", findtext, counter);
}

void readCommand(){
  free(E.command);
  E.command = editorPrompt("Escribe el comando %s");
  char *findText = E.command;
  char delim[] = " ";
  editorSetStatusMessage("%s comando escrito", E.command);
  switch(E.command[0]){
    case ':':
        if(E.command[1] == 'q') {
          clearScreen();
          exit(0);
        }else if(E.command[1] == 'e'){
          rawMode = 1; 
        }else if(E.command[1] == 'f'){
          strcpy(E.command,findText);
          memmove(findText,findText+3,strlen(findText));
          numberOcurrences(findText);
        }else if(E.command[1] == 'r'){
          strcpy(E.command,findText);
          memmove(findText,findText+3,strlen(findText));
          char *ptr = strtok(findText,delim);
          char *toReplace, *replacement;
          toReplace = ptr;
          ptr = strtok(NULL,delim);
          replacement = ptr;
          replaceText(toReplace,replaceText);
        }
        break;
  }
}
void editorSave() {
  int len;
  char *buf = editorRowsToString(&len); // se llama editors rows to strings para tener el buffer completo de la info nueva
  int fd = open(fileName, O_RDWR | O_CREAT, 0644);  // se abre o se crea un archivo nuevo en caso de que no exista
  if( fd != -1) {
   if( ftruncate(fd, len) != 1){ // resizes the file size to specific length
    if(write(fd, buf, len) == len){ // se escribe el buffer
    close(fd); // se cierra la file
    free(buf); 
    editorSetStatusMessage("%d bytes guardados", len); // mensaje de que si se guardo , se puede quitar
    return; // return 
    }
  }
  close(fd); // cierra archivo
}
   free(buf);
   editorSetStatusMessage("No se puede guardar! I/O error: %s", strerror(errno)); // mensaje de error
}
void editorProcessKeypress() { // handles keypress 
  int c = editorReadKey();
  switch (c) {
    case '\r':  // enter key 
      editorInsertNewline();
      break;
    case CTRL_KEY('q'): // si es ctl + q se cierra la pantalla 
       rawMode = 0;
      break;
    case CTRL_KEY('s'): // maps ctl + s para que le de save
      editorSave();
      break;
    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
      case BACKSPACE:
      case DEL_KEY:
        if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
           break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;
    case '\x1b':// escape 
       break;
    default: 
      editorInsertChar(c);
      break;
  }
}
void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.command = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize"); // llamo get windows size al iniciar el proyecto , para la struct de la terminal
  E.screenrows -=1;
}

int main (int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    char *temp = argv[1];
    strcpy(fileName,temp);
    if(argc >= 2) editorOpen(argv[1]);
    editorSetStatusMessage("HELP: Ctrl-Q = quit | Ctrl-S = save");
    while (1) {
        editorRefreshScreen();
        if(!rawMode) {
        readCommand("Escriba su comando ");
        // Funcion para leer comando 
       }else
        editorProcessKeypress(); // esto es cuando esta en rawmode
    }
    return 0;
}