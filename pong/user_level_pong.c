#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#define video_BYTES 10 // number of characters to read from /dev/video
#define key_BYTES 1 // number of characters to read from /dev/video
#define sw_BYTES 3 // number of characters to read from /dev/video

int screen_x, screen_y;
int x1 = 0;
int y1 = 0;
int past_y1 = 0;
int key_data;
int sw_data;
int color = 0xf800;
int color2 = 0x07e0;
int dircx[8] = {1,-1,1,-1,1,1,1,-1};
int dircy[8] = {1,1,-1,-1,-1,-1,1,-1};
int movex[8] = {1,-1,1,-1,1,1,1,-1};
int movey[8] = {1,1,-1,-1,-1,-1,1,-1};
int box_x1[8] = {5,10,15,20,25,30,40,57};
int box_y1[8] = {30,27,8,55,25,40,10,15};
int box_x2[8] = {15,20,25,30,35,40,50,67};
int box_y2[8] = {39,36,17,64,34,49,19,24};
int max = 2;//(int)( sizeof(box_x1) / sizeof(box_x1[0]));
//int x_loc[5] = {SCREEN_X-10, SCREEN_X-20, SCREEN_X-35, SCREEN_X-50, SCREEN_X-65};
//int y_loc[5] = {SCREEN_Y-10, SCREEN_Y-20, SCREEN_Y-30, SCREEN_Y-40, SCREEN_Y-55};
int delay = 10000;

void increment_lines(int *x_0, int *y_0, int *x_1, int *y_1, int *xmove, int *ymove, int *dirx, int *diry, int color, int video_FD, char buffer[max]);

int main(int argc, char *argv[]){
	int video_FD, // video file descriptor
        sw_FD,    // switch file descriptor
        key_FD;   // key file descriptor
	int col_inc = 0xFF;
	char buffer[video_BYTES]; // buffer for data read from /dev/video
	char sw_buffer[sw_BYTES];
	char key_buffer[key_BYTES];
	int incr = 0;

	// Open the character device driver
	if ((video_FD = open("/dev/video", O_RDWR)) == -1)
		printf("Error opening /dev/video: %s\n", strerror(errno));
	if ((sw_FD = open("/dev/SW", O_RDONLY)) == -1)
		printf("Error opening /dev/SW: %s\n", strerror(errno));
  	if ((key_FD = open("/dev/KEY", O_RDONLY)) == -1)
		printf("Error opening /dev/KEY: %s\n", strerror(errno));
    if ((video_FD == -1) || (sw_FD == -1) || (key_FD == -1))
		return -1;

	// Set screen_x and screen_y by reading from the driver
	//...code not shown
    while (read (video_FD, buffer, video_BYTES));	// read the VIDEO port until EOF
    sscanf(buffer, "%d %d", &screen_x, &screen_y);
    printf("\tx = %d\n\ty = %d\n\n", screen_x, screen_y);
//    screen_x = screen_x / 3.5;
//    screen_y = screen_y / 2.5;

    printf("To make it work on my monitor I must\ndivide screen_x and screen_y by 4.\n\n");
    printf("\tnew x = %d\n\tnew y = %d\n\n\n", screen_x, screen_y);
    printf("key_FD = %X\n",key_FD);
//    printf("*key_FD = %X",*key_FD);
    printf("&key_FD = %X\n",&key_FD);
	// Use pixel commands to color some pixels on the screen
	//...code not shown
    while (1)
    {
        while (read (key_FD, key_buffer, key_BYTES));	// read the KEY port until EOF
		sscanf (key_buffer, "%X", &key_data);
		while (read (sw_FD, sw_buffer, sw_BYTES));	// read the KEY port until EOF
		sscanf (sw_buffer, "%X", &sw_data);
        /* box_x1 is the x-coordinate for top left corner of boxes
         * box_y1 is the y-coordinate for top left corner of boxes
         * box_x2 is the x-coordinate for bottom right corner of boxes
         * box_y2 is the y-coordinate for bottom right corner of boxes
         */
        while(incr < max) {
            box_x1[incr] = box_x1[incr] + dircx[incr];
            box_y1[incr] = box_y1[incr] + dircy[incr];
            box_x2[incr] = box_x2[incr] + dircx[incr];
            box_y2[incr] = box_y2[incr] + dircy[incr];
            incr++;
        }
        incr = 0;
        sprintf(buffer, "cl");
        write(video_FD, buffer, strlen(buffer));
//        sprintf(buffer, "sync");
//        write(video_FD, buffer, strlen(buffer));

        while(incr < max) {
            sprintf(buffer, "box %d,%d %d,%d %X", box_x1[incr], box_y1[incr], box_x2[incr], box_y2[incr], color);
            write(video_FD, buffer, strlen(buffer));
//            sprintf(buffer, "sync");
//            write(video_FD, buffer, strlen(buffer));

//            printf("incr = %d\nbox_x1 = %d   box_y1 = %d\nbox_x2 = %d   box_y2 = %d\n\n\n", incr, box_x1[incr], box_y1[incr], box_x2[incr], box_y2[incr]);
            increment_lines(&box_x1[incr], &box_y1[incr], &box_x2[incr], &box_y2[incr], &movex[incr], &movey[incr], &dircx[incr], &dircy[incr], color, video_FD, &buffer[video_BYTES]);
            if (sw_data)
            {
                if (incr == (max - 1)) {
                    sprintf(buffer, "line %d,%d %d,%d %X", box_x1[incr], box_y1[incr], box_x2[0], box_y2[0], color2);
                    write(video_FD, buffer, strlen(buffer));
//                    sprintf(buffer, "sync");
//                    write(video_FD, buffer, strlen(buffer));
                }
                else {
                    sprintf(buffer, "line %d,%d %d,%d %X", box_x1[incr], box_y1[incr], box_x2[incr+1], box_y2[incr+1], color2);
                    write(video_FD, buffer, strlen(buffer));
//                    sprintf(buffer, "sync");
//                    write(video_FD, buffer, strlen(buffer));
                }
            }
            incr++;
        }
        if (key_data & 0x1) {
            delay = delay + 1000;
        }
        if (key_data & 0x2) {
            delay = delay - 1000;
        }
        if (key_data & 0x8) {
            if (max < 8 && max >= 0)
                max = max + 1;
            else
                max = 8;
        }
        if (key_data & 0x4) {
            if (max <= 8 && max > 1)
                max = max - 1;
            else
                max = 1;
        }
        incr = 0;
        usleep(delay);

    }
    printf("closing!!!");
	close (video_FD);
	return 0;
}

void increment_lines(int *x_0, int *y_0, int *x_1, int *y_1, int *xmove, int *ymove, int *dirx, int *diry, int color, int video_FD, char buffer[max])
{
    int temp;
    int val;
    do {
        val = -3 + rand() % (3 - (-3) + 1);
    } while(val == 0);
    //printf("val = %d\n", val);
    if (*y_0 > *y_1)
    {
        temp = *y_0;
        *y_0 = *y_1;
        *y_1 = temp;
    }
    srand((unsigned)time(NULL));
    //printf("x1 = %d  y1 = %d  x2 = %d  y2 = %d\n", *x_0, *y_0, *x_1, *y_1);
    if (*x_1 > screen_x - 2){
        *x_1 = screen_x - 1;
        *x_0 = screen_x - 10;
        *dirx = val * -(*xmove);
        *xmove = -(*xmove);
    }
    if (*y_1 > screen_y - 2){
        *y_1 = screen_y - 1;
        *y_0 = screen_y - 9;
        *diry = val * -(*ymove);
        *ymove = -(*ymove);
    }
    if (*x_0 < 1) {//true if line is going down
        *x_0 = 1;
        *x_1 = 10;
        *dirx = val * -(*xmove);
        *xmove = -(*xmove);
    }
    if (*y_0 < 1) {
        *y_0 = 1;
        *y_1 = 9;
        *diry = val * -(*ymove);
        *ymove = -(*ymove);
    }
//    printf("dircx = %d   dircy = %d", dirx, di)
}




