all:
	gcc -o capture_frame main.c
	./capture_frame
	cat frame*.raw > video.raw
	rm frame*.raw
	yes | ffmpeg -framerate 24 -f rawvideo -pix_fmt yuyv422 -s 640x480 -i video.raw -c:v libx264 -pix_fmt yuv420p out.mp4
	xdg-open out.mp4
