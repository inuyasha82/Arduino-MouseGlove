#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <malloc.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <string.h>

#include "pointermouse.h"

int fd; 

int curposx;
int curposy;
int startx;
int starty;
int delta;
struct termios toptions;
static int _XlibErrorHandler(Display *display, XErrorEvent *event) {
    fprintf(stderr, "An error occured detecting the mouse position\n");
    return True;
}

void setupSerial(char *port){ 
 printf("Port opened: %s\n", port);
 fd = open(port, O_RDWR | O_NOCTTY); 
 fprintf(stdout, "fd opened as %i\n", fd);
  /* wait for the Arduino to reboot */
 usleep(3500000);
  /* get current serial port settings */
 tcgetattr(fd, &toptions);
 /* set 9600 baud both ways */
 cfsetispeed(&toptions, B57600);
 cfsetospeed(&toptions, B57600);
 /* 8 bits, no parity, no stop bits */
 toptions.c_cflag &= ~PARENB;
 toptions.c_cflag &= ~CSTOPB;
 toptions.c_cflag &= ~CSIZE;
 toptions.c_cflag |= CS8;
 /* Canonical mode */
 toptions.c_lflag |= ICANON;
 /* commit the serial port settings */
 tcsetattr(fd, TCSANOW, &toptions);
}

int main(int argc, char *argv[]) {
    int number_of_screens;
    int i;
    Bool result;
    Window *root_windows;
    Window window_returned;
    int root_x, root_y;
    int win_x, win_y;
    unsigned int mask_return;
    int sumx;
    int sumy;    
    Display *display = XOpenDisplay(NULL);    
    char *com_port = COM_PORT;
    sumx=0;
    sumy=0;
    delta=DELTA_VALUE;
    int option_index;    
    int opt_result;
    while((opt_result=getopt_long(argc,argv,"hvd:t:s:", long_options, &option_index))!=-1){
      switch(opt_result){
	case 'v':
	  printf("%s - Version %s\n", argv[0], VERSION);
	  return 0;
	case 'd':
	  printf("Val: %s\n", optarg);
	  com_port = malloc(strlen(optarg));
	  strcpy(com_port,optarg);
	  printf("%s\n",com_port);	  
	break;
      }      
    }
    setupSerial(com_port);
    assert(display);
    XSetErrorHandler(_XlibErrorHandler);    
    number_of_screens = XScreenCount(display);
    fprintf(stderr, "There are %d screens available in this X session\n", number_of_screens);    
    int width = XDisplayWidth(display,0);
    int height = XDisplayHeight(display,0);
    fprintf(stdout, "Size: %dx%d Number of screens: %d\n", width, height, number_of_screens);
    root_windows = malloc(sizeof(Window) * number_of_screens);
    int x, y;
    for (i = 0; i < number_of_screens; i++) {
	root_windows[i] = XRootWindow(display, i);
	XSelectInput(display, root_windows[i], KeyReleaseMask);
	int j=0;
	
	while(j<NUM_READ){	  
	  int status = readSerial(fd, &x, &y);
	  if(status ==-1) continue;
	  printf("Status: %d, x: %d, y: %d\n", status, x,y);
	  sumx +=x;
	  sumy +=y;
	  XWarpPointer(display, None, root_windows[i], 0, 0, 0, 0, 100+(j*5), 100+(j*5));
	  XFlush(display);	  
	  sleep(1);
	  j++;
	}
	curposx = sumx/NUM_READ;
	curposy = sumy/NUM_READ;
    }
    startx = curposx;
    starty = curposy;
    //Loop di controllo
    printf("Starting from: x=%d y=%d\n", x,y);
    while(1){
      int res = readSerial(fd, &x, &y);
      if(res==-1) {
	  printf("No data...\n");
	  continue;
      }
      for (i = 0; i < number_of_screens; i++) {      
	 result = XQueryPointer(display, root_windows[i], &window_returned,
		&window_returned, &root_x, &root_y, &win_x, &win_y,
		&mask_return);
	 if (result == True) {
	    break;
	  }
      }
      if (result != True) {
	fprintf(stderr, "No mouse found.\n");
	return -1;
      }
    //printf("Mouse is at (%d,%d) Screen size: %dx%d\n", root_x, root_y,width,height);
    printf("x: %d, y: %d\n", x,y);
    moveMouse(x,y, display,root_windows[0], root_x, root_y);
      if(root_x == width-1 || root_y == height-1 || root_x == 0 || root_y == 0){
	write(fd, "W", 1);
      }
    }    
    free(root_windows);
    XCloseDisplay(display);
    return 0;
}

/**
 * Function that move mouse given accellerometer reads.
 * @param fd file descriptor
 * @param &x pointer to x container variable
 * @param &y pointer to y container variable
 */
int readSerial(int fd, int* x, int* y){
  if(fd==-1) {
    //fprintf(stderr, "Device not opened");
    return -1;
  }
    else {
      char buffer[32] = "";
      int n = read(fd, buffer, sizeof(buffer));
      if (n <= 0){
	 fputs("read failed!\n", stderr);
	 return -1;
      }
      else {	
	char buffer2[10];
	sscanf(buffer, "%c=%d%c=%d\n", buffer2,y,buffer2,x);	
	//printf("String: %s - x: %d, y: %d\n", buffer, *x,*y);
      }
      return (fd);
    }
    return 0;
}

/**
 * Function that move mouse given accellerometer reads.
 * @param x current x read
 * @param y current y read
 * @param display current display handler
 * @param root_window The root window 
 * @param posx current x pointer position  
 * @param posy current y pointer position  
 */
void moveMouse(int x, int y, Display *display, Window root_window, int posx, int posy){
  int movementx = x - startx;
  int movementy = y - starty;  
  if(abs(movementx)>delta){
    XWarpPointer(display, None, root_window, 0, 0, 0, 0, posx-(movementx/SPEED_FACTOR), posy);
    curposx = x;
  }
  if(abs(movementy)>delta){    
    XWarpPointer(display, None, root_window, 0, 0, 0, 0, posx, (posy+movementy/SPEED_FACTOR));
    curposy=y;
  }
  printf("mx: %d, my: %d | sx: %d, sy: %d | cx: %d, cy: %d\n", movementx,movementy,startx, starty, curposx, curposy);
  XFlush(display);	  
}
