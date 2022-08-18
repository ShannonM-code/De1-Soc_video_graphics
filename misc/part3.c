#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define video_BYTES 8 // number of characters to read from /dev/video

int screen_x, screen_y;

int main(int argc, char *argv[]){
    int video_FD; // file descriptor
    int y, vertical_flag, count, x1, x2;
    char buffer[video_BYTES]; // buffer for data read from /dev/video
    char command[64]; // buffer for commands written to /dev/video

    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1)
    {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }

    // Set screen_x and screen_y by reading from the driver
    while(read(video_FD, buffer, video_BYTES));
    sscanf(buffer,"%d %d", &screen_x, &screen_y);

    //sprintf(buffer, "cl");
    write(video_FD, buffer, video_BYTES);
    sprintf(buffer, "cl");

    // Variables for moving the line on the screen
    y = 1;
    vertical_flag = -1;
    count = 0;
    x1 = 0;
    x2 = screen_x - 1;

    // Draw a line
    sprintf (command, "line %d,%d %d,%d %X\n", x1, y, x2, y, 0xFFE0);
    write (video_FD, command, strlen(command));

    while(count < 1000){

        // Sync with the VGA controller
        sprintf (command, "sync\n");
        write (video_FD, command, strlen(command));

        // Sync with the VGA controller
        sprintf (command, "cl\n");
        write (video_FD, command, strlen(command));

        // Change the direction of the line based on the flag
        //
        if(y == (screen_y - 1) && vertical_flag == -1){
            vertical_flag = 1;
        }
        else if(y == 0 && vertical_flag == 1){
            vertical_flag = -1;
        }

        if(vertical_flag == -1)
            y++;
        else if(vertical_flag == 1)
            y--;

        if(y == 1)
            count++;

        count++;

        // Draw a line
        sprintf (command, "line %d,%d %d,%d %X\n", x1, y, x2, y, 0xFFE0);
        write (video_FD, command, strlen(command));

    }

    // Clear the screen
    sprintf (command, "cl");
    write (video_FD, command, strlen(command));

    close (video_FD);
    return 0;
}


