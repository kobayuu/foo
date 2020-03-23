/* header 
   step 76 https://viewsourcecode.org/snaptoken/kilo/04.aTextViewer.html#multiple-lines
*/
/*** includes***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
/** define o uso da tecla Control **/
#define CTRL_KEY(k) ((k) & 0x1f)
/** versão do app**/
#define FOO_VERSION "alpha_0.0.1"

/**define as teclas de movimento **/
enum editorKey {
		ARROW_LEFT = 1000,
		ARROW_RIGHT,
		ARROW_UP,
		ARROW_DOWN,
		DEL_KEY,
		HOME_KEY,
		END_KEY,
		PAGE_UP,
		PAGE_DOWN
};

/*** dados***/
/** struct que faz o No. das colunas  **/
typedef struct erow {
  int size;
  char *chars;
} erow;

struct editorConfig{
  int cx, cy;
  /* Let’s add a rowoff (row offset) variable to the global editor state, which will keep track of what row of the file the user is currently scrolled to. */
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/
/**fecha o buffer e da display de msg de erro
a função die tende a receber o valor 'tcsetattr' **/
void die(const char *s){
  /* <esc>[2J limpa a tela, \x1b[ (começo de uma operação) , 2J(argumetno) , 4(quantidade de bites que \xb1 puxa) */
  write(STDOUT_FILENO, "\x1b[2J", 4);
  /* manda a posição do cursor , nafalta de argumetos mandando 1 (ao inves do abitual o) tanto para linha quanto coluna(<esc>[12(row);40(collum)H)  */
  write(STDOUT_FILENO, "\x1b[H", 3);  

  perror(s);
  exit(1);
}
/** desabilita o modo de edição, voltando o terminal ao seu estado anterior **/
void disableRawMode(){
  /* 
     tcsetattr(int fd, int optional_actions,
              const struct termios *termios_p);
     tcsetattr (Terminal(C)SETATTRibutes - define parametros associados à sessão do terminal
     TCSAFLUSH - a mudança ocorre depois da saida no objeto referido em 'fd' & os dados serão discartados depois que a mudanã for aplicada
     
*/
  if (tcsetattr(STDIN_FILENO,TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}


/**modifica o estado do terminal, para habilitar i editor de texto**/
void enableRawMode(){
  /*
    tcgetattr(int fd, struct termios *termios_p);
    tcgetattr - pega os atibudos de fd e guarda em termios_p
  */
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  /* ao fechamento dessa função, chama disable raw mode */ 
  atexit(disableRawMode);
  /*
    aplica as seguintes flags a sessão de terminal, desabilitando hotkeys e afins expecíficos, <como C-a, C-c, C-v>

    tcflag_t c_iflag;      / input modes
    tcflag_t c_oflag;      / output modes 
    tcflag_t c_cflag;      / control modes 
    tcflag_t c_lflag;      / local modes 
    cc_t     c_cc[NCCS];   / special characters 
   */
  struct termios raw = E.orig_termios;
  /* (x &= y ) == (x = x &(and) y) */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP |IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON| IEXTEN  | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int editorReadKey(){
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1){
    /* testa se o systena está bricado, EAGAIN testa se é possivel re-fazer a chamada ou algo do tipo, o fato dele não poder ser chamado dizque o systema está bricado  */
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9'){
	if (read(STDOUT_FILENO, &seq[2], 1) != 1) return '\x1b';
	if (seq[2] == '>') {
	  switch (seq[1]){
	    case '1': return HOME_KEY;
	    case '3': return DEL_KEY;
	    case '4': return END_KEY;
	    case '5': return PAGE_UP;
	    case '6': return PAGE_DOWN;
	    case '7': return HOME_KEY;
	    case '8': return HOME_KEY;
	  }
	}
      } else {
      switch (seq[1]) {
	  case 'A': return ARROW_UP;
	  case 'B': return ARROW_DOWN;
	  case 'C': return ARROW_RIGHT;
	  case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
	}
      }
    } else if (seq[0] == '0') {
      switch (seq[1]) {
          case 'H':return HOME_KEY;
          case 'F': return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}
/** pega a posição do cursor **/
int getCursorPosition(int *rows, int *cols) {

  char buf[32];
  unsigned int i = 0;
  /* reporta a posição do cursor  */
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  /* pulamos o caractere de buf[] '<esc>' na hora de printar   */
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  /* printf espera que strings terminem com um byte 0, por isso damos assign ao ultimo byte de buf '\0' */
  buf[i] = '\0';
  /* pula os primeiros caracters de buf[] para pegar apenas os números de '\x1b[', pegando assim apenas o tamanho do buffer */
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;

}
/**pega o tamanho da janela**/
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  /* ioctl(), TIOCGWINSZ, and struct winsize come from <sys/ioctl.h>
     caso tenha sucesso, ioctl() pega o No. de linhas e colunas e passa  para a struct winsize(em falha passa -1)
 
     no caso do segundo if(extraido direto do site
     >>https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html):
     >We are sending two escape sequences one after the other. The C command (Cursor Forward) moves the cursor to the right, and the B command (Cursor Down) moves the cursor down. The argument says how much to move it right or down by. We use a very large value, 999, which should ensure that the cursor reaches the right and bottom edges of the screen.

The C and B commands are specifically documented to stop the cursor from going past the edge of the screen. The reason we don’t use the <esc>[999;999H command is that the documentation doesn’t specify what happens when you try to move the cursor off-screen.
  */
  if( ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** 'row operations'  ***/

void editorAppendRow ( char *s, size_t len) {
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  /** We have to tell realloc() how many bytes we want to allocate, so we multiply the number of bytes each erow takes (sizeof(erow)) and multiply that by the number of rows we want. Then we set at to the index of the new row we want to initialize, and replace each occurrence of E.row with E.row[at]. Lastly, we change E.numrows = 1 to E.numrows++. **/
  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars,s,len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}

/*** entrada e saida de arquivos ***/
/*
   editorOpen() will eventually be for opening and reading a file from disk, so we put it in a new *** file io *** section. To load our Hello, world message into the editor’s erow struct, we set the size field to the length of our message, malloc() the necessary memory, and memcpy() the message to the chars field which points to the memory we allocated. Finally, we set the E.numrows variable to 1, to indicate that the erow now contains a line that should be displayed. */
void editorOpen(char *filename){
  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");
  
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  linelen = getline(&line, &linecap, fp);
  while((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen -1] == '\n' ||
			   line[linelen -1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen);
}
  free(line);
  fclose(fp);
}
/*** append buffer ***/
/** um append buffer consiste em um ponteiro para nosso buffer na memoria **/

struct abuf {
  char *b;
  int len;
};
/**q consiste num buffer vazio **/ 
#define ABUF_INIT {NULL, 0}

/** para dar append da string s em 'abuf', é necessário que aloquemos momoria o bastante para comportar a nova string. usa-se realloc() para nos dar um bloco de memória do tamanho da string atual + o tamanho do append **/
void abAppend(struct abuf *ab, const char *s, int len) {
  char *new =realloc(ab->b, ab-> len + len);

  if (new == NULL) return;
  /* memcpy copia a tring s apos o fim do buffer, após isso fazemos o update do ponteiro e tamanho de abuf para um novo valor*/
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}
/** é um destrutor que desaloca a memória dinamica em uso por abufx  **/
void abFree(struct abuf *ab) {
  free(ab->b);
}    

/*** saida ***/
/** pega o tamano da tela e imprime a msg, no caso a msg de boas vindas & o resto com ">" **/

void editorScroll(){
  if (E.cy < E.rowoff){
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows){
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.cx < E.coloff) {
    E.coloff = E.cx;
  }
  if (E.cx >= E.coloff + E.screencols) {
    E.coloff = E.cx - E.screencols + 1;
  }
}

void editorDrawRows(struct abuf *ab) {
  int y;
  for(y = 0; y < E.screenrows; y++){
    /* We wrap our previous row-drawing code in an if statement that checks whether we are currently drawing a row that is part of the text buffer, or a row that comes after the end of the text buffer.

       To draw a row that’s part of the text buffer, we simply write out the chars field of the erow. But first, we take care to truncate the rendered line if it would go past the end of the screen.*/
    int filerow = y + E.rowoff;
    /* checa se o cursor esta visivel na janela  */
    if(filerow >= E.numrows) {
      /* documentar direito depois
	 The second if statement checks if the cursor is past the bottom of the visible window, and contains slightly more complicated arithmetic because E.rowoff refers to what’s at the top of the screen, and we have to get E.screenrows involved to talk about what’s at the bottom of the screen.
       */
    if (E.numrows == 0 && y == E.screenrows / 3){
      char welcome[80];
      /* usamos o snprintf() para interpolar a msg FOO_VERSION em mensagem de welcome  */
      int welcomeLen = snprintf(welcome, sizeof(welcome),
				"FOO editor -- version %s", FOO_VERSION);
      if (welcomeLen > E.screencols) welcomeLen = E.screencols;
      /*centra a msg, dividindo a largura da tela por fois */
      int padding = (E.screencols - welcomeLen) / 2;
      if (padding) {
	abAppend(ab, ">", 1);
	padding--;
      }
      while (padding--) abAppend(ab, " ", 1);
      abAppend(ab, welcome, welcomeLen);
    } else {
      abAppend(ab, ">", 1);
    }
    }else {
      int len = E.row[filerow].size - E.coloff;
      if ( len < 0) len = 0;
      if( len > E.screencols) len = E.screencols;
      abAppend(ab, &E.row[filerow].chars[E.coloff], len);
    }

    /*limpa a tela, uma linha por vez */
    abAppend(ab, "\x1b[K",3);
    if (y < E.screenrows -1){
      abAppend(ab, "\r\n", 2);
    }
  }
}

/** não entendi direito??
    >In editorRefreshScreen(), we first initialize a new abuf called ab, by assigning ABUF_INIT to it. We then replace each occurrence of write(STDOUT_FILENO, ...) with abAppend(&ab, ...). We also pass ab into editorDrawRows(), so it too can use abAppend(). Lastly, we write() the buffer’s contents out to standard output, and free the memory used by the abuf.
  **/
void editorRefreshScreen(){
  editorScroll();
  
  struct abuf ab = ABUF_INIT;

  /* faz o cursor desparecer enquanto recarrega o terminal  
     >We use escape sequences to tell the terminal to hide and show the cursor. The h and l commands (Set Mode, Reset Mode) are used to turn on and turn off various terminal features or “modes”. The VT100 User Guide just linked to doesn’t document argument ?25 which we use above. It appears the cursor hiding/showing feature appeared in later VT models. So some terminals might not support hiding/showing the cursor, but if they don’t, then they will just ignore those escape sequences, which isn’t a big deal in this case.

*/
  /* faz o cursor não aparecer entre os recarregamentos	 */
  abAppend(&ab, "\x1b[?25l",6);

  /* removido por abAppend(ab, "\xx1b[K",3); na func editorDrawRows()*/
  /* abAppend(&ab, "\x1b[2J", 4);*/
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH",(E.cy - E.rowoff) +1,
	                                   (E.cx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));
  
  abAppend(&ab, "\x1b[H", 3);
  abAppend(&ab, "\x1b[?25h", 6);
  
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
  
}

/*** entrada***/
/** processa o apertar das teclas, <caso, se 'q'> fecha a aplicação> **/

void editorMoveCursor(int key) {
  switch(key){
    case ARROW_LEFT:
      if ( E.cx != 0){
	E.cx--;
      }
      break;
    case ARROW_RIGHT:
	E.cx++;
      break;
    case ARROW_UP:
      if( E.cy != 0){
	E.cy--;
      }
      break;
    case ARROW_DOWN:
      if ( E.cy < E.numrows) {
	E.cy++;
      }
      break;
  }
}

void editorProcessKeypress(){
  int c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case HOME_KEY:
      E.cx = 0;
      break;

    case END_KEY:
      E.cx = E.screencols -1;
      break;

      /* simula o apertar de <seta pra cima> e <seta pra baixo> */
    case PAGE_UP:
    case PAGE_DOWN:
      {
	int times = E.screenrows;
	while (times--)
	  editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;
  }
}

/*** init ***/
/** caso de felha ao iniciar manda "getWindowSize" para die(), que passa o argumento a perror()
    e.cx é a coordenada horizontal e cy a vertical, começamos elas em 0,0 ; que é o comeo do terminal
 **/
void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(int argc, char *argv[]){
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    /* recebe o nome do arquivo
 
       foda-se depois faço
      
♋︎ may or may not compile

FILE, fopen(), and getline() come from <stdio.h>.

The core of editorOpen() is the same, we just get the line and linelen values from getline() now, instead of hardcoded values.

editorOpen() now takes a filename and opens the file for reading using fopen(). We allow the user to choose a file to open by checking if they passed a filename as a command line argument. If they did, we call editorOpen() and pass it the filename. If they ran ./kilo with no arguments, editorOpen() will not be called and they’ll start with a blank file.

getline() is useful for reading lines from a file when we don’t know how much memory to allocate for each line. It takes care of memory management for you. First, we pass it a null line pointer and a linecap (line capacity) of 0. That makes it allocate new memory for the next line it reads, and set line to point to the memory, and set linecap to let you know how much memory it allocated. Its return value is the length of the line it read, or -1 if it’s at the end of the file and there are no more lines to read. Later, when we have editorOpen() read multiple lines of a file, we will be able to feed the new line and linecap values back into getline() over and over, and it will try and reuse the memory that line points to as long as the linecap is big enough to fit the next line it reads. For now, we just copy the one line it reads into E.row.chars, and then free() the line that getline() allocated.

We also strip off the newline or carriage return at the end of the line before copying it into our erow. We know each erow represents one line of text, so there’s no use storing a newline character at the end of each one.

If your compiler complains about getline(), you may need to define a feature test macro. Even if it compiles fine on your machine without them, let’s add them to make our code more portable.
  */
    editorOpen(argv[1]);
  }
  
  while(1){
    editorRefreshScreen();
    editorProcessKeypress();
  }
  
  return 0;
}
