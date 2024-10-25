#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>

#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480
#define NUM_BUFFERS 10
#define NUM_FRAMES 60 

struct buffer {
    void   *start;
    size_t length;
};

int main() {
    const char *device = "/dev/video0";
    int fd = open(device, O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("ioctl query capabilities");
        close(fd);
        return 1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "device does not support video capture\n");
        close(fd);
        return 1;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = FRAME_WIDTH;
    fmt.fmt.pix.height = FRAME_HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("pixel format");
        close(fd);
        return 1;
    }

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = NUM_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("request buffers");
        close(fd);
        return 1;
    }

    struct buffer buffers[NUM_BUFFERS];
    for (int i = 0; i < NUM_BUFFERS; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("query buffer");
            close(fd);
            return 1;
        }

        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED) {
            perror("map buffer");
            close(fd);
            return 1;
        }
    }

    for (int i = 0; i < NUM_BUFFERS; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("queue buffers");
            close(fd);
            return 1;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("starting capture");
        close(fd);
        return 1;
    }

    for (int frame_count = 0; frame_count < NUM_FRAMES; frame_count++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        // pop from the queue
        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("Dequeuing buffer");
            break;
        }

        // process buffer (save raw frame data)
        char filename[20];
        snprintf(filename, sizeof(filename), "frame%d.raw", frame_count);
        FILE *file = fopen(filename, "wb");
        if (file) {
            fwrite(buffers[buf.index].start, buf.bytesused, 1, file);
            fclose(file);
            printf("frame %d saved to %s\n", frame_count, filename);
        } else {
            perror("could not open file for frame dump");
        }

        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("requeue buffer");
            break;
        }
    }

    // cleanup
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        perror("stopping capture");
    }

    for (int i = 0; i < NUM_BUFFERS; i++) {
        munmap(buffers[i].start, buffers[i].length);
    }

    close(fd);
    return 0;
}

