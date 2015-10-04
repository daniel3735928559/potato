#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define MAX_WIDTH 1000
#define MAX_HEIGHT 1000

typedef struct{
  int sockfd;
  Display *display;
  int window;
  GC gc;
  GC his_gc;
  GC refresh_gc;
} recv_info_t;

Pixmap back_buffer;
int q_count = 0;
int line_widths[] = {1, 2, 3, 5, 7, 10, 15};
XColor cols[10];
int line_style = LineSolid;
int cap_style = CapRound;
int join_style = JoinBevel;
int prevx = -1, prevy = -1, sending = 0, current_width = 1;
int current_color = 1;

void error(const char *msg){ perror(msg); exit(1);}

Window create_simple_window(Display* display, Atom *wm_delete, int width, int height, int x, int y){
  int screen_num = DefaultScreen(display);
  int win_border_width = 2;
  Window win;
  win = XCreateSimpleWindow(display, RootWindow(display, screen_num), x, y, width, height, win_border_width, BlackPixel(display, screen_num), WhitePixel(display, screen_num));
  *wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", 0);
  XSetWMProtocols(display, win, wm_delete, 1);
  XMapWindow(display, win);
  XFlush(display);
  return win;
}

GC create_gc(Display* display, Window win){
  GC gc;
  unsigned long valuemask = 0;
  XGCValues values;
  int screen_num = DefaultScreen(display);

  gc = XCreateGC(display, win, valuemask, &values);
  if (gc < 0) fprintf(stderr, "XCreateGC: \n");
  XSetForeground(display, gc, BlackPixel(display, screen_num));
  XSetBackground(display, gc, WhitePixel(display, screen_num));

  XSetLineAttributes(display, gc, line_widths[1], line_style, cap_style, join_style);
  XSetFillStyle(display, gc, FillSolid);

  return gc;
}

void clear_all(Display *display, Window window, GC refresh_gc){
  XClearArea(display, window, 0, 0, MAX_WIDTH, MAX_HEIGHT, 1);
  XFillRectangle(display, back_buffer, refresh_gc, 0, 0, MAX_WIDTH, MAX_HEIGHT);
}

void handle_expose(Display* display, GC gc, XExposeEvent* expose_event){
  if(expose_event->count != 0) return;
  XCopyArea(display, back_buffer, expose_event->window, gc, 0, 0, MAX_WIDTH, MAX_HEIGHT, 0, 0);
}

void send_comm(int sockfd, int prevx, int prevy, int x, int y, int comm){
  int buffer[5];
  buffer[0] = prevx;
  buffer[1] = prevy;
  buffer[2] = x;
  buffer[3] = y;
  buffer[4] = comm;
  int n = write(sockfd,buffer,5*sizeof(int));
  if(n < 0) error("ERROR writing to socket");
}

void quitme(Display *display, GC gc, GC his_gc, GC refresh_gc, int sockfd, int code){
  if(sending){
    send_comm(sockfd, 0, -1, -1, -1, -1);
    close(sockfd);
  }
  XFreePixmap(display, back_buffer);
  XFreeGC(display, gc);
  XFreeGC(display, his_gc);
  XFreeGC(display, refresh_gc);
  XCloseDisplay(display);
  exit(code);
}

void *recv_line(void *arg){
  recv_info_t *info = (recv_info_t *)arg;
  int sockfd = info->sockfd;
  Display* display = info->display;
  GC gc = info->gc;
  GC his_gc = info->his_gc;
  GC refresh_gc = info->refresh_gc;
  int window = info->window;
  int i = 0;
  while(sending){
    int data[5];
    int n = read(sockfd,data,5*sizeof(int)); if(n < 0) error("ERROR reading from socket");
    if(data[4] == 0){
      if(is_OK2(data[2], data[3])){
	XDrawLine(display, window, his_gc, data[0], data[1], data[2], data[3]);
	XDrawLine(display, back_buffer, his_gc, data[0], data[1], data[2], data[3]);
      }
    }
    else if(data[4] == 1) XSetLineAttributes(display, his_gc, data[0], line_style, cap_style, join_style);
    else if(data[4] == 2) XSetForeground(display, his_gc, data[0]);
    else if(data[4] == 3) clear_all(display, window, refresh_gc);
    else if(data[4] == -1) sending = 0; //Our friend died
    XFlush(display);
  }
}

int is_OK2(x, y){ return (x >= 0  && x <= MAX_WIDTH && y >= 0 && y <= MAX_HEIGHT);}

int is_OK(x, y){ return (x >= 0  && x <= MAX_WIDTH && y >= 0 && y <= MAX_HEIGHT);}

void handle_button_down(Display* display, GC gc, XButtonEvent* button_event, unsigned int win_width, unsigned int win_height){
  int x = button_event->x;
  int y = button_event->y;
  if(is_OK(x, y)){ prevx = x; prevy = y; }
}

void handle_drag(Display* display, GC gc, XButtonEvent* drag_event, unsigned int win_width, unsigned int win_height, int sockfd){
  int x, y;
  x = drag_event->x;
  y = drag_event->y;
  if(is_OK(x, y)){
    XDrawLine(display, drag_event->window, gc, prevx, prevy, x, y);
    XDrawLine(display, back_buffer, gc, prevx, prevy, x, y);
    if(sending) send_comm(sockfd, prevx, prevy, x, y, 0);
    prevx = x; prevy = y;
  }
  XFlush(display);
}

void set_title(Display* display, Window win){
  int i, N = 50;
  char buffer[N];
  for(i = 0; i < N; i++) buffer[i] = 0;
  int len = sprintf(buffer, "%d, %04X %04X %04X", current_width, (unsigned int)(cols[current_color].red), (unsigned int)(cols[current_color].green), (unsigned int)(cols[current_color].blue));
  buffer[len] = 0;
  XStoreName(display, win, buffer);
}

void set_width(Display* display, GC gc, int w, int sockfd){
  XSetLineAttributes(display, gc, w, line_style, cap_style, join_style);
  if(sending) send_comm(sockfd, w, -1, -1, -1, 1);
}

void set_color(Display* display, GC gc, long c, int sockfd){
  XSetForeground(display, gc, c);
  if(sending) send_comm(sockfd, (int)c, -1, -1, -1, 2);
}

void handle_key(Display* display, Window win, GC gc, GC his_gc, GC refresh_gc, Colormap map, XKeyEvent* key_event, int sockfd){
  char c = XLookupKeysym(key_event, 0);
  int w = -1;
  if(c == 'q'){
    q_count++;
    if(q_count >= 3){
      quitme(display, gc, his_gc, refresh_gc, sockfd, 0);
    }
  }
  else{
    q_count = 0;
    if(c == 'l'){
      clear_all(display, key_event->window, refresh_gc);
      if(sending) send_comm(sockfd, 0, 0, 0, 0, 3);
    }
    else if('0' <= c && c <= '9'){  current_color = c -'0'; set_color(display, gc, cols[c - '0'].pixel, sockfd); set_title(display, win);}
    else if(c == 'z' || c == 'x' || c == 'c' || c == 'v' || c == 'b' || c == 'n' || c == 'm'){
      switch(c){
      case 'm': w++;
      case 'n': w++;
      case 'b': w++;
      case 'v': w++;
      case 'c': w++;
      case 'x': w++;
      case 'z': w++; break;
      }
      if(0 <= w){ current_width = line_widths[w]; set_width(display, gc, line_widths[w], sockfd); set_title(display, win);}
    }
  }
}

int main(int argc, char* argv[]){
  int sockfd, oldsockfd, portno, n;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  struct hostent *server;
  pthread_t recv_thread;
  Colormap map;

  Display* display;
  int screen_num;
  Window win;
  unsigned int display_width, display_height;
  unsigned int width, height;
  char *display_name = getenv("DISPLAY");
  GC gc, his_gc, refresh_gc;

  if(argc == 2){ // We are server
    sending = 1;
    if(argc < 2) { fprintf(stderr,"ERROR, no port provided\n"); exit(1); }
    oldsockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(oldsockfd < 0) error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    int on = 1;
    if(setsockopt(oldsockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0) error("ERROR on sockopt");
    if(bind(oldsockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");
    listen(oldsockfd,5);
    clilen = sizeof(cli_addr);
    sockfd = accept(oldsockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (sockfd < 0) error("ERROR on accept");
  }
  else if(argc == 3){ // We are client
    sending = 1;
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0); if (sockfd < 0) error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if(server == NULL){ fprintf(stderr,"ERROR, no such host\n"); exit(0); }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)(server->h_addr), (char *)(&serv_addr.sin_addr.s_addr), server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) error("ERROR connecting");
  }
  else{
    server = 0;
  }
  
  display = XOpenDisplay(display_name);
  if (display == NULL) {
    fprintf(stderr, "%s: cannot connect to X server '%s'\n", argv[0], display_name);
    exit(1);
  }

  screen_num = DefaultScreen(display);
  display_width = DisplayWidth(display, screen_num);
  display_height = DisplayHeight(display, screen_num);
  width = (display_width / 3);
  height = (display_height / 3);
  Atom wm_delete;
  win = create_simple_window(display, &wm_delete, width, height, 0, 0);


  XGCValues values;
  values.graphics_exposures = 0;
  gc = create_gc(display, win);
  his_gc = create_gc(display, win);
  refresh_gc = XCreateGC(display, win, GCGraphicsExposures, &values);

  XSetForeground(display, refresh_gc, WhitePixel(display, screen_num));
  XSetBackground(display, refresh_gc, WhitePixel(display, screen_num));

  map = DefaultColormap(display, 0);
  XParseColor(display, map, "#FFFFFF", &cols[0]);
  XAllocColor(display, map, &cols[0]);
  XParseColor(display, map, "#000000", &cols[1]);
  XAllocColor(display, map, &cols[1]);
  XParseColor(display, map, "#FF0000", &cols[2]);
  XAllocColor(display, map, &cols[2]);
  XParseColor(display, map, "#00FF00", &cols[3]);
  XAllocColor(display, map, &cols[3]);
  XParseColor(display, map, "#0000FF", &cols[4]);
  XAllocColor(display, map, &cols[4]);
  XParseColor(display, map, "#FFFF00", &cols[5]);
  XAllocColor(display, map, &cols[5]);
  XParseColor(display, map, "#FF00FF", &cols[6]);
  XAllocColor(display, map, &cols[6]);
  XParseColor(display, map, "#00FFFF", &cols[7]);
  XAllocColor(display, map, &cols[7]);
  XParseColor(display, map, "#999999", &cols[8]);
  XAllocColor(display, map, &cols[8]);
  XParseColor(display, map, "#666666", &cols[9]);
  XAllocColor(display, map, &cols[9]);
  
  set_title(display, win);
  
  back_buffer = XCreatePixmap(display, win, MAX_WIDTH, MAX_HEIGHT, DefaultDepth(display, screen_num));
  XFillRectangle(display, back_buffer, refresh_gc, 0, 0, MAX_WIDTH, MAX_HEIGHT);

  if(sending){
    recv_info_t info;
    info.display = display;
    info.window = win;
    info.gc = gc;
    info.his_gc = his_gc;
    info.refresh_gc = refresh_gc;
    info.sockfd = sockfd;
    pthread_create(&recv_thread, NULL, recv_line, &info);
  }

  XSelectInput(display, win, ExposureMask | KeyPressMask | ButtonPressMask | Button1MotionMask | StructureNotifyMask);
  XEvent an_event;
  while(1) {
    XNextEvent(display, &an_event);
    switch(an_event.type){
    case ClientMessage: if(an_event.xclient.data.l[0] == wm_delete) quitme(display, gc, his_gc, refresh_gc, sockfd, 0); break;
    case Expose: handle_expose(display, refresh_gc, (XExposeEvent*)&an_event.xexpose); break;
    case ConfigureNotify: width = an_event.xconfigure.width; height = an_event.xconfigure.height; break;
    case ButtonPress: handle_button_down(display, gc, (XButtonEvent*)&an_event.xbutton, width, height); break;
    case MotionNotify: handle_drag(display, gc, (XButtonEvent*)&an_event.xbutton, width, height, sockfd); break;
    case KeyPress: handle_key(display, win, gc, his_gc, refresh_gc, map, (XKeyEvent*)&an_event.xkey, sockfd); break;
    default: break;
    }
  }
  quitme(display, gc, his_gc, refresh_gc, sockfd, 0);
}
